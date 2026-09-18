#include "paging.h"
#include "pmm.h"
#include <stdint.h>

#define PAGE_SIZE UINT64_C(0x1000)
#define LARGE_PAGE_SIZE UINT64_C(0x200000)
#define ENTRIES 512u

#define PTE_PRESENT UINT64_C(0x001)
#define PTE_RW      UINT64_C(0x002)
#define PTE_PS      UINT64_C(0x080)
#define PTE_ADDR_MASK UINT64_C(0x000ffffffffff000)
#define PDE_2M_ADDR_MASK UINT64_C(0x000fffffffe00000)

typedef uint64_t page_table_t[ENTRIES];

static uint64_t physical_offset;
static uint64_t root_phys;
static uint64_t mapped_phys_limit;

static uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1u);
}

static int align_up(uint64_t value, uint64_t alignment, uint64_t *out) {
    if (!out || alignment == 0 || (alignment & (alignment - 1u)) != 0) return 0;
    uint64_t mask = alignment - 1u;
    if ((value & mask) == 0) {
        *out = value;
        return 1;
    }
    uint64_t add = alignment - (value & mask);
    if (UINT64_MAX - value < add) return 0;
    *out = value + add;
    return 1;
}

static int canonical(uint64_t address) {
    uint64_t upper = address >> 48;
    uint64_t sign = (address >> 47) & 1u;
    return sign ? upper == UINT64_C(0xffff) : upper == 0;
}

static page_table_t *table_ptr(uint64_t physical) {
    if (physical > UINT64_MAX - physical_offset) return 0;
    return (page_table_t *)(uintptr_t)(physical_offset + physical);
}

static void zero_table(page_table_t *table) {
    if (!table) return;
    for (uint32_t i = 0; i < ENTRIES; ++i) (*table)[i] = 0;
}

static paging_status_t alloc_table(uint64_t *physical_out, page_table_t **virtual_out) {
    uint64_t physical = pmm_alloc_frame();
    if (physical == UINT64_MAX) return PAGING_NO_MEMORY;

    page_table_t *table = table_ptr(physical);
    if (!table) {
        (void)pmm_free_frame(physical);
        return PAGING_UNSUPPORTED_LAYOUT;
    }
    zero_table(table);
    *physical_out = physical;
    *virtual_out = table;
    return PAGING_OK;
}

static paging_status_t child_table(page_table_t *parent,
                                   uint32_t index,
                                   page_table_t **child_out) {
    uint64_t entry = (*parent)[index];
    if ((entry & PTE_PRESENT) != 0) {
        if ((entry & PTE_PS) != 0) return PAGING_MAP_CONFLICT;
        page_table_t *child = table_ptr(entry & PTE_ADDR_MASK);
        if (!child) return PAGING_UNSUPPORTED_LAYOUT;
        *child_out = child;
        return PAGING_OK;
    }

    uint64_t child_phys;
    page_table_t *child;
    paging_status_t status = alloc_table(&child_phys, &child);
    if (status != PAGING_OK) return status;
    (*parent)[index] = child_phys | PTE_PRESENT | PTE_RW;
    *child_out = child;
    return PAGING_OK;
}

static paging_status_t map_2m(page_table_t *pml4,
                              uint64_t virtual,
                              uint64_t physical) {
    if ((virtual & (LARGE_PAGE_SIZE - 1u)) != 0 ||
        (physical & (LARGE_PAGE_SIZE - 1u)) != 0 ||
        !canonical(virtual)) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint32_t i4 = (uint32_t)((virtual >> 39) & 0x1ffu);
    uint32_t i3 = (uint32_t)((virtual >> 30) & 0x1ffu);
    uint32_t i2 = (uint32_t)((virtual >> 21) & 0x1ffu);

    page_table_t *pdpt;
    paging_status_t status = child_table(pml4, i4, &pdpt);
    if (status != PAGING_OK) return status;

    page_table_t *pd;
    status = child_table(pdpt, i3, &pd);
    if (status != PAGING_OK) return status;

    uint64_t existing = (*pd)[i2];
    uint64_t wanted = (physical & PDE_2M_ADDR_MASK) |
                      PTE_PRESENT | PTE_RW | PTE_PS;
    if ((existing & PTE_PRESENT) != 0 && existing != wanted) {
        return PAGING_MAP_CONFLICT;
    }
    (*pd)[i2] = wanted;
    return PAGING_OK;
}

static paging_status_t map_4k(page_table_t *pml4,
                              uint64_t virtual,
                              uint64_t physical) {
    if ((virtual & (PAGE_SIZE - 1u)) != 0 ||
        (physical & (PAGE_SIZE - 1u)) != 0 ||
        !canonical(virtual)) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint32_t i4 = (uint32_t)((virtual >> 39) & 0x1ffu);
    uint32_t i3 = (uint32_t)((virtual >> 30) & 0x1ffu);
    uint32_t i2 = (uint32_t)((virtual >> 21) & 0x1ffu);
    uint32_t i1 = (uint32_t)((virtual >> 12) & 0x1ffu);

    page_table_t *pdpt;
    paging_status_t status = child_table(pml4, i4, &pdpt);
    if (status != PAGING_OK) return status;

    page_table_t *pd;
    status = child_table(pdpt, i3, &pd);
    if (status != PAGING_OK) return status;

    page_table_t *pt;
    status = child_table(pd, i2, &pt);
    if (status != PAGING_OK) return status;

    uint64_t wanted = (physical & PTE_ADDR_MASK) | PTE_PRESENT | PTE_RW;
    uint64_t existing = (*pt)[i1];
    if ((existing & PTE_PRESENT) != 0 && existing != wanted) {
        return PAGING_MAP_CONFLICT;
    }
    (*pt)[i1] = wanted;
    return PAGING_OK;
}

static uint64_t framebuffer_phys(const boot_context_t *boot) {
    uint64_t address = (uint64_t)(uintptr_t)boot->framebuffer.address;
    if (boot->physical_memory_offset != 0 &&
        address >= boot->physical_memory_offset) {
        return address - boot->physical_memory_offset;
    }
    return address;
}

static paging_status_t physical_span(const boot_context_t *boot,
                                     uint64_t *limit_out) {
    uint64_t limit = 0;
    for (uint32_t i = 0; i < boot->memory_map_count; ++i) {
        const boot_memory_region_t *r = &boot->memory_map[i];
        if (r->length == 0) continue;
        if (UINT64_MAX - r->base < r->length) return PAGING_BAD_ARGUMENT;
        uint64_t end = r->base + r->length;
        if (end > limit) limit = end;
    }

    uint64_t fb_phys = framebuffer_phys(boot);
    if (boot->framebuffer.pitch != 0 && boot->framebuffer.height != 0) {
        if (boot->framebuffer.height > UINT64_MAX / boot->framebuffer.pitch) {
            return PAGING_BAD_ARGUMENT;
        }
        uint64_t bytes = boot->framebuffer.pitch * boot->framebuffer.height;
        if (UINT64_MAX - fb_phys < bytes) return PAGING_BAD_ARGUMENT;
        uint64_t fb_end = fb_phys + bytes;
        if (fb_end > limit) limit = fb_end;
    }

    if (boot->physical_memory_limit != BOOT_PHYSICAL_UNLIMITED &&
        limit > boot->physical_memory_limit) {
        limit = boot->physical_memory_limit;
    }

    if (!align_up(limit, LARGE_PAGE_SIZE, &limit) || limit == 0) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    /* One PML4 slot spans 512 GiB; keep this first direct map deliberately bounded. */
    if (limit > UINT64_C(0x8000000000)) return PAGING_UNSUPPORTED_LAYOUT;
    *limit_out = limit;
    return PAGING_OK;
}

static paging_status_t map_physical_window(page_table_t *pml4,
                                           const boot_context_t *boot,
                                           uint64_t limit) {
    if ((boot->physical_memory_offset & (LARGE_PAGE_SIZE - 1u)) != 0) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    for (uint64_t physical = 0; physical < limit; physical += LARGE_PAGE_SIZE) {
        if (physical > UINT64_MAX - boot->physical_memory_offset) {
            return PAGING_UNSUPPORTED_LAYOUT;
        }
        uint64_t virtual = boot->physical_memory_offset + physical;
        paging_status_t status = map_2m(pml4, virtual, physical);
        if (status != PAGING_OK) return status;
    }
    return PAGING_OK;
}

static paging_status_t map_kernel(page_table_t *pml4,
                                  const boot_context_t *boot) {
    if (boot->kernel_phys_start >= boot->kernel_phys_end ||
        boot->kernel_virt_start >= boot->kernel_virt_end) {
        return PAGING_BAD_ARGUMENT;
    }

    uint64_t physical = align_down(boot->kernel_phys_start, PAGE_SIZE);
    uint64_t virtual = align_down(boot->kernel_virt_start, PAGE_SIZE);
    uint64_t end;
    if (!align_up(boot->kernel_virt_end, PAGE_SIZE, &end)) {
        return PAGING_BAD_ARGUMENT;
    }

    if ((boot->kernel_phys_start & (PAGE_SIZE - 1u)) !=
        (boot->kernel_virt_start & (PAGE_SIZE - 1u))) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    while (virtual < end) {
        paging_status_t status = map_4k(pml4, virtual, physical);
        if (status != PAGING_OK) return status;
        if (UINT64_MAX - virtual < PAGE_SIZE ||
            UINT64_MAX - physical < PAGE_SIZE) {
            return PAGING_BAD_ARGUMENT;
        }
        virtual += PAGE_SIZE;
        physical += PAGE_SIZE;
    }
    return PAGING_OK;
}

static int self_test(void) {
    uint64_t frame = pmm_alloc_frame();
    if (frame == UINT64_MAX || frame >= mapped_phys_limit) return 0;

    volatile uint64_t *memory =
        (volatile uint64_t *)(uintptr_t)(physical_offset + frame);
    const uint64_t pattern = UINT64_C(0x4a4f534850414745);
    *memory = pattern;
    int ok = *memory == pattern;
    if (pmm_free_frame(frame) != PMM_OK) return 0;
    return ok;
}

paging_status_t paging_init(boot_context_t *boot) {
    if (!boot || boot->memory_map_count == 0 ||
        !canonical(boot->kernel_virt_start) ||
        !canonical(boot->physical_memory_offset)) {
        return PAGING_BAD_ARGUMENT;
    }

    physical_offset = boot->physical_memory_offset;

    uint64_t limit = 0;
    paging_status_t status = physical_span(boot, &limit);
    if (status != PAGING_OK) return status;

    uint64_t new_root_phys;
    page_table_t *new_root;
    status = alloc_table(&new_root_phys, &new_root);
    if (status != PAGING_OK) return status;

    status = map_physical_window(new_root, boot, limit);
    if (status != PAGING_OK) return status;

    status = map_kernel(new_root, boot);
    if (status != PAGING_OK) return status;

    /*
     * The current stack, boot context, framebuffer, and page-table frames all
     * remain reachable at the same direct/identity-map addresses. Therefore a
     * CR3 switch can safely happen in-place without a temporary trampoline.
     */
    __asm__ volatile ("movq %0, %%cr3" : : "r"(new_root_phys) : "memory");

    root_phys = new_root_phys;
    mapped_phys_limit = limit;

    if (!self_test()) return PAGING_SELF_TEST_FAILED;
    return PAGING_OK;
}

uint64_t paging_root_phys(void) {
    return root_phys;
}

void *paging_phys_to_virt(uint64_t physical_address) {
    if (physical_address >= mapped_phys_limit ||
        physical_address > UINT64_MAX - physical_offset) return 0;
    return (void *)(uintptr_t)(physical_offset + physical_address);
}

const char *paging_status_string(paging_status_t status) {
    switch (status) {
        case PAGING_OK: return "ok";
        case PAGING_BAD_ARGUMENT: return "bad argument";
        case PAGING_NO_MEMORY: return "out of page-table frames";
        case PAGING_UNSUPPORTED_LAYOUT: return "unsupported address layout";
        case PAGING_MAP_CONFLICT: return "page-table mapping conflict";
        case PAGING_SELF_TEST_FAILED: return "paging self-test failed";
        default: return "unknown paging error";
    }
}
