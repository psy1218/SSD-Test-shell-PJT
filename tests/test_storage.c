#define _POSIX_C_SOURCE 200809L

#include "ssd.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned int tests_run;
static unsigned int tests_failed;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            (void)fprintf(stderr, "  assertion failed: %s (%s:%d)\n",       \
                          #condition, __FILE__, __LINE__);                    \
            return 0;                                                        \
        }                                                                    \
    } while (0)

static void run_test(const char *name, int (*test_function)(const char *),
                     const char *nand_path)
{
    int passed;

    ++tests_run;
    (void)remove(nand_path);
    passed = test_function(nand_path);
    (void)remove(nand_path);
    if (passed != 0) {
        (void)printf("PASS %s\n", name);
    } else {
        ++tests_failed;
        (void)printf("FAIL %s\n", name);
    }
}

static int file_size_is(const char *path, off_t expected)
{
    struct stat information;
    return stat(path, &information) == 0 && information.st_size == expected;
}

static int write_raw_file(const char *path, size_t byte_count, uint8_t byte)
{
    FILE *file = fopen(path, "wb");
    size_t index;

    if (file == NULL) {
        return 0;
    }
    for (index = 0U; index < byte_count; ++index) {
        if (fputc((int)byte, file) == EOF) {
            (void)fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int test_encoding(const char *unused_path)
{
    uint8_t bytes[SSD_BYTES_PER_LBA] = { 0U, 0U, 0U, 0U };
    (void)unused_path;

    ssd_encode_u32_le(UINT32_C(0x12345678), bytes);
    CHECK(bytes[0] == UINT8_C(0x78));
    CHECK(bytes[1] == UINT8_C(0x56));
    CHECK(bytes[2] == UINT8_C(0x34));
    CHECK(bytes[3] == UINT8_C(0x12));
    CHECK(ssd_decode_u32_le(bytes) == UINT32_C(0x12345678));
    return 1;
}

static int test_missing_file_initialization(const char *nand_path)
{
    uint32_t values[SSD_LBA_COUNT];
    size_t index;

    CHECK(ssd_storage_load(nand_path, values) == SSD_STATUS_OK);
    CHECK(file_size_is(nand_path, (off_t)SSD_NAND_SIZE));
    for (index = 0U; index < SSD_LBA_COUNT; ++index) {
        CHECK(values[index] == UINT32_C(0));
    }
    return 1;
}

static int test_write_and_persistence(const char *nand_path)
{
    uint32_t loaded[SSD_LBA_COUNT];
    uint32_t actual = UINT32_C(0);

    CHECK(ssd_storage_write_lba(nand_path, 3U, UINT32_C(0x12345678))
          == SSD_STATUS_OK);
    CHECK(ssd_storage_read_lba(nand_path, 3U, &actual) == SSD_STATUS_OK);
    CHECK(actual == UINT32_C(0x12345678));
    memset(loaded, 0xA5, sizeof(loaded));
    CHECK(ssd_storage_load(nand_path, loaded) == SSD_STATUS_OK);
    CHECK(loaded[3] == UINT32_C(0x12345678));
    CHECK(loaded[2] == UINT32_C(0));
    CHECK(loaded[4] == UINT32_C(0));
    return 1;
}

static int test_boundary_lbas(const char *nand_path)
{
    uint32_t actual = UINT32_C(0);

    CHECK(ssd_storage_write_lba(nand_path, 0U, UINT32_C(0x01020304))
          == SSD_STATUS_OK);
    CHECK(ssd_storage_write_lba(nand_path, 99U, UINT32_C(0xAABBCCDD))
          == SSD_STATUS_OK);
    CHECK(ssd_storage_read_lba(nand_path, 0U, &actual) == SSD_STATUS_OK);
    CHECK(actual == UINT32_C(0x01020304));
    CHECK(ssd_storage_read_lba(nand_path, 99U, &actual) == SSD_STATUS_OK);
    CHECK(actual == UINT32_C(0xAABBCCDD));
    return 1;
}

static int test_invalid_lba(const char *nand_path)
{
    uint32_t actual = UINT32_C(0xCAFEBABE);

    CHECK(ssd_storage_write_lba(nand_path, 100U, UINT32_C(1))
          == SSD_STATUS_INVALID_ARGUMENT);
    CHECK(ssd_storage_read_lba(nand_path, 100U, &actual)
          == SSD_STATUS_INVALID_ARGUMENT);
    CHECK(actual == UINT32_C(0xCAFEBABE));
    CHECK(access(nand_path, F_OK) != 0);
    return 1;
}

static int test_raw_little_endian(const char *nand_path)
{
    FILE *file;
    uint8_t bytes[SSD_BYTES_PER_LBA];

    CHECK(ssd_storage_write_lba(nand_path, 3U, UINT32_C(0x12345678))
          == SSD_STATUS_OK);
    file = fopen(nand_path, "rb");
    CHECK(file != NULL);
    CHECK(fseek(file, 12L, SEEK_SET) == 0);
    CHECK(fread(bytes, 1U, sizeof(bytes), file) == sizeof(bytes));
    CHECK(fclose(file) == 0);
    CHECK(bytes[0] == UINT8_C(0x78));
    CHECK(bytes[1] == UINT8_C(0x56));
    CHECK(bytes[2] == UINT8_C(0x34));
    CHECK(bytes[3] == UINT8_C(0x12));
    return 1;
}

static int test_corrupt_sizes(const char *nand_path)
{
    uint32_t values[SSD_LBA_COUNT];

    memset(values, 0xA5, sizeof(values));
    CHECK(write_raw_file(nand_path, SSD_NAND_SIZE - 1U, UINT8_C(0xCC)) != 0);
    CHECK(ssd_storage_load(nand_path, values) == SSD_STATUS_CORRUPT_NAND);
    CHECK(file_size_is(nand_path, (off_t)(SSD_NAND_SIZE - 1U)));

    CHECK(write_raw_file(nand_path, SSD_NAND_SIZE + 1U, UINT8_C(0xDD)) != 0);
    CHECK(ssd_storage_load(nand_path, values) == SSD_STATUS_CORRUPT_NAND);
    CHECK(file_size_is(nand_path, (off_t)(SSD_NAND_SIZE + 1U)));
    return 1;
}

static int test_invalid_arguments(const char *nand_path)
{
    uint32_t values[SSD_LBA_COUNT];
    uint32_t actual = UINT32_C(0);
    uint8_t bytes[SSD_BYTES_PER_LBA] = { 1U, 2U, 3U, 4U };

    CHECK(ssd_storage_load(NULL, values) == SSD_STATUS_INVALID_ARGUMENT);
    CHECK(ssd_storage_load("", values) == SSD_STATUS_INVALID_ARGUMENT);
    CHECK(ssd_storage_load(nand_path, NULL) == SSD_STATUS_INVALID_ARGUMENT);
    CHECK(ssd_storage_save_atomic(NULL, values)
          == SSD_STATUS_INVALID_ARGUMENT);
    CHECK(ssd_storage_save_atomic(nand_path, NULL)
          == SSD_STATUS_INVALID_ARGUMENT);
    CHECK(ssd_storage_read_lba(nand_path, 0U, NULL)
          == SSD_STATUS_INVALID_ARGUMENT);
    CHECK(ssd_storage_read_lba(NULL, 0U, &actual)
          == SSD_STATUS_INVALID_ARGUMENT);
    CHECK(ssd_storage_write_lba(NULL, 0U, UINT32_C(0))
          == SSD_STATUS_INVALID_ARGUMENT);
    ssd_encode_u32_le(UINT32_C(0xFFFFFFFF), NULL);
    CHECK(ssd_decode_u32_le(NULL) == UINT32_C(0));
    CHECK(bytes[0] == 1U);
    CHECK(access(nand_path, F_OK) != 0);
    return 1;
}

int main(void)
{
    char directory_template[] = "/tmp/ssd-phase1-XXXXXX";
    char nand_path[sizeof(directory_template) + 16U];
    char *directory = mkdtemp(directory_template);
    int path_length;

    if (directory == NULL) {
        (void)fprintf(stderr, "could not create isolated test directory\n");
        return EXIT_FAILURE;
    }

    path_length = snprintf(nand_path, sizeof(nand_path), "%s/nand.txt",
                           directory);
    if (path_length < 0 || (size_t)path_length >= sizeof(nand_path)) {
        (void)fprintf(stderr, "test path is too long\n");
        (void)rmdir(directory);
        return EXIT_FAILURE;
    }

    run_test("TC-STOR-001 encode/decode", test_encoding, nand_path);
    run_test("TC-STOR-002 missing file initialization",
             test_missing_file_initialization, nand_path);
    run_test("TC-STOR-003 write and persistence",
             test_write_and_persistence, nand_path);
    run_test("TC-STOR-004 boundary LBAs", test_boundary_lbas, nand_path);
    run_test("TC-STOR-005 invalid LBA", test_invalid_lba, nand_path);
    run_test("TC-STOR-006 raw little endian", test_raw_little_endian,
             nand_path);
    run_test("TC-STOR-007 corrupt sizes", test_corrupt_sizes, nand_path);
    run_test("TC-STOR-008 invalid arguments", test_invalid_arguments,
             nand_path);

    (void)remove(nand_path);
    if (rmdir(directory) != 0) {
        (void)fprintf(stderr, "warning: could not remove test directory\n");
    }

    (void)printf("SUMMARY total=%u passed=%u failed=%u\n",
                 tests_run, tests_run - tests_failed, tests_failed);
    return tests_failed == 0U ? EXIT_SUCCESS : EXIT_FAILURE;
}
