#include "paging.h"
#include "pmm.h"
#include <stdint.h>

#define PAGE_SIZE UINT64_C(0x1000)
#define HUGE_PAGE_SIZE UINT64_C(0x200000)
#define PAGE_MASK UINT64_C(0x000ffffffffff000)
#define PAGE_PRESENT UINT64_C(0x001)
#define PAGE_RW UINT64_C(0x002)
#define PAGE_PWT UINT64_C(0x008)
#define PAGE_PCD UINT64_C(0x010)
#define PAGE_PS UINT64_C(0x080)
#define DIRECT_MAP_MAX UINT64_C(0x10000000000)
#define MMIO_VIRTUAL_BASE UINT64_C(0xffffc00000000000)
#define MMIO_VIRTUAL_LIMIT UINT64_C(0xffffc00040000000)

static const boot_context_t *active_boot;
static uint64_t active_root_phys;
static uint64_t next_mmio_virtual = MMIO_VIRTUAL_BASE;

static void zero_page(void *page) {
    uint64_t *words = (uint64_t *)page;
    for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); ++i) words[i] = 0;
}

static uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1u);
}

static int align_up(uint64_t value, uint64_t alignment, uint64_t *out) {
    if (!out || alignment == 0) return 0;
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

static void *phys_ptr(const boot_context_t *boot, uint64_t phys) {
    if (!boot || UINT64_MAX - boot->physical_memory_offset < phys) return 0;
    return (void *)(uintptr_t)(boot->physical_memory_offset + phys);
}

static paging_status_t alloc_table(const boot_context_t *boot, uint64_t *phys_out) {
    if (!phys_out) return PAGING_BAD_ARGUMENT;
    uint64_t phys = pmm_alloc_frame();
    if (phys == UINT64_MAX) return PAGING_NO_MEMORY;

    void *pointer = phys_ptr(boot, phys);
    if (!pointer) return PAGING_UNSUPPORTED_LAYOUT;
    zero_page(pointer);
    *phys_out = phys;
    return PAGING_OK;
}

static paging_status_t ensure_table(
    const boot_context_t *boot,
    uint64_t table_phys,
    uint16_t index,
    uint64_t *next_phys_out
) {
    uint64_t *table = (uint64_t *)phys_ptr(boot, table_phys);
    if (!table || !next_phys_out) return PAGING_UNSUPPORTED_LAYOUT;

    uint64_t entry = table[index];
    if (entry & PAGE_PRESENT) {
        if (entry & PAGE_PS) return PAGING_MAPPING_CONFLICT;
        *next_phys_out = entry & PAGE_MASK;
        return PAGING_OK;
    }

    uint64_t next_phys;
    paging_status_t status = alloc_table(boot, &next_phys);
    if (status != PAGING_OK) return status;
    table[index] = next_phys | PAGE_PRESENT | PAGE_RW;
    *next_phys_out = next_phys;
    return PAGING_OK;
}

static paging_status_t map_2m(
    const boot_context_t *boot,
    uint64_t root_phys,
    uint64_t virt,
    uint64_t phys
) {
    if ((virt & (HUGE_PAGE_SIZE - 1u)) != 0 ||
        (phys & (HUGE_PAGE_SIZE - 1u)) != 0) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint64_t pdpt_phys;
    paging_status_t status = ensure_table(
        boot, root_phys, (uint16_t)((virt >> 39) & 0x1ffu), &pdpt_phys);
    if (status != PAGING_OK) return status;

    uint64_t pd_phys;
    status = ensure_table(
        boot, pdpt_phys, (uint16_t)((virt >> 30) & 0x1ffu), &pd_phys);
    if (status != PAGING_OK) return status;

    uint64_t *pd = (uint64_t *)phys_ptr(boot, pd_phys);
    if (!pd) return PAGING_UNSUPPORTED_LAYOUT;
    uint16_t index = (uint16_t)((virt >> 21) & 0x1ffu);
    uint64_t wanted = phys | PAGE_PRESENT | PAGE_RW | PAGE_PS;
    if ((pd[index] & PAGE_PRESENT) && pd[index] != wanted) {
        return PAGING_MAPPING_CONFLICT;
    }
    pd[index] = wanted;
    return PAGING_OK;
}

static paging_status_t map_4k_flags(
    const boot_context_t *boot,
    uint64_t root_phys,
    uint64_t virt,
    uint64_t phys,
    uint64_t extra_flags
) {
    if ((virt & (PAGE_SIZE - 1u)) != 0 ||
        (phys & (PAGE_SIZE - 1u)) != 0) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint64_t pdpt_phys;
    paging_status_t status = ensure_table(
        boot, root_phys, (uint16_t)((virt >> 39) & 0x1ffu), &pdpt_phys);
    if (status != PAGING_OK) return status;

    uint64_t pd_phys;
    status = ensure_table(
        boot, pdpt_phys, (uint16_t)((virt >> 30) & 0x1ffu), &pd_phys);
    if (status != PAGING_OK) return status;

    uint64_t pt_phys;
    status = ensure_table(
        boot, pd_phys, (uint16_t)((virt >> 21) & 0x1ffu), &pt_phys);
    if (status != PAGING_OK) return status;

    uint64_t *pt = (uint64_t *)phys_ptr(boot, pt_phys);
    if (!pt) return PAGING_UNSUPPORTED_LAYOUT;
    uint16_t index = (uint16_t)((virt >> 12) & 0x1ffu);
    uint64_t wanted = phys | PAGE_PRESENT | PAGE_RW | extra_flags;
    if ((pt[index] & PAGE_PRESENT) && pt[index] != wanted) {
        return PAGING_MAPPING_CONFLICT;
    }
    pt[index] = wanted;
    return PAGING_OK;
}

static paging_status_t map_4k(
    const boot_context_t *boot,
    uint64_t root_phys,
    uint64_t virt,
    uint64_t phys
) {
    return map_4k_flags(boot, root_phys, virt, phys, 0);
}

static int physical_map_limit(const boot_context_t *boot, uint64_t *limit_out) {
    if (!boot || !limit_out) return 0;
    uint64_t limit = 0;

    for (uint32_t i = 0; i < boot->memory_map_count; ++i) {
        const boot_memory_region_t *region = &boot->memory_map[i];
        if (region->type != BOOT_MEMORY_USABLE || region->length == 0 ||
            UINT64_MAX - region->base < region->length) {
            continue;
        }
        uint64_t end = region->base + region->length;
        if (end > limit) limit = end;
    }

    if (boot->framebuffer_phys_end > limit) limit = boot->framebuffer_phys_end;
    if (boot->kernel_phys_end > limit) limit = boot->kernel_phys_end;
    if (limit == 0 || limit > DIRECT_MAP_MAX) return 0;
    return align_up(limit, HUGE_PAGE_SIZE, limit_out);
}

paging_status_t paging_init(const boot_context_t *boot, uint64_t *root_phys_out) {
    if (!boot || !root_phys_out ||
        boot->kernel_phys_start >= boot->kernel_phys_end ||
        boot->kernel_virt_start >= boot->kernel_virt_end) {
        return PAGING_BAD_ARGUMENT;
    }

    if ((boot->physical_memory_offset & (HUGE_PAGE_SIZE - 1u)) != 0) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint64_t direct_limit;
    if (!physical_map_limit(boot, &direct_limit)) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }
    if (UINT64_MAX - boot->physical_memory_offset < direct_limit - 1u) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint64_t root_phys;
    paging_status_t status = alloc_table(boot, &root_phys);
    if (status != PAGING_OK) return status;

    for (uint64_t phys = 0; phys < direct_limit; phys += HUGE_PAGE_SIZE) {
        status = map_2m(
            boot, root_phys, boot->physical_memory_offset + phys, phys);
        if (status != PAGING_OK) return status;
    }

    uint64_t phys_page = align_down(boot->kernel_phys_start, PAGE_SIZE);
    uint64_t virt_page = align_down(boot->kernel_virt_start, PAGE_SIZE);
    if ((boot->kernel_phys_start & (PAGE_SIZE - 1u)) !=
        (boot->kernel_virt_start & (PAGE_SIZE - 1u))) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint64_t kernel_bytes = boot->kernel_phys_end - phys_page;
    uint64_t mapped_bytes;
    if (!align_up(kernel_bytes, PAGE_SIZE, &mapped_bytes)) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    for (uint64_t offset = 0; offset < mapped_bytes; offset += PAGE_SIZE) {
        if (UINT64_MAX - phys_page < offset || UINT64_MAX - virt_page < offset) {
            return PAGING_UNSUPPORTED_LAYOUT;
        }
        status = map_4k(
            boot, root_phys, virt_page + offset, phys_page + offset);
        if (status != PAGING_OK) return status;
    }

    active_boot = boot;
    active_root_phys = root_phys;
    next_mmio_virtual = MMIO_VIRTUAL_BASE;
    *root_phys_out = root_phys;
    return PAGING_OK;
}

paging_status_t paging_map_mmio(uint64_t physical_address, uint64_t length, void **virtual_out) {
    if (!active_boot || active_root_phys == 0 || !virtual_out || length == 0) {
        return PAGING_BAD_ARGUMENT;
    }
    uint64_t phys_page = align_down(physical_address, PAGE_SIZE);
    uint64_t offset = physical_address - phys_page;
    if (UINT64_MAX - offset < length) return PAGING_UNSUPPORTED_LAYOUT;

    uint64_t bytes;
    if (!align_up(offset + length, PAGE_SIZE, &bytes)) return PAGING_UNSUPPORTED_LAYOUT;
    if (next_mmio_virtual > MMIO_VIRTUAL_LIMIT ||
        bytes > MMIO_VIRTUAL_LIMIT - next_mmio_virtual) return PAGING_UNSUPPORTED_LAYOUT;

    uint64_t base = next_mmio_virtual;
    for (uint64_t mapped = 0; mapped < bytes; mapped += PAGE_SIZE) {
        if (UINT64_MAX - phys_page < mapped) return PAGING_UNSUPPORTED_LAYOUT;
        paging_status_t status = map_4k_flags(
            active_boot, active_root_phys, base + mapped, phys_page + mapped,
            PAGE_PWT | PAGE_PCD);
        if (status != PAGING_OK) return status;
        __asm__ volatile ("invlpg (%0)" :: "r"((void *)(uintptr_t)(base + mapped)) : "memory");
    }
    next_mmio_virtual += bytes;
    *virtual_out = (void *)(uintptr_t)(base + offset);
    return PAGING_OK;
}

const char *paging_status_string(paging_status_t status) {
    switch (status) {
        case PAGING_OK: return "ok";
        case PAGING_BAD_ARGUMENT: return "bad argument";
        case PAGING_NO_MEMORY: return "no memory for page tables";
        case PAGING_UNSUPPORTED_LAYOUT: return "unsupported physical/virtual layout";
        case PAGING_MAPPING_CONFLICT: return "page-table mapping conflict";
        default: return "unknown paging error";
    }
}
