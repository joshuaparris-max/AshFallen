#ifndef JOSHOS_BLOCK_H
#define JOSHOS_BLOCK_H

#include <stdint.h>

#define JOSH_BLOCK_SECTOR_SIZE 512u

typedef int (*josh_block_read_fn)(
    void *context,
    uint64_t lba,
    uint32_t sector_count,
    void *buffer
);

typedef int (*josh_block_write_fn)(
    void *context,
    uint64_t lba,
    uint32_t sector_count,
    const void *buffer
);

typedef struct {
    void *context;
    uint64_t sector_count;
    josh_block_read_fn read;
    josh_block_write_fn write;
} josh_block_device_t;

static inline int josh_block_range_valid(
    const josh_block_device_t *device,
    uint64_t lba,
    uint32_t sector_count
) {
    if (!device || sector_count == 0) return 0;
    if (lba >= device->sector_count) return 0;
    return (uint64_t)sector_count <= device->sector_count - lba;
}

static inline int josh_block_read(
    const josh_block_device_t *device,
    uint64_t lba,
    uint32_t sector_count,
    void *buffer
) {
    if (!device || !device->read || !buffer ||
        !josh_block_range_valid(device, lba, sector_count)) {
        return -1;
    }
    return device->read(device->context, lba, sector_count, buffer);
}

static inline int josh_block_write(
    const josh_block_device_t *device,
    uint64_t lba,
    uint32_t sector_count,
    const void *buffer
) {
    if (!device || !device->write || !buffer ||
        !josh_block_range_valid(device, lba, sector_count)) {
        return -1;
    }
    return device->write(device->context, lba, sector_count, buffer);
}

#endif
