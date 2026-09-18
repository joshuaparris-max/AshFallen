#include "block_cache.h"
#include <stddef.h>
#include <stdint.h>

static void copy_bytes(void *destination, const void *source, uint32_t length) {
    uint8_t *out = destination;
    const uint8_t *in = source;
    for (uint32_t i = 0; i < length; ++i) out[i] = in[i];
}

void block_cache_init(block_cache_t *cache) {
    if (!cache) return;
    cache->clock = 0;
    cache->hits = 0;
    cache->misses = 0;
    for (uint32_t i = 0; i < BLOCK_CACHE_ENTRIES; ++i) cache->entries[i].valid = 0;
}

static int find_entry(block_cache_t *cache, const block_device_t *device, uint64_t lba) {
    for (uint32_t i = 0; i < BLOCK_CACHE_ENTRIES; ++i) {
        if (cache->entries[i].valid && cache->entries[i].device == device &&
            cache->entries[i].lba == lba) return (int)i;
    }
    return -1;
}

static uint32_t victim_entry(block_cache_t *cache) {
    uint32_t victim = 0;
    uint64_t oldest = UINT64_MAX;
    for (uint32_t i = 0; i < BLOCK_CACHE_ENTRIES; ++i) {
        if (!cache->entries[i].valid) return i;
        if (cache->entries[i].age < oldest) {
            oldest = cache->entries[i].age;
            victim = i;
        }
    }
    return victim;
}

block_status_t block_cache_read(block_cache_t *cache, const block_device_t *device,
                                uint64_t lba, void *sector_out) {
    if (!cache || !device || !sector_out || device->sector_size == 0 ||
        device->sector_size > BLOCK_CACHE_MAX_SECTOR) return BLOCK_BAD_ARGUMENT;

    int found = find_entry(cache, device, lba);
    if (found >= 0) {
        block_cache_entry_t *entry = &cache->entries[found];
        entry->age = ++cache->clock;
        cache->hits++;
        copy_bytes(sector_out, entry->data, device->sector_size);
        return BLOCK_OK;
    }

    uint32_t slot = victim_entry(cache);
    block_cache_entry_t *entry = &cache->entries[slot];
    block_status_t status = block_read(device, lba, 1, entry->data);
    if (status != BLOCK_OK) return status;
    entry->valid = 1;
    entry->device = device;
    entry->lba = lba;
    entry->age = ++cache->clock;
    cache->misses++;
    copy_bytes(sector_out, entry->data, device->sector_size);
    return BLOCK_OK;
}

block_status_t block_cache_write(block_cache_t *cache, const block_device_t *device,
                                 uint64_t lba, const void *sector) {
    if (!cache || !device || !sector || device->sector_size == 0 ||
        device->sector_size > BLOCK_CACHE_MAX_SECTOR) return BLOCK_BAD_ARGUMENT;

    block_status_t status = block_write(device, lba, 1, sector);
    if (status != BLOCK_OK) return status;

    int found = find_entry(cache, device, lba);
    uint32_t slot = found >= 0 ? (uint32_t)found : victim_entry(cache);
    block_cache_entry_t *entry = &cache->entries[slot];
    entry->valid = 1;
    entry->device = device;
    entry->lba = lba;
    entry->age = ++cache->clock;
    copy_bytes(entry->data, sector, device->sector_size);
    return BLOCK_OK;
}

block_status_t block_cache_flush(block_cache_t *cache, const block_device_t *device) {
    if (!cache || !device) return BLOCK_BAD_ARGUMENT;
    return block_flush(device);
}

void block_cache_invalidate(block_cache_t *cache, const block_device_t *device) {
    if (!cache) return;
    for (uint32_t i = 0; i < BLOCK_CACHE_ENTRIES; ++i) {
        if (!device || cache->entries[i].device == device) cache->entries[i].valid = 0;
    }
}
