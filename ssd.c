#define _POSIX_C_SOURCE 200809L

#include "ssd.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

    if (fseek(file, 0L, SEEK_END) != 0) {
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
