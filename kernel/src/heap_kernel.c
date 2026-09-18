#include "heap.h"
#include "paging.h"
#include "pmm.h"
#include <stdint.h>

#define KHEAP_MIN_GROW_PAGES 16u

static void *kernel_heap_grow(
    size_t minimum_bytes,
    size_t *actual_bytes_out,
    void *context
) {
    (void)context;
    if (!actual_bytes_out || minimum_bytes == 0) return 0;

    uint64_t pages =
        ((uint64_t)minimum_bytes + PMM_PAGE_SIZE - 1u) / PMM_PAGE_SIZE;
    if (pages < KHEAP_MIN_GROW_PAGES) pages = KHEAP_MIN_GROW_PAGES;
    if (pages > UINT32_MAX) return 0;

    uint64_t physical = pmm_alloc_frames((uint32_t)pages);
    if (physical == UINT64_MAX) return 0;

    void *virtual_address = paging_direct_pointer(physical);
    if (!virtual_address) {
        (void)pmm_free_frames(physical, (uint32_t)pages);
        return 0;
    }

    *actual_bytes_out = (size_t)(pages * PMM_PAGE_SIZE);
    return virtual_address;
}

heap_status_t heap_kernel_init(const boot_context_t *boot) {
    if (!boot || !paging_kernel_address_space()) return HEAP_BAD_ARGUMENT;
    return heap_init(kernel_heap_grow, 0);
}
