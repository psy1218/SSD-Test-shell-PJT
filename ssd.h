#ifndef SSD_H
#define SSD_H

#include <stddef.h>
#include <stdint.h>

#define SSD_LBA_COUNT 100U
#define SSD_BYTES_PER_LBA 4U
#define SSD_NAND_SIZE (SSD_LBA_COUNT * SSD_BYTES_PER_LBA)

typedef enum {
    SSD_STATUS_OK = 0,
    SSD_STATUS_INVALID_ARGUMENT = 1,
    SSD_STATUS_IO_ERROR = 2,
    SSD_STATUS_CORRUPT_NAND = 3
} SsdStatus;

void ssd_encode_u32_le(uint32_t value, uint8_t out[SSD_BYTES_PER_LBA]);
uint32_t ssd_decode_u32_le(const uint8_t in[SSD_BYTES_PER_LBA]);

SsdStatus ssd_storage_load(const char *path,
                           uint32_t values[SSD_LBA_COUNT]);
SsdStatus ssd_storage_save_atomic(
    const char *path,
    const uint32_t values[SSD_LBA_COUNT]);
SsdStatus ssd_storage_read_lba(const char *path, size_t lba,
                               uint32_t *value_out);
SsdStatus ssd_storage_write_lba(const char *path, size_t lba,
                                uint32_t value);

#endif
