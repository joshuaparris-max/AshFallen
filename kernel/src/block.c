#include "block.h"

static block_device_t devices[BLOCK_MAX_DEVICES];
static uint32_t device_count;

static void copy_name(char destination[BLOCK_NAME_MAX + 1u], const char *source) {
    uint32_t i = 0;
    if (source) {
        while (i < BLOCK_NAME_MAX && source[i]) {
            destination[i] = source[i];
            i++;
        }
    }
    destination[i] = '\0';
}

static int request_in_range(const block_device_t *device, uint64_t lba, uint32_t count) {
    if (!device || count == 0) return 0;
    if (lba >= device->sector_count) return 0;
    return (uint64_t)count <= device->sector_count - lba;
}

void block_registry_reset(void) {
    device_count = 0;
}

block_status_t block_register(const block_device_t *device, uint32_t *index_out) {
    if (!device || !device->read || device->sector_size == 0 || device->sector_count == 0) {
        return BLOCK_BAD_ARGUMENT;
    }
    if (device->writable && !device->write) return BLOCK_BAD_ARGUMENT;
    if (device_count >= BLOCK_MAX_DEVICES) return BLOCK_REGISTRY_FULL;

    devices[device_count] = *device;
    copy_name(devices[device_count].name, device->name);
    if (index_out) *index_out = device_count;
    device_count++;
    return BLOCK_OK;
}

uint32_t block_count(void) {
    return device_count;
}

const block_device_t *block_get(uint32_t index) {
    return index < device_count ? &devices[index] : 0;
}

block_status_t block_read(const block_device_t *device, uint64_t lba, uint32_t count, void *buffer) {
    if (!buffer || !device || !device->read) return BLOCK_BAD_ARGUMENT;
    if (!request_in_range(device, lba, count)) return BLOCK_OUT_OF_RANGE;
    return device->read(device->context, lba, count, buffer);
}

block_status_t block_write(const block_device_t *device, uint64_t lba, uint32_t count, const void *buffer) {
    if (!buffer || !device) return BLOCK_BAD_ARGUMENT;
    if (!device->writable || !device->write) return BLOCK_READ_ONLY;
    if (!request_in_range(device, lba, count)) return BLOCK_OUT_OF_RANGE;
    return device->write(device->context, lba, count, buffer);
}

block_status_t block_flush(const block_device_t *device) {
    if (!device) return BLOCK_BAD_ARGUMENT;
    if (!device->flush) return BLOCK_OK;
    return device->flush(device->context);
}

const char *block_status_string(block_status_t status) {
    switch (status) {
        case BLOCK_OK: return "ok";
        case BLOCK_BAD_ARGUMENT: return "bad argument";
        case BLOCK_OUT_OF_RANGE: return "out of range";
        case BLOCK_IO_ERROR: return "I/O error";
        case BLOCK_READ_ONLY: return "read only";
        case BLOCK_REGISTRY_FULL: return "registry full";
        default: return "unknown block error";
    }
}
