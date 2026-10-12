#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INPUT_BUFFER_SIZE 256U
#define MAX_TOKENS 4U
#define SSD_LBA_COUNT 100U
#define RESULT_FILE_PATH "result.txt"

#define SHELL_EXIT_OK 0
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

    if (file == NULL) {
        (void)fprintf(stderr, "cannot read %s: %s\n",
                      RESULT_FILE_PATH, strerror(errno));
        return false;
    }
    if (fgets(buffer, (int)sizeof(buffer), file) == NULL) {
        (void)fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        return false;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';
    length = strlen(buffer);
    if (length != 10U || buffer[0] != '0' || buffer[1] != 'x') {
        return false;
    }
    for (index = 2U; index < 10U; ++index) {
        if (!((buffer[index] >= '0' && buffer[index] <= '9')
              || (buffer[index] >= 'A' && buffer[index] <= 'F'))) {
            return false;
        }
    }
    memcpy(value_out, buffer, 11U);
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

static void command_help(void)
{
    (void)puts("write <LBA> <0xVALUE>  Write one LBA");
    (void)puts("read <LBA>             Read one LBA");
    (void)puts("fullwrite <0xVALUE>    Write all LBAs");
    (void)puts("fullread                Read all LBAs");
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
    const char *ssd_path = "./ssd";

    if (argc == 3 && strcmp(argv[1], "--ssd-path") == 0
        && argv[2][0] != '\0') {
        ssd_path = argv[2];
    } else if (argc != 1) {
        (void)puts("INVALID COMMAND");
        (void)fprintf(stderr, "usage: testshell [--ssd-path PATH]\n");
        return SHELL_EXIT_USAGE;
    }

    return run_interactive_shell(ssd_path);
}
