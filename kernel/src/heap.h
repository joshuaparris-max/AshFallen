#ifndef JOSHOS_HEAP_H
#define JOSHOS_HEAP_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    HEAP_OK = 0,
    HEAP_NOT_INITIALISED,
    HEAP_BAD_ARGUMENT,
    HEAP_OUT_OF_MEMORY,
    HEAP_MAP_FAILED,
    HEAP_INVALID_FREE,
    HEAP_DOUBLE_FREE,
    HEAP_CORRUPT
} heap_status_t;

typedef struct {
    uint64_t mapped_bytes;
    uint64_t allocated_bytes;
    uint64_t free_bytes;
    uint64_t allocation_count;
} heap_stats_t;

heap_status_t heap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
heap_status_t kfree(void *pointer);
heap_stats_t heap_stats(void);
int heap_self_test(void);
const char *heap_status_string(heap_status_t status);

#endif
