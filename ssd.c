#define _POSIX_C_SOURCE 200809L

#include "ssd.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define NAND_FILE_PATH "nand.txt"
#define RESULT_FILE_PATH "result.txt"

#define SSD_EXIT_OK 0
#define SSD_EXIT_INVALID_COMMAND 2
#define SSD_EXIT_IO_ERROR 3
#define SSD_EXIT_CORRUPT_NAND 4
#define SSD_EXIT_INTERNAL_ERROR 5

static int path_is_valid(const char *path)
{
    return path != NULL && path[0] != '\0';
}

void ssd_encode_u32_le(uint32_t value, uint8_t out[SSD_BYTES_PER_LBA])
{
    if (out == NULL) {
        return;
    }

    out[0] = (uint8_t)(value & UINT32_C(0xFF));
    out[1] = (uint8_t)((value >> 8U) & UINT32_C(0xFF));
    out[2] = (uint8_t)((value >> 16U) & UINT32_C(0xFF));
    out[3] = (uint8_t)((value >> 24U) & UINT32_C(0xFF));
}

uint32_t ssd_decode_u32_le(const uint8_t in[SSD_BYTES_PER_LBA])
{
    if (in == NULL) {
        return UINT32_C(0);
    }

    return (uint32_t)in[0]
         | ((uint32_t)in[1] << 8U)
         | ((uint32_t)in[2] << 16U)
         | ((uint32_t)in[3] << 24U);
}

static SsdStatus close_checked(FILE *file, SsdStatus current_status)
{
    if (fclose(file) != 0 && current_status == SSD_STATUS_OK) {
        return SSD_STATUS_IO_ERROR;
    }
    return current_status;
}

static SsdStatus create_zeroed_nand(const char *path)
{
    uint32_t zeros[SSD_LBA_COUNT] = { UINT32_C(0) };
    return ssd_storage_save_atomic(path, zeros);
}

SsdStatus ssd_storage_load(const char *path,
                           uint32_t values[SSD_LBA_COUNT])
{
    uint8_t bytes[SSD_NAND_SIZE];
    uint32_t decoded[SSD_LBA_COUNT];
    FILE *file;
    struct stat file_information;
    long file_size;
    size_t index;
    size_t bytes_read;
    SsdStatus status = SSD_STATUS_OK;

    if (!path_is_valid(path) || values == NULL) {
        return SSD_STATUS_INVALID_ARGUMENT;
    }

    file = fopen(path, "rb");
    if (file == NULL && errno == ENOENT) {
        status = create_zeroed_nand(path);
        if (status != SSD_STATUS_OK) {
            return status;
        }
        file = fopen(path, "rb");
    }
    if (file == NULL) {
        return SSD_STATUS_IO_ERROR;
    }

    if (fstat(fileno(file), &file_information) != 0
        || !S_ISREG(file_information.st_mode)) {
        status = SSD_STATUS_IO_ERROR;
    } else if (fseek(file, 0L, SEEK_END) != 0) {
        status = SSD_STATUS_IO_ERROR;
    } else {
        file_size = ftell(file);
        if (file_size < 0L) {
            status = SSD_STATUS_IO_ERROR;
        } else if ((unsigned long)file_size != (unsigned long)SSD_NAND_SIZE) {
            status = SSD_STATUS_CORRUPT_NAND;
        }
    }

    if (status == SSD_STATUS_OK && fseek(file, 0L, SEEK_SET) != 0) {
        status = SSD_STATUS_IO_ERROR;
    }

    if (status == SSD_STATUS_OK) {
        bytes_read = fread(bytes, 1U, SSD_NAND_SIZE, file);
        if (bytes_read != SSD_NAND_SIZE) {
            status = ferror(file) != 0
                   ? SSD_STATUS_IO_ERROR
                   : SSD_STATUS_CORRUPT_NAND;
        }
    }

    status = close_checked(file, status);
    if (status != SSD_STATUS_OK) {
        return status;
    }

    for (index = 0U; index < SSD_LBA_COUNT; ++index) {
        decoded[index] = ssd_decode_u32_le(&bytes[index * SSD_BYTES_PER_LBA]);
    }
    memcpy(values, decoded, sizeof(decoded));
    return SSD_STATUS_OK;
}

SsdStatus ssd_storage_save_atomic(
    const char *path,
    const uint32_t values[SSD_LBA_COUNT])
{
    uint8_t bytes[SSD_NAND_SIZE];
    char *temporary_path;
    size_t path_length;
    size_t index;
    size_t bytes_written;
    int written_length;
    int file_descriptor;
    FILE *file;
    SsdStatus status = SSD_STATUS_OK;

    if (!path_is_valid(path) || values == NULL) {
        return SSD_STATUS_INVALID_ARGUMENT;
    }

    path_length = strlen(path);
    if (path_length > SIZE_MAX - 32U) {
        return SSD_STATUS_INVALID_ARGUMENT;
    }

    temporary_path = malloc(path_length + 32U);
    if (temporary_path == NULL) {
        return SSD_STATUS_IO_ERROR;
    }

    written_length = snprintf(temporary_path, path_length + 32U,
                              "%s.tmp.%ld", path, (long)getpid());
    if (written_length < 0
        || (size_t)written_length >= path_length + 32U) {
        free(temporary_path);
        return SSD_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0U; index < SSD_LBA_COUNT; ++index) {
        ssd_encode_u32_le(values[index],
                          &bytes[index * SSD_BYTES_PER_LBA]);
    }

    file = fopen(temporary_path, "wb");
    if (file == NULL) {
        free(temporary_path);
        return SSD_STATUS_IO_ERROR;
    }

    bytes_written = fwrite(bytes, 1U, SSD_NAND_SIZE, file);
    if (bytes_written != SSD_NAND_SIZE) {
        status = SSD_STATUS_IO_ERROR;
    }
    if (status == SSD_STATUS_OK && fflush(file) != 0) {
        status = SSD_STATUS_IO_ERROR;
    }

    if (status == SSD_STATUS_OK) {
        file_descriptor = fileno(file);
        if (file_descriptor < 0 || fsync(file_descriptor) != 0) {
            status = SSD_STATUS_IO_ERROR;
        }
    }

    status = close_checked(file, status);
    if (status == SSD_STATUS_OK && rename(temporary_path, path) != 0) {
        status = SSD_STATUS_IO_ERROR;
    }
    if (status != SSD_STATUS_OK) {
        (void)remove(temporary_path);
    }

    free(temporary_path);
    return status;
}

SsdStatus ssd_storage_read_lba(const char *path, size_t lba,
                               uint32_t *value_out)
{
    uint32_t values[SSD_LBA_COUNT];
    SsdStatus status;

    if (!path_is_valid(path) || value_out == NULL || lba >= SSD_LBA_COUNT) {
        return SSD_STATUS_INVALID_ARGUMENT;
    }

    status = ssd_storage_load(path, values);
    if (status == SSD_STATUS_OK) {
        *value_out = values[lba];
    }
    return status;
}

SsdStatus ssd_storage_write_lba(const char *path, size_t lba,
                                uint32_t value)
{
    uint32_t values[SSD_LBA_COUNT];
    SsdStatus status;

    if (!path_is_valid(path) || lba >= SSD_LBA_COUNT) {
        return SSD_STATUS_INVALID_ARGUMENT;
    }

    status = ssd_storage_load(path, values);
    if (status != SSD_STATUS_OK) {
        return status;
    }

    values[lba] = value;
    return ssd_storage_save_atomic(path, values);
}

#ifndef SSD_NO_MAIN
static bool parse_lba(const char *text, size_t *lba_out)
{
    size_t value = 0U;
    size_t index;

    if (text == NULL || lba_out == NULL || text[0] == '\0') {
        return false;
    }

    for (index = 0U; text[index] != '\0'; ++index) {
        unsigned int digit;

        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        digit = (unsigned int)(text[index] - '0');
        if (value > (SSD_LBA_COUNT - 1U) / 10U) {
            return false;
        }
        value = value * 10U + (size_t)digit;
        if (value >= SSD_LBA_COUNT) {
            return false;
        }
    }

    *lba_out = value;
    return true;
}

static bool parse_hex_value(const char *text, uint32_t *value_out)
{
    uint32_t value = UINT32_C(0);
    size_t index;

    if (text == NULL || value_out == NULL || strlen(text) != 10U
        || text[0] != '0' || text[1] != 'x') {
        return false;
    }

    for (index = 2U; index < 10U; ++index) {
        uint32_t digit;

        if (text[index] >= '0' && text[index] <= '9') {
            digit = (uint32_t)(text[index] - '0');
        } else if (text[index] >= 'A' && text[index] <= 'F') {
            digit = (uint32_t)(text[index] - 'A') + UINT32_C(10);
        } else {
            return false;
        }
        value = (value << 4U) | digit;
    }

    *value_out = value;
    return true;
}

static SsdStatus write_result_atomic(uint32_t value)
{
    char temporary_path[64];
    FILE *file;
    int written_length;
    int file_descriptor;
    SsdStatus status = SSD_STATUS_OK;

    written_length = snprintf(temporary_path, sizeof(temporary_path),
                              "%s.tmp.%ld", RESULT_FILE_PATH,
                              (long)getpid());
    if (written_length < 0
        || (size_t)written_length >= sizeof(temporary_path)) {
        return SSD_STATUS_IO_ERROR;
    }

    file = fopen(temporary_path, "wb");
    if (file == NULL) {
        return SSD_STATUS_IO_ERROR;
    }

    if (fprintf(file, "0x%08" PRIX32 "\n", value) < 0) {
        status = SSD_STATUS_IO_ERROR;
    }
    if (status == SSD_STATUS_OK && fflush(file) != 0) {
        status = SSD_STATUS_IO_ERROR;
    }
    if (status == SSD_STATUS_OK) {
        file_descriptor = fileno(file);
        if (file_descriptor < 0 || fsync(file_descriptor) != 0) {
            status = SSD_STATUS_IO_ERROR;
        }
    }

    status = close_checked(file, status);
    if (status == SSD_STATUS_OK
        && rename(temporary_path, RESULT_FILE_PATH) != 0) {
        status = SSD_STATUS_IO_ERROR;
    }
    if (status != SSD_STATUS_OK) {
        (void)remove(temporary_path);
    }
    return status;
}

static int status_to_exit_code(SsdStatus status)
{
    switch (status) {
    case SSD_STATUS_OK:
        return SSD_EXIT_OK;
    case SSD_STATUS_INVALID_ARGUMENT:
        return SSD_EXIT_INVALID_COMMAND;
    case SSD_STATUS_IO_ERROR:
        return SSD_EXIT_IO_ERROR;
    case SSD_STATUS_CORRUPT_NAND:
        return SSD_EXIT_CORRUPT_NAND;
    default:
        return SSD_EXIT_INTERNAL_ERROR;
    }
}

static int report_invalid_command(const char *detail)
{
    (void)puts("INVALID COMMAND");
    if (detail != NULL) {
        (void)fprintf(stderr, "%s\n", detail);
    }
    return SSD_EXIT_INVALID_COMMAND;
}

static int report_storage_error(SsdStatus status, const char *operation)
{
    (void)puts("INVALID COMMAND");
    (void)fprintf(stderr, "%s failed: storage status %d\n",
                  operation, (int)status);
    return status_to_exit_code(status);
}

static int execute_write(const char *lba_text, const char *value_text)
{
    size_t lba;
    uint32_t value;
    SsdStatus status;

    if (!parse_lba(lba_text, &lba) || !parse_hex_value(value_text, &value)) {
        return report_invalid_command("invalid write argument");
    }

    status = ssd_storage_write_lba(NAND_FILE_PATH, lba, value);
    if (status != SSD_STATUS_OK) {
        return report_storage_error(status, "write");
    }
    return SSD_EXIT_OK;
}

static int execute_read(const char *lba_text)
{
    size_t lba;
    uint32_t value;
    SsdStatus status;

    if (!parse_lba(lba_text, &lba)) {
        return report_invalid_command("invalid read argument");
    }

    status = ssd_storage_read_lba(NAND_FILE_PATH, lba, &value);
    if (status != SSD_STATUS_OK) {
        return report_storage_error(status, "read");
    }

    status = write_result_atomic(value);
    if (status != SSD_STATUS_OK) {
        return report_storage_error(status, "result write");
    }
    return SSD_EXIT_OK;
}

int main(int argc, char *argv[])
{
    if (argc == 4 && strcmp(argv[1], "W") == 0) {
        return execute_write(argv[2], argv[3]);
    }
    if (argc == 3 && strcmp(argv[1], "R") == 0) {
        return execute_read(argv[2]);
    }
    return report_invalid_command("expected: ssd W LBA 0xVALUE or ssd R LBA");
}
#endif
