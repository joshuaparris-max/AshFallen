#ifndef JOSHOS_BLOCK_H
#define JOSHOS_BLOCK_H

#include <stddef.h>
#include <stdint.h>

#define BLOCK_MAX_DEVICES 8u
#define BLOCK_NAME_MAX 15u

typedef enum {
    BLOCK_OK = 0,
    BLOCK_BAD_ARGUMENT,
    BLOCK_OUT_OF_RANGE,
    BLOCK_IO_ERROR,
    BLOCK_READ_ONLY,
    BLOCK_REGISTRY_FULL
} block_status_t;

typedef block_status_t (*block_read_fn)(void *context, uint64_t lba, uint32_t count, void *buffer);
typedef block_status_t (*block_write_fn)(void *context, uint64_t lba, uint32_t count, const void *buffer);
typedef block_status_t (*block_flush_fn)(void *context);

typedef struct {
    char name[BLOCK_NAME_MAX + 1u];
    uint32_t sector_size;
    uint64_t sector_count;
    int writable;
    void *context;
    block_read_fn read;
    block_write_fn write;
    block_flush_fn flush;
} block_device_t;

void block_registry_reset(void);
block_status_t block_register(const block_device_t *device, uint32_t *index_out);
uint32_t block_count(void);
const block_device_t *block_get(uint32_t index);
block_status_t block_read(const block_device_t *device, uint64_t lba, uint32_t count, void *buffer);
block_status_t block_write(const block_device_t *device, uint64_t lba, uint32_t count, const void *buffer);
block_status_t block_flush(const block_device_t *device);
const char *block_status_string(block_status_t status);

#endif
