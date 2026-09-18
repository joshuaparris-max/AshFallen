#ifndef JOSHOS_PMM_H
#define JOSHOS_PMM_H

#include "boot.h"
#include <stdint.h>

#define PMM_PAGE_SIZE UINT64_C(4096)

typedef enum {
    PMM_OK = 0,
    PMM_BAD_ARGUMENT,
    PMM_NO_MEMORY,
    PMM_RANGE_CAPACITY,
    PMM_INVALID_FREE,
    PMM_DOUBLE_FREE
} pmm_status_t;

typedef struct {
    uint64_t total_frames;
    uint64_t free_frames;
    uint64_t allocated_frames;
    uint32_t free_range_count;
} pmm_stats_t;

pmm_status_t pmm_init(const boot_context_t *boot);
uint64_t pmm_alloc_frame(void);
pmm_status_t pmm_free_frame(uint64_t physical_address);
pmm_stats_t pmm_stats(void);
int pmm_self_test(uint32_t frame_count);
const char *pmm_status_string(pmm_status_t status);

#endif
