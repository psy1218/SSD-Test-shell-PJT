#define _POSIX_C_SOURCE 200809L

#include "ssd.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define METRIC_COUNT 7U
#define MAX_ITERATIONS 20U

typedef struct {
    const char *name;
    double samples[MAX_ITERATIONS];
    double minimum;
    double average;
    double median;
    double maximum;
} Metric;

static double elapsed_ms(const struct timespec *start, const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) * 1000.0
         + (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

static bool run_quiet_process(const char *path, char *const arguments[])
{
    pid_t child = fork();
    pid_t waited;
    int status;

    if (child < 0) {
        return false;
    }
    if (child == 0) {
        int null_descriptor = open("/dev/null", O_WRONLY);
        if (null_descriptor >= 0) {
            (void)dup2(null_descriptor, STDOUT_FILENO);
            (void)dup2(null_descriptor, STDERR_FILENO);
            (void)close(null_descriptor);
        }
        execv(path, arguments);
        _exit(127);
    }
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    return waited == child && WIFEXITED(status) != 0
        && WEXITSTATUS(status) == 0;
}

static bool process_write_100(const char *ssd_path, double *duration)
{
    struct timespec start;
    struct timespec end;
    size_t lba;

    (void)remove("nand.txt");
    (void)remove("result.txt");
    (void)clock_gettime(CLOCK_MONOTONIC, &start);
    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        char lba_text[4];
        char value_text[11];
        char *arguments[5];
        uint32_t value = UINT32_C(0xA5000000) | (uint32_t)lba;

        (void)snprintf(lba_text, sizeof(lba_text), "%zu", lba);
        (void)snprintf(value_text, sizeof(value_text), "0x%08" PRIX32, value);
        arguments[0] = (char *)ssd_path;
        arguments[1] = (char *)"W";
        arguments[2] = lba_text;
        arguments[3] = value_text;
        arguments[4] = NULL;
        if (!run_quiet_process(ssd_path, arguments)) {
            return false;
        }
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &end);
    *duration = elapsed_ms(&start, &end);
    return true;
}

static bool process_read_100(const char *ssd_path, double *duration)
{
    struct timespec start;
    struct timespec end;
    size_t lba;

    (void)clock_gettime(CLOCK_MONOTONIC, &start);
    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        char lba_text[4];
        char *arguments[4];

        (void)snprintf(lba_text, sizeof(lba_text), "%zu", lba);
        arguments[0] = (char *)ssd_path;
        arguments[1] = (char *)"R";
        arguments[2] = lba_text;
        arguments[3] = NULL;
        if (!run_quiet_process(ssd_path, arguments)) {
            return false;
        }
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &end);
    *duration = elapsed_ms(&start, &end);
    return true;
}

static bool direct_write_100(double *duration)
{
    struct timespec start;
    struct timespec end;
    size_t lba;

    (void)remove("direct_nand.bin");
    (void)clock_gettime(CLOCK_MONOTONIC, &start);
    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        uint32_t value = UINT32_C(0xA5000000) | (uint32_t)lba;
        if (ssd_storage_write_lba("direct_nand.bin", lba, value)
            != SSD_STATUS_OK) {
            return false;
        }
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &end);
    *duration = elapsed_ms(&start, &end);
    return true;
}

static bool direct_read_100(double *duration)
{
    struct timespec start;
    struct timespec end;
    size_t lba;

    (void)clock_gettime(CLOCK_MONOTONIC, &start);
    for (lba = 0U; lba < SSD_LBA_COUNT; ++lba) {
        uint32_t value;
        if (ssd_storage_read_lba("direct_nand.bin", lba, &value)
            != SSD_STATUS_OK
            || value != (UINT32_C(0xA5000000) | (uint32_t)lba)) {
            return false;
        }
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &end);
    *duration = elapsed_ms(&start, &end);
    return true;
}

static bool measure_testshell(const char *testshell_path, const char *ssd_path,
                              const char *option, const char *test_id,
                              double *duration)
{
    struct timespec start;
    struct timespec end;
    char *arguments[7];

    arguments[0] = (char *)testshell_path;
    arguments[1] = (char *)"--ssd-path";
    arguments[2] = (char *)ssd_path;
    arguments[3] = (char *)option;
    arguments[4] = (char *)test_id;
    arguments[5] = NULL;

    (void)clock_gettime(CLOCK_MONOTONIC, &start);
    if (!run_quiet_process(testshell_path, arguments)) {
        return false;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &end);
    *duration = elapsed_ms(&start, &end);
    return true;
}

static int compare_double(const void *left, const void *right)
{
    double first = *(const double *)left;
    double second = *(const double *)right;
    return first < second ? -1 : (first > second ? 1 : 0);
}

static void calculate_statistics(Metric *metric, size_t iterations)
{
    double sorted[MAX_ITERATIONS];
    double total = 0.0;
    size_t index;

    memcpy(sorted, metric->samples, iterations * sizeof(double));
    qsort(sorted, iterations, sizeof(double), compare_double);
    for (index = 0U; index < iterations; ++index) {
        total += metric->samples[index];
    }
    metric->minimum = sorted[0];
    metric->maximum = sorted[iterations - 1U];
    metric->average = total / (double)iterations;
    metric->median = iterations % 2U == 0U
                   ? (sorted[iterations / 2U - 1U] + sorted[iterations / 2U])
                     / 2.0
                   : sorted[iterations / 2U];
}

static bool remove_if_present(const char *path)
{
    return remove(path) == 0 || errno == ENOENT;
}

static bool cleanup_workspace(const char *directory)
{
    char path[512];
    const char *files[] = {
        "nand.txt", "result.txt", "direct_nand.bin",
        "logs/test_result.log", "logs/test_summary.txt",
        "reports/test_report.csv", "reports/test_report.md"
    };
    size_t index;

    for (index = 0U; index < sizeof(files) / sizeof(files[0]); ++index) {
        int length = snprintf(path, sizeof(path), "%s/%s", directory,
                              files[index]);
        if (length < 0 || (size_t)length >= sizeof(path)
            || !remove_if_present(path)) {
            return false;
        }
    }
    (void)snprintf(path, sizeof(path), "%s/logs", directory);
    if (rmdir(path) != 0 && errno != ENOENT) return false;
    (void)snprintf(path, sizeof(path), "%s/reports", directory);
    if (rmdir(path) != 0 && errno != ENOENT) return false;
    return rmdir(directory) == 0;
}

static bool write_reports(const char *csv_path, const char *markdown_path,
                          Metric metrics[METRIC_COUNT], size_t iterations)
{
    FILE *csv = fopen(csv_path, "w");
    FILE *markdown = fopen(markdown_path, "w");
    size_t index;
    bool close_ok = true;
    double write_ratio = metrics[0].average / metrics[2].average;
    double read_ratio = metrics[1].average / metrics[3].average;

    if (csv == NULL || markdown == NULL) {
        if (csv != NULL) (void)fclose(csv);
        if (markdown != NULL) (void)fclose(markdown);
        return false;
    }
    (void)fprintf(csv, "metric,iterations,min_ms,avg_ms,median_ms,max_ms\n");
    (void)fprintf(markdown,
                  "# Performance Report\n\n"
                  "- Clock: `CLOCK_MONOTONIC`\n"
                  "- Repetitions: %zu\n"
                  "- Unit: milliseconds\n\n"
                  "| Metric | Min | Average | Median | Max |\n"
                  "|---|---:|---:|---:|---:|\n", iterations);
    for (index = 0U; index < METRIC_COUNT; ++index) {
        const Metric *metric = &metrics[index];
        (void)fprintf(csv, "%s,%zu,%.3f,%.3f,%.3f,%.3f\n",
                      metric->name, iterations, metric->minimum,
                      metric->average, metric->median, metric->maximum);
        (void)fprintf(markdown, "| %s | %.3f | %.3f | %.3f | %.3f |\n",
                      metric->name, metric->minimum, metric->average,
                      metric->median, metric->maximum);
    }
    (void)fprintf(markdown,
                  "\n## Analysis\n\n"
                  "- Process/direct Write average ratio: %.2fx\n"
                  "- Process/direct Read average ratio: %.2fx\n"
                  "- The comparison measures process and file-simulator "
                  "overhead, not physical SSD performance.\n",
                  write_ratio, read_ratio);
    if (fclose(csv) != 0) close_ok = false;
    if (fclose(markdown) != 0) close_ok = false;
    return close_ok;
}

int main(int argc, char *argv[])
{
    Metric metrics[METRIC_COUNT] = {
        { "process_write_100", { 0 }, 0, 0, 0, 0 },
        { "process_read_100", { 0 }, 0, 0, 0, 0 },
        { "direct_write_100", { 0 }, 0, 0, 0, 0 },
        { "direct_read_100", { 0 }, 0, 0, 0, 0 },
        { "testapp1", { 0 }, 0, 0, 0, 0 },
        { "testapp2", { 0 }, 0, 0, 0, 0 },
        { "all_automated_tests", { 0 }, 0, 0, 0, 0 }
    };
    char directory_template[] = "/tmp/ssd-benchmark-XXXXXX";
    char *original_directory;
    char *benchmark_directory;
    char *end_pointer;
    unsigned long parsed_iterations;
    size_t iterations;
    size_t repetition;
    size_t index;
    bool success = true;

    if (argc != 6) {
        (void)fprintf(stderr,
                      "usage: benchmark SSD TESTSHELL ITERATIONS CSV MD\n");
        return EXIT_FAILURE;
    }
    errno = 0;
    parsed_iterations = strtoul(argv[3], &end_pointer, 10);
    if (errno != 0 || end_pointer == argv[3] || *end_pointer != '\0'
        || parsed_iterations == 0UL || parsed_iterations > MAX_ITERATIONS) {
        (void)fprintf(stderr, "iterations must be 1..%u\n", MAX_ITERATIONS);
        return EXIT_FAILURE;
    }
    iterations = (size_t)parsed_iterations;

    original_directory = getcwd(NULL, 0U);
    benchmark_directory = mkdtemp(directory_template);
    if (original_directory == NULL || benchmark_directory == NULL
        || chdir(benchmark_directory) != 0) {
        free(original_directory);
        (void)fprintf(stderr, "cannot create benchmark workspace\n");
        return EXIT_FAILURE;
    }

    for (repetition = 0U; repetition < iterations && success; ++repetition) {
        success = process_write_100(argv[1], &metrics[0].samples[repetition])
               && process_read_100(argv[1], &metrics[1].samples[repetition])
               && direct_write_100(&metrics[2].samples[repetition])
               && direct_read_100(&metrics[3].samples[repetition])
               && measure_testshell(argv[2], argv[1], "--run-test", "TC-S001",
                                    &metrics[4].samples[repetition])
               && measure_testshell(argv[2], argv[1], "--run-test", "TC-S002",
                                    &metrics[5].samples[repetition])
               && measure_testshell(argv[2], argv[1], "--run-tests", NULL,
                                    &metrics[6].samples[repetition]);
    }

    if (chdir(original_directory) != 0) {
        success = false;
    }
    if (!cleanup_workspace(benchmark_directory)) {
        success = false;
    }
    free(original_directory);

    if (!success) {
        (void)fprintf(stderr, "benchmark execution failed\n");
        return EXIT_FAILURE;
    }
    for (index = 0U; index < METRIC_COUNT; ++index) {
        calculate_statistics(&metrics[index], iterations);
        (void)printf("%s avg_ms=%.3f\n", metrics[index].name,
                     metrics[index].average);
    }
    if (!write_reports(argv[4], argv[5], metrics, iterations)) {
        (void)fprintf(stderr, "cannot write benchmark reports\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
