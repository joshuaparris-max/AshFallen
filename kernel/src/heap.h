#ifndef JOSHOS_HEAP_H
#define JOSHOS_HEAP_H

#include "boot.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HEAP_OK = 0,
    HEAP_BAD_ARGUMENT,
    HEAP_NOT_INITIALISED,
    HEAP_OUT_OF_MEMORY,
    HEAP_INVALID_POINTER,
    HEAP_DOUBLE_FREE,
    HEAP_CORRUPT
} heap_status_t;

typedef void *(*heap_grow_fn)(
    size_t minimum_bytes,
    size_t *actual_bytes_out,
    void *context
);

typedef struct {
    uint64_t total_region_bytes;
    uint64_t free_payload_bytes;
    uint64_t allocated_payload_bytes;
    uint64_t active_allocations;
    uint64_t total_allocations;
    uint64_t allocation_failures;
    uint32_t region_count;
} heap_stats_t;

heap_status_t heap_init(heap_grow_fn grow, void *context);
heap_status_t heap_kernel_init(const boot_context_t *boot);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
heap_status_t kfree(void *pointer);
heap_stats_t heap_stats(void);
int heap_self_test(void);
const char *heap_status_string(heap_status_t status);

#endif
