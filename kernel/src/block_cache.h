#ifndef JOSHOS_BLOCK_CACHE_H
#define JOSHOS_BLOCK_CACHE_H

#include "block.h"
#include <stdint.h>

#define BLOCK_CACHE_ENTRIES 8u
#define BLOCK_CACHE_MAX_SECTOR 4096u

typedef struct {
    int valid;
    const block_device_t *device;
    uint64_t lba;
    uint64_t age;
    uint8_t data[BLOCK_CACHE_MAX_SECTOR];
} block_cache_entry_t;

typedef struct {
    uint64_t clock;
    uint64_t hits;
    uint64_t misses;
    block_cache_entry_t entries[BLOCK_CACHE_ENTRIES];
} block_cache_t;

void block_cache_init(block_cache_t *cache);
block_status_t block_cache_read(block_cache_t *cache, const block_device_t *device,
                                uint64_t lba, void *sector_out);
block_status_t block_cache_write(block_cache_t *cache, const block_device_t *device,
                                 uint64_t lba, const void *sector);
block_status_t block_cache_flush(block_cache_t *cache, const block_device_t *device);
void block_cache_invalidate(block_cache_t *cache, const block_device_t *device);

#endif
