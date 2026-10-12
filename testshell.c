#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define INPUT_BUFFER_SIZE 256U
#define MAX_TOKENS 4U
#define SSD_LBA_COUNT 100U
#define RESULT_FILE_PATH "result.txt"
#define NAND_FILE_PATH "nand.txt"
#define MAX_TEST_RESULTS 32U

#define SHELL_EXIT_OK 0
#define SHELL_EXIT_TEST_FAILURE 1
#define SHELL_EXIT_USAGE 2

typedef struct {
    bool exited;
    int exit_code;
    int signal_number;
} ProcessResult;

typedef enum {
    INPUT_OK,
    INPUT_EOF,
    INPUT_TOO_LONG,
    INPUT_ERROR
} InputStatus;

typedef enum {
    TEST_PASS,
    TEST_FAIL,
    TEST_ERROR
} TestStatus;

typedef struct {
    const char *ssd_path;
    uint32_t expected[SSD_LBA_COUNT];
    char failure_reason[128];
    char reproduction[128];
    uint32_t expected_value;
    uint32_t actual_value;
    bool has_comparison;
    bool environment_error;
} TestContext;

typedef struct {
    const char *id;
    const char *requirement_id;
    const char *name;
    TestStatus status;
    double duration_ms;
    char failure_reason[128];
    char reproduction[128];
    uint32_t expected_value;
    uint32_t actual_value;
    bool has_comparison;
} TestResult;

typedef bool (*TestFunction)(TestContext *context);

typedef struct {
    const char *id;
    const char *requirement_id;
    const char *name;
    TestFunction function;
} TestCase;

typedef struct {
    TestResult results[MAX_TEST_RESULTS];
    size_t count;
    size_t passed;
    size_t failed;
    size_t errors;
    double duration_ms;
} TestSummary;

static bool suppress_child_output;
static TestSummary last_summary;

static InputStatus read_user_input(char buffer[INPUT_BUFFER_SIZE])
{
    int next_character;

    if (fgets(buffer, (int)INPUT_BUFFER_SIZE, stdin) == NULL) {
        return feof(stdin) != 0 ? INPUT_EOF : INPUT_ERROR;
    }

    if (strchr(buffer, '\n') != NULL) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        return INPUT_OK;
    }

    next_character = fgetc(stdin);
    if (next_character == EOF) {
        return INPUT_OK;
    }
    while (next_character != '\n' && next_character != EOF) {
        next_character = fgetc(stdin);
    }
    return INPUT_TOO_LONG;
}

static size_t tokenize_command(char *line, char *tokens[MAX_TOKENS],
                               bool *has_extra_token)
{
    char *save_pointer = NULL;
    char *token;
    size_t count = 0U;

    *has_extra_token = false;
    token = strtok_r(line, " \t\r\n", &save_pointer);
    while (token != NULL) {
        if (count >= MAX_TOKENS) {
            *has_extra_token = true;
            break;
        }
        tokens[count] = token;
        ++count;
        token = strtok_r(NULL, " \t\r\n", &save_pointer);
    }
    return count;
}

static bool execute_ssd_process(const char *ssd_path, char *const arguments[],
                                ProcessResult *result)
{
    pid_t child;
    pid_t waited;
    int wait_status;

    if (ssd_path == NULL || arguments == NULL || result == NULL) {
        return false;
    }

    child = fork();
    if (child < 0) {
        (void)fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return false;
    }
    if (child == 0) {
        if (suppress_child_output) {
            int null_descriptor = open("/dev/null", O_WRONLY);
            if (null_descriptor >= 0) {
                (void)dup2(null_descriptor, STDOUT_FILENO);
                (void)dup2(null_descriptor, STDERR_FILENO);
                (void)close(null_descriptor);
            }
        }
        execv(ssd_path, arguments);
        (void)fprintf(stderr, "execv failed for %s: %s\n",
                      ssd_path, strerror(errno));
        _exit(127);
    }

    do {
        waited = waitpid(child, &wait_status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited < 0) {
        (void)fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        return false;
    }

    result->exited = WIFEXITED(wait_status) != 0;
    result->exit_code = result->exited ? WEXITSTATUS(wait_status) : -1;
    result->signal_number = WIFSIGNALED(wait_status)
                          ? WTERMSIG(wait_status)
                          : 0;
    return true;
}

static bool execute_ssd_write(const char *ssd_path, const char *lba,
                              const char *value)
{
    char *arguments[5];
    ProcessResult result;

    arguments[0] = (char *)ssd_path;
    arguments[1] = (char *)"W";
    arguments[2] = (char *)lba;
    arguments[3] = (char *)value;
    arguments[4] = NULL;

    if (!execute_ssd_process(ssd_path, arguments, &result)) {
        (void)puts("INVALID COMMAND");
        return false;
    }
    if (!result.exited || result.exit_code == 127) {
        if (result.signal_number != 0) {
            (void)fprintf(stderr, "ssd terminated by signal %d\n",
                          result.signal_number);
        }
        (void)puts("INVALID COMMAND");
        return false;
    }
    return result.exit_code == 0;
}

static bool execute_ssd_read(const char *ssd_path, const char *lba)
{
    char *arguments[4];
    ProcessResult result;

    arguments[0] = (char *)ssd_path;
    arguments[1] = (char *)"R";
    arguments[2] = (char *)lba;
    arguments[3] = NULL;

    if (!execute_ssd_process(ssd_path, arguments, &result)) {
        (void)puts("INVALID COMMAND");
        return false;
    }
    if (!result.exited || result.exit_code == 127) {
        if (result.signal_number != 0) {
            (void)fprintf(stderr, "ssd terminated by signal %d\n",
                          result.signal_number);
        }
        (void)puts("INVALID COMMAND");
        return false;
    }
    return result.exit_code == 0;
}

static bool read_ssd_result(char value_out[11])
{
    char buffer[32];
    FILE *file = fopen(RESULT_FILE_PATH, "rb");
    size_t length;
    size_t index;
    int trailing_character;

    if (file == NULL) {
        (void)fprintf(stderr, "cannot read %s: %s\n",
                      RESULT_FILE_PATH, strerror(errno));
        return false;
    }
    if (fgets(buffer, (int)sizeof(buffer), file) == NULL) {
        (void)fclose(file);
        return false;
    }
    trailing_character = fgetc(file);
    if (trailing_character != EOF || ferror(file) != 0) {
        (void)fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        return false;
    }

    length = strlen(buffer);
    if (length != 11U || buffer[10] != '\n'
        || buffer[0] != '0' || buffer[1] != 'x') {
        return false;
    }
    buffer[10] = '\0';
    for (index = 2U; index < 10U; ++index) {
        if (!((buffer[index] >= '0' && buffer[index] <= '9')
              || (buffer[index] >= 'A' && buffer[index] <= 'F'))) {
            return false;
        }
    }
    memcpy(value_out, buffer, 11U);
    return true;
}

static bool parse_result_value(uint32_t *value_out)
{
    char text[11];
    uint32_t value = UINT32_C(0);
    size_t index;

    if (value_out == NULL || !read_ssd_result(text)) {
        return false;
    }
    for (index = 2U; index < 10U; ++index) {
        uint32_t digit;
        if (text[index] >= '0' && text[index] <= '9') {
            digit = (uint32_t)(text[index] - '0');
        } else {
            digit = (uint32_t)(text[index] - 'A') + UINT32_C(10);
        }
        value = (value << 4U) | digit;
    }
    *value_out = value;
    return true;
}

static void format_lba(size_t lba, char text[4])
{
    (void)snprintf(text, 4U, "%zu", lba);
}

static bool command_write(const char *ssd_path, const char *lba,
                          const char *value)
{
    return execute_ssd_write(ssd_path, lba, value);
}

static bool command_read(const char *ssd_path, const char *lba)
{
    char value[11];

    if (!execute_ssd_read(ssd_path, lba)) {
        return false;
    }
    if (!read_ssd_result(value)) {
        (void)puts("INVALID COMMAND");
        return false;
    }
    (void)puts(value);
    return true;
}

static bool command_fullwrite(const char *ssd_path, const char *value)
{
    size_t lba;
    char lba_text[4];

    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        format_lba(lba, lba_text);
        if (!execute_ssd_write(ssd_path, lba_text, value)) {
            return false;
        }
    }
    return true;
}

static bool command_fullread(const char *ssd_path)
{
    size_t lba;
    char lba_text[4];
    char value[11];

    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        format_lba(lba, lba_text);
        if (!execute_ssd_read(ssd_path, lba_text)) {
            return false;
        }
        if (!read_ssd_result(value)) {
            (void)puts("INVALID COMMAND");
            return false;
        }
        (void)printf("LBA %02zu: %s\n", lba, value);
    }
    return true;
}

static void initialize_reference_model(TestContext *context)
{
    memset(context->expected, 0, sizeof(context->expected));
}

static void reference_write(TestContext *context, size_t lba, uint32_t value)
{
    if (lba < SSD_LBA_COUNT) {
        context->expected[lba] = value;
    }
}

static uint32_t reference_read(const TestContext *context, size_t lba)
{
    return lba < SSD_LBA_COUNT ? context->expected[lba] : UINT32_C(0);
}

static void set_test_failure(TestContext *context, const char *reason,
                             const char *reproduction, bool environment_error)
{
    (void)snprintf(context->failure_reason,
                   sizeof(context->failure_reason), "%s", reason);
    (void)snprintf(context->reproduction, sizeof(context->reproduction),
                   "%s", reproduction);
    context->environment_error = environment_error;
}

static bool oracle_write(TestContext *context, size_t lba, uint32_t value)
{
    char lba_text[4];
    char value_text[11];
    char reproduction[128];

    format_lba(lba, lba_text);
    (void)snprintf(value_text, sizeof(value_text), "0x%08" PRIX32, value);
    (void)snprintf(reproduction, sizeof(reproduction), "%s W %s %s",
                   context->ssd_path, lba_text, value_text);
    if (!execute_ssd_write(context->ssd_path, lba_text, value_text)) {
        set_test_failure(context, "SSD_WRITE_FAILED", reproduction, true);
        return false;
    }
    reference_write(context, lba, value);
    return true;
}

static bool oracle_verify_read(TestContext *context, size_t lba)
{
    char lba_text[4];
    char reproduction[128];
    uint32_t expected;
    uint32_t actual;

    format_lba(lba, lba_text);
    (void)snprintf(reproduction, sizeof(reproduction), "%s R %s",
                   context->ssd_path, lba_text);
    if (!execute_ssd_read(context->ssd_path, lba_text)
        || !parse_result_value(&actual)) {
        set_test_failure(context, "SSD_READ_FAILED", reproduction, true);
        return false;
    }

    expected = reference_read(context, lba);
    context->expected_value = expected;
    context->actual_value = actual;
    context->has_comparison = true;
    if (expected != actual) {
        set_test_failure(context, "DATA_MISMATCH", reproduction, false);
        return false;
    }
    return true;
}

static bool test_write_then_read(TestContext *context)
{
    return oracle_write(context, 3U, UINT32_C(0x12345678))
        && oracle_verify_read(context, 3U);
}

static bool test_unwritten_read(TestContext *context)
{
    return oracle_verify_read(context, 42U);
}

static bool test_overwrite(TestContext *context)
{
    return oracle_write(context, 7U, UINT32_C(0xAAAABBBB))
        && oracle_write(context, 7U, UINT32_C(0x12345678))
        && oracle_verify_read(context, 7U);
}

static bool test_distinct_lbas(TestContext *context)
{
    return oracle_write(context, 10U, UINT32_C(0xAAAAAAAA))
        && oracle_write(context, 11U, UINT32_C(0x55555555))
        && oracle_verify_read(context, 10U)
        && oracle_verify_read(context, 11U);
}

static bool test_full_pattern(TestContext *context)
{
    size_t lba;

    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        if (!oracle_write(context, lba, UINT32_C(0xA5A5A5A5))) {
            return false;
        }
    }
    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        if (!oracle_verify_read(context, lba)) {
            return false;
        }
    }
    return true;
}

static bool test_boundary_zero(TestContext *context)
{
    return oracle_write(context, 0U, UINT32_C(0x00000000))
        && oracle_verify_read(context, 0U);
}

static bool test_boundary_ninety_nine(TestContext *context)
{
    return oracle_write(context, 99U, UINT32_C(0xFFFFFFFF))
        && oracle_verify_read(context, 99U);
}

static bool testapp1(TestContext *context)
{
    size_t lba;

    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        if (!oracle_write(context, lba, UINT32_C(0xA5A5A5A5))) {
            return false;
        }
    }
    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        if (!oracle_verify_read(context, lba)) {
            return false;
        }
    }

    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        uint32_t address_pattern = UINT32_C(0xA5000000) | (uint32_t)lba;
        if (!oracle_write(context, lba, address_pattern)) {
            return false;
        }
    }
    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        if (!oracle_verify_read(context, lba)) {
            return false;
        }
    }
    return true;
}

static bool testapp2(TestContext *context)
{
    size_t repetition;
    size_t lba;

    for (repetition = 0U; repetition < 5U; ++repetition) {
        for (lba = 0U; lba < 6U; ++lba) {
            if (!oracle_write(context, lba, UINT32_C(0xAAAABBBB))) {
                return false;
            }
        }
    }
    for (lba = 0U; lba < 6U; ++lba) {
        if (!oracle_write(context, lba, UINT32_C(0x12345678))) {
            return false;
        }
    }
    for (lba = 0U; lba < 6U; ++lba) {
        if (!oracle_verify_read(context, lba)) {
            return false;
        }
    }
    return true;
}

static const TestCase test_cases[] = {
    { "TC-F001", "CLI-001", "Write then Read", test_write_then_read },
    { "TC-F002", "STOR-005", "Unwritten LBA", test_unwritten_read },
    { "TC-F003", "CLI-006", "Overwrite", test_overwrite },
    { "TC-F004", "STOR-001", "Distinct LBAs", test_distinct_lbas },
    { "TC-F005", "SHELL-001", "Full pattern", test_full_pattern },
    { "TC-B001", "STOR-008", "Boundary LBA 0", test_boundary_zero },
    { "TC-B002", "STOR-008", "Boundary LBA 99", test_boundary_ninety_nine },
    { "TC-S001", "AUTO-001", "TestApp1", testapp1 },
    { "TC-S002", "AUTO-001", "TestApp2", testapp2 }
};

static const char *test_status_text(TestStatus status)
{
    switch (status) {
    case TEST_PASS:
        return "PASS";
    case TEST_FAIL:
        return "FAIL";
    case TEST_ERROR:
        return "ERROR";
    default:
        return "ERROR";
    }
}

static double elapsed_milliseconds(const struct timespec *start,
                                   const struct timespec *end)
{
    double seconds = (double)(end->tv_sec - start->tv_sec) * 1000.0;
    double nanoseconds = (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
    return seconds + nanoseconds;
}

static bool cleanup_test_directory(const char *directory)
{
    char path[256];
    const char *files[] = { NAND_FILE_PATH, RESULT_FILE_PATH };
    size_t index;

    for (index = 0U; index < sizeof(files) / sizeof(files[0]); ++index) {
        int length = snprintf(path, sizeof(path), "%s/%s", directory,
                              files[index]);
        if (length < 0 || (size_t)length >= sizeof(path)) {
            return false;
        }
        if (remove(path) != 0 && errno != ENOENT) {
            return false;
        }
    }
    return rmdir(directory) == 0;
}

static TestResult run_test_case(const TestCase *test_case,
                                const char *ssd_path)
{
    TestResult result;
    TestContext context;
    struct timespec start = { 0, 0 };
    struct timespec end = { 0, 0 };
    char directory_template[] = "/tmp/ssd-validation-XXXXXX";
    char *original_directory = getcwd(NULL, 0U);
    char *test_directory;
    bool test_passed = false;
    bool environment_ready = true;

    memset(&result, 0, sizeof(result));
    memset(&context, 0, sizeof(context));
    result.id = test_case->id;
    result.requirement_id = test_case->requirement_id;
    result.name = test_case->name;
    context.ssd_path = ssd_path;
    initialize_reference_model(&context);

    test_directory = mkdtemp(directory_template);
    if (original_directory == NULL || test_directory == NULL
        || chdir(test_directory) != 0) {
        environment_ready = false;
        set_test_failure(&context, "TEST_ENVIRONMENT_SETUP_FAILED",
                         "create isolated test directory", true);
    }

    if (environment_ready) {
        suppress_child_output = true;
        (void)clock_gettime(CLOCK_MONOTONIC, &start);
        test_passed = test_case->function(&context);
        (void)clock_gettime(CLOCK_MONOTONIC, &end);
        suppress_child_output = false;
    }

    if (original_directory != NULL && chdir(original_directory) != 0) {
        environment_ready = false;
        set_test_failure(&context, "WORKING_DIRECTORY_RESTORE_FAILED",
                         "restore runner working directory", true);
    }
    if (test_directory != NULL && !cleanup_test_directory(test_directory)) {
        environment_ready = false;
        set_test_failure(&context, "TEST_DIRECTORY_CLEANUP_FAILED",
                         test_directory, true);
    }
    free(original_directory);

    result.duration_ms = elapsed_milliseconds(&start, &end);
    result.status = test_passed && environment_ready
                  ? TEST_PASS
                  : (context.environment_error ? TEST_ERROR : TEST_FAIL);
    (void)snprintf(result.failure_reason, sizeof(result.failure_reason),
                   "%s", context.failure_reason);
    (void)snprintf(result.reproduction, sizeof(result.reproduction),
                   "%s", context.reproduction);
    result.expected_value = context.expected_value;
    result.actual_value = context.actual_value;
    result.has_comparison = context.has_comparison;
    return result;
}

static void reset_test_summary(void)
{
    memset(&last_summary, 0, sizeof(last_summary));
}

static void add_test_result(TestResult result)
{
    if (last_summary.count >= MAX_TEST_RESULTS) {
        return;
    }
    last_summary.results[last_summary.count] = result;
    ++last_summary.count;
    last_summary.duration_ms += result.duration_ms;
    if (result.status == TEST_PASS) {
        ++last_summary.passed;
    } else if (result.status == TEST_FAIL) {
        ++last_summary.failed;
    } else {
        ++last_summary.errors;
    }
    (void)printf("[%s] %s %s (%.3f ms)\n", test_status_text(result.status),
                 result.id, result.name, result.duration_ms);
    if (result.status != TEST_PASS) {
        (void)printf("  reason: %s\n  reproduce: %s\n",
                     result.failure_reason, result.reproduction);
        if (result.has_comparison) {
            (void)printf("  expected: 0x%08" PRIX32
                         " actual: 0x%08" PRIX32 "\n",
                         result.expected_value, result.actual_value);
        }
    }
}

static void print_test_summary(void)
{
    double completed = (double)(last_summary.passed + last_summary.failed);
    double pass_rate = completed > 0.0
                     ? ((double)last_summary.passed / completed) * 100.0
                     : 0.0;

    (void)printf("SUMMARY total=%zu pass=%zu fail=%zu error=%zu "
                 "duration_ms=%.3f pass_rate=%.2f%%\n",
                 last_summary.count, last_summary.passed,
                 last_summary.failed, last_summary.errors,
                 last_summary.duration_ms, pass_rate);
}

static bool ensure_directory(const char *path)
{
    return mkdir(path, 0775) == 0 || errno == EEXIST;
}

static bool export_test_report(const char *ssd_path)
{
    FILE *log_file;
    FILE *summary_file;
    FILE *csv_file;
    FILE *markdown_file;
    time_t now = time(NULL);
    struct tm local_time;
    char timestamp[32] = "unknown";
    size_t index;
    bool close_ok = true;

    if (!ensure_directory("logs") || !ensure_directory("reports")) {
        return false;
    }
    if (localtime_r(&now, &local_time) != NULL) {
        (void)strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z",
                       &local_time);
    }

    log_file = fopen("logs/test_result.log", "w");
    summary_file = fopen("logs/test_summary.txt", "w");
    csv_file = fopen("reports/test_report.csv", "w");
    markdown_file = fopen("reports/test_report.md", "w");
    if (log_file == NULL || summary_file == NULL || csv_file == NULL
        || markdown_file == NULL) {
        if (log_file != NULL) (void)fclose(log_file);
        if (summary_file != NULL) (void)fclose(summary_file);
        if (csv_file != NULL) (void)fclose(csv_file);
        if (markdown_file != NULL) (void)fclose(markdown_file);
        return false;
    }

    (void)fprintf(csv_file, "test_id,requirement_id,name,status,duration_ms,"
                  "expected,actual,failure_reason,reproduction\n");
    (void)fprintf(markdown_file, "# Automated Test Report\n\n"
                  "- Executed: %s\n- SSD: `%s`\n\n"
                  "| Test ID | Requirement | Name | Status | Duration (ms) |\n"
                  "|---|---|---|---:|---:|\n", timestamp, ssd_path);

    for (index = 0U; index < last_summary.count; ++index) {
        const TestResult *result = &last_summary.results[index];
        const char *status = test_status_text(result->status);
        (void)fprintf(log_file,
                      "[TIME] %s\n[TEST] %s\n[REQUIREMENT] %s\n"
                      "[SSD] %s\n[RESULT] %s\n[DURATION_MS] %.3f\n",
                      timestamp, result->id, result->requirement_id,
                      ssd_path, status, result->duration_ms);
        if (result->has_comparison) {
            (void)fprintf(log_file, "[EXPECTED] 0x%08" PRIX32
                          "\n[ACTUAL] 0x%08" PRIX32 "\n",
                          result->expected_value, result->actual_value);
        }
        if (result->status != TEST_PASS) {
            (void)fprintf(log_file, "[REASON] %s\n[REPRODUCE] %s\n",
                          result->failure_reason, result->reproduction);
        }
        (void)fputc('\n', log_file);

        (void)fprintf(csv_file, "%s,%s,%s,%s,%.3f,",
                      result->id, result->requirement_id, result->name,
                      status, result->duration_ms);
        if (result->has_comparison) {
            (void)fprintf(csv_file, "0x%08" PRIX32 ",0x%08" PRIX32 ",",
                          result->expected_value, result->actual_value);
        } else {
            (void)fprintf(csv_file, ",,");
        }
        (void)fprintf(csv_file, "%s,%s\n", result->failure_reason,
                      result->reproduction);
        (void)fprintf(markdown_file, "| %s | %s | %s | %s | %.3f |\n",
                      result->id, result->requirement_id, result->name,
                      status, result->duration_ms);
    }

    (void)fprintf(summary_file,
                  "Total: %zu\nPass: %zu\nFail: %zu\nError: %zu\n"
                  "DurationMs: %.3f\n",
                  last_summary.count, last_summary.passed,
                  last_summary.failed, last_summary.errors,
                  last_summary.duration_ms);
    (void)fprintf(markdown_file,
                  "\n## Summary\n\n- Total: %zu\n- Pass: %zu\n"
                  "- Fail: %zu\n- Error: %zu\n- Duration: %.3f ms\n",
                  last_summary.count, last_summary.passed,
                  last_summary.failed, last_summary.errors,
                  last_summary.duration_ms);

    if (fclose(log_file) != 0) close_ok = false;
    if (fclose(summary_file) != 0) close_ok = false;
    if (fclose(csv_file) != 0) close_ok = false;
    if (fclose(markdown_file) != 0) close_ok = false;
    return close_ok;
}

static const TestCase *find_test_case(const char *id)
{
    size_t index;
    for (index = 0U; index < sizeof(test_cases) / sizeof(test_cases[0]);
         ++index) {
        if (strcmp(test_cases[index].id, id) == 0) {
            return &test_cases[index];
        }
    }
    return NULL;
}

static bool run_selected_tests(const char *ssd_path, const char *test_id)
{
    size_t index;
    const TestCase *selected;

    reset_test_summary();
    if (test_id != NULL) {
        selected = find_test_case(test_id);
        if (selected == NULL) {
            (void)printf("UNKNOWN TEST: %s\n", test_id);
            return false;
        }
        add_test_result(run_test_case(selected, ssd_path));
    } else {
        for (index = 0U;
             index < sizeof(test_cases) / sizeof(test_cases[0]); ++index) {
            add_test_result(run_test_case(&test_cases[index], ssd_path));
        }
    }
    if (!export_test_report(ssd_path)) {
        (void)fprintf(stderr, "failed to export test report\n");
        ++last_summary.errors;
        print_test_summary();
        return false;
    }
    print_test_summary();
    return last_summary.failed == 0U && last_summary.errors == 0U;
}

static char *make_absolute_path(const char *path)
{
    char *current_directory;
    char *absolute_path;
    size_t needed;

    if (path == NULL || path[0] == '\0') {
        return NULL;
    }
    if (path[0] == '/') {
        return strdup(path);
    }
    current_directory = getcwd(NULL, 0U);
    if (current_directory == NULL) {
        return NULL;
    }
    needed = strlen(current_directory) + strlen(path) + 2U;
    absolute_path = malloc(needed);
    if (absolute_path != NULL) {
        (void)snprintf(absolute_path, needed, "%s/%s", current_directory,
                       path);
    }
    free(current_directory);
    return absolute_path;
}

static void command_help(void)
{
    (void)puts("write <LBA> <0xVALUE>  Write one LBA");
    (void)puts("read <LBA>             Read one LBA");
    (void)puts("fullwrite <0xVALUE>    Write all LBAs");
    (void)puts("fullread                Read all LBAs");
    (void)puts("testapp1               Run TestApp1");
    (void)puts("testapp2               Run TestApp2");
    (void)puts("testall                Run all automated tests");
    (void)puts("testcase <ID>          Run one automated test");
    (void)puts("testsummary            Show the last test summary");
    (void)puts("help                    Show this help");
    (void)puts("exit                    Exit Test Shell");
}

static bool dispatch_command(const char *ssd_path, char *tokens[MAX_TOKENS],
                             size_t token_count, bool has_extra_token,
                             bool *should_exit)
{
    *should_exit = false;
    if (has_extra_token || token_count == 0U) {
        (void)puts("INVALID COMMAND");
        return false;
    }

    if (strcmp(tokens[0], "write") == 0 && token_count == 3U) {
        return command_write(ssd_path, tokens[1], tokens[2]);
    }
    if (strcmp(tokens[0], "read") == 0 && token_count == 2U) {
        return command_read(ssd_path, tokens[1]);
    }
    if (strcmp(tokens[0], "fullwrite") == 0 && token_count == 2U) {
        return command_fullwrite(ssd_path, tokens[1]);
    }
    if (strcmp(tokens[0], "fullread") == 0 && token_count == 1U) {
        return command_fullread(ssd_path);
    }
    if (strcmp(tokens[0], "testapp1") == 0 && token_count == 1U) {
        return run_selected_tests(ssd_path, "TC-S001");
    }
    if (strcmp(tokens[0], "testapp2") == 0 && token_count == 1U) {
        return run_selected_tests(ssd_path, "TC-S002");
    }
    if (strcmp(tokens[0], "testall") == 0 && token_count == 1U) {
        return run_selected_tests(ssd_path, NULL);
    }
    if (strcmp(tokens[0], "testcase") == 0 && token_count == 2U) {
        return run_selected_tests(ssd_path, tokens[1]);
    }
    if (strcmp(tokens[0], "testsummary") == 0 && token_count == 1U) {
        if (last_summary.count == 0U) {
            (void)puts("No test results.");
        } else {
            print_test_summary();
        }
        return true;
    }
    if (strcmp(tokens[0], "help") == 0 && token_count == 1U) {
        command_help();
        return true;
    }
    if (strcmp(tokens[0], "exit") == 0 && token_count == 1U) {
        *should_exit = true;
        return true;
    }

    (void)puts("INVALID COMMAND");
    return false;
}

static int run_interactive_shell(const char *ssd_path)
{
    char input[INPUT_BUFFER_SIZE];
    char *tokens[MAX_TOKENS];
    bool interactive = isatty(STDIN_FILENO) != 0;
    bool should_exit = false;

    while (!should_exit) {
        InputStatus input_status;
        bool has_extra_token;
        size_t token_count;

        if (interactive) {
            (void)fputs("Shell> ", stdout);
            (void)fflush(stdout);
        }
        input_status = read_user_input(input);
        if (input_status == INPUT_EOF) {
            break;
        }
        if (input_status == INPUT_TOO_LONG) {
            (void)puts("INVALID COMMAND");
            continue;
        }
        if (input_status == INPUT_ERROR) {
            (void)fprintf(stderr, "input error\n");
            return SHELL_EXIT_USAGE;
        }

        token_count = tokenize_command(input, tokens, &has_extra_token);
        (void)dispatch_command(ssd_path, tokens, token_count,
                               has_extra_token, &should_exit);
    }
    return SHELL_EXIT_OK;
}

int main(int argc, char *argv[])
{
    const char *configured_ssd_path = "./ssd";
    const char *run_test_id = NULL;
    char *ssd_path;
    bool run_tests = false;
    int argument_index = 1;
    int exit_code;

    while (argument_index < argc) {
        if (strcmp(argv[argument_index], "--ssd-path") == 0
            && argument_index + 1 < argc
            && argv[argument_index + 1][0] != '\0') {
            configured_ssd_path = argv[argument_index + 1];
            argument_index += 2;
        } else if (strcmp(argv[argument_index], "--run-tests") == 0
                   && !run_tests && run_test_id == NULL) {
            run_tests = true;
            ++argument_index;
        } else if (strcmp(argv[argument_index], "--run-test") == 0
                   && argument_index + 1 < argc
                   && !run_tests && run_test_id == NULL) {
            run_test_id = argv[argument_index + 1];
            argument_index += 2;
        } else {
            (void)puts("INVALID COMMAND");
            (void)fprintf(stderr,
                          "usage: testshell [--ssd-path PATH] "
                          "[--run-tests | --run-test ID]\n");
            return SHELL_EXIT_USAGE;
        }
    }

    ssd_path = make_absolute_path(configured_ssd_path);
    if (ssd_path == NULL) {
        (void)fprintf(stderr, "cannot resolve SSD path\n");
        return SHELL_EXIT_USAGE;
    }

    if (run_tests || run_test_id != NULL) {
        if (run_test_id != NULL && find_test_case(run_test_id) == NULL) {
            (void)printf("UNKNOWN TEST: %s\n", run_test_id);
            free(ssd_path);
            return SHELL_EXIT_USAGE;
        }
        (void)run_selected_tests(ssd_path, run_test_id);
        exit_code = last_summary.errors > 0U
                  ? SHELL_EXIT_USAGE
                  : (last_summary.failed > 0U
                     ? SHELL_EXIT_TEST_FAILURE
                     : SHELL_EXIT_OK);
    } else {
        exit_code = run_interactive_shell(ssd_path);
    }

    free(ssd_path);
    return exit_code;
}
