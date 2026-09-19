#include "paging.h"
#include "pmm.h"
#include <stdint.h>

#define PAGE_SIZE UINT64_C(0x1000)
#define HUGE_PAGE_SIZE UINT64_C(0x200000)
#define PAGE_MASK UINT64_C(0x000ffffffffff000)
#define PAGE_PRESENT UINT64_C(0x001)
#define PAGE_RW UINT64_C(0x002)
#define PAGE_USER UINT64_C(0x004)
#define PAGE_PWT UINT64_C(0x008)
#define PAGE_PCD UINT64_C(0x010)
#define PAGE_PS UINT64_C(0x080)
#define PAGE_NX (UINT64_C(1) << 63)

#define DIRECT_MAP_MAX UINT64_C(0x10000000000)
#define MMIO_BASE UINT64_C(0xffffc00000000000)
#define MMIO_LIMIT UINT64_C(0xffffc00040000000)
#define TEMP_MAP_BASE UINT64_C(0xffffd00000000000)
#define USER_MAP_BASE UINT64_C(0x0000400000000000)
#define USER_MAP_LIMIT UINT64_C(0x0000800000000000)

extern char __text_start[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];
extern char __data_start[];
extern char __data_end[];
extern char __paging_guard_low_start[];
extern char __paging_stack_start[];
extern char __paging_stack_end[];
extern char __paging_guard_high_start[];
extern char __paging_guard_high_end[];

static const boot_context_t *active_boot;
static paging_address_space_t kernel_space;
static uint64_t direct_limit;
static uint64_t mmio_next = MMIO_BASE;
static int temp_mapped;

static void zero_page(void *page) {
    uint64_t *words = (uint64_t *)page;
    for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); ++i) words[i] = 0;
}

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

static paging_status_t pte_for_virtual(
    const boot_context_t *boot,
    uint64_t root_phys,
    uint64_t virt,
    int create,
    uint64_t **pte_out
) {
    if (!boot || !pte_out) return PAGING_BAD_ARGUMENT;

    uint64_t *pml4 = (uint64_t *)phys_ptr(boot, root_phys);
    if (!pml4) return PAGING_UNSUPPORTED_LAYOUT;

    uint16_t pml4_index = (uint16_t)((virt >> 39) & 0x1ffu);
    uint16_t pdpt_index = (uint16_t)((virt >> 30) & 0x1ffu);
    uint16_t pd_index = (uint16_t)((virt >> 21) & 0x1ffu);
    uint16_t pt_index = (uint16_t)((virt >> 12) & 0x1ffu);

    uint64_t pdpt_phys;
    if (create) {
        paging_status_t status = ensure_table(
            boot, root_phys, pml4_index, &pdpt_phys);
        if (status != PAGING_OK) return status;
    } else {
        uint64_t entry = pml4[pml4_index];
        if ((entry & PAGE_PRESENT) == 0 || (entry & PAGE_PS) != 0) {
            return PAGING_MAPPING_CONFLICT;
        }
        pdpt_phys = entry & PAGE_MASK;
    }

    uint64_t *pdpt = (uint64_t *)phys_ptr(boot, pdpt_phys);
    if (!pdpt) return PAGING_UNSUPPORTED_LAYOUT;

    uint64_t pd_phys;
    if (create) {
        paging_status_t status = ensure_table(
            boot, pdpt_phys, pdpt_index, &pd_phys);
        if (status != PAGING_OK) return status;
    } else {
        uint64_t entry = pdpt[pdpt_index];
        if ((entry & PAGE_PRESENT) == 0 || (entry & PAGE_PS) != 0) {
            return PAGING_MAPPING_CONFLICT;
        }
        pd_phys = entry & PAGE_MASK;
    }

    uint64_t *pd = (uint64_t *)phys_ptr(boot, pd_phys);
    if (!pd) return PAGING_UNSUPPORTED_LAYOUT;
    if (pd[pd_index] & PAGE_PS) return PAGING_MAPPING_CONFLICT;

    uint64_t pt_phys;
    if (create) {
        paging_status_t status = ensure_table(
            boot, pd_phys, pd_index, &pt_phys);
        if (status != PAGING_OK) return status;
    } else {
        uint64_t entry = pd[pd_index];
        if ((entry & PAGE_PRESENT) == 0) return PAGING_MAPPING_CONFLICT;
        pt_phys = entry & PAGE_MASK;
    }

    uint64_t *pt = (uint64_t *)phys_ptr(boot, pt_phys);
    if (!pt) return PAGING_UNSUPPORTED_LAYOUT;
    *pte_out = &pt[pt_index];
    return PAGING_OK;
}

static paging_status_t map_2m_flags(
    const boot_context_t *boot,
    uint64_t root_phys,
    uint64_t virt,
    uint64_t phys,
    uint64_t flags
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
    uint64_t wanted = phys | PAGE_PRESENT | PAGE_PS | flags;
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
    uint64_t flags,
    int replace
) {
    if ((virt & (PAGE_SIZE - 1u)) != 0 ||
        (phys & (PAGE_SIZE - 1u)) != 0) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint64_t *pte;
    paging_status_t status =
        pte_for_virtual(boot, root_phys, virt, 1, &pte);
    if (status != PAGING_OK) return status;

    uint64_t wanted = phys | PAGE_PRESENT | flags;
    if ((*pte & PAGE_PRESENT) && *pte != wanted && !replace) {
        return PAGING_MAPPING_CONFLICT;
    }
    *pte = wanted;
    return PAGING_OK;
}

static paging_status_t ensure_user_table(
    const boot_context_t *boot,
    uint64_t table_phys,
    uint16_t index,
    uint64_t *next_phys_out
) {
    uint64_t *table = (uint64_t *)phys_ptr(boot, table_phys);
    if (!table || !next_phys_out) return PAGING_UNSUPPORTED_LAYOUT;

    uint64_t entry = table[index];
    if (entry & PAGE_PRESENT) {
        if ((entry & PAGE_PS) != 0 || (entry & PAGE_USER) == 0) {
            return PAGING_MAPPING_CONFLICT;
        }
        *next_phys_out = entry & PAGE_MASK;
        return PAGING_OK;
    }

    uint64_t next_phys;
    paging_status_t status = alloc_table(boot, &next_phys);
    if (status != PAGING_OK) return status;

    table[index] = next_phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    *next_phys_out = next_phys;
    return PAGING_OK;
}

static paging_status_t map_user_4k(
    const boot_context_t *boot,
    uint64_t root_phys,
    uint64_t virt,
    uint64_t phys,
    int writable,
    int executable
) {
    if ((virt & (PAGE_SIZE - 1u)) != 0 ||
        (phys & (PAGE_SIZE - 1u)) != 0 ||
        virt < USER_MAP_BASE || virt >= USER_MAP_LIMIT) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint64_t pdpt_phys;
    paging_status_t status = ensure_user_table(
        boot, root_phys, (uint16_t)((virt >> 39) & 0x1ffu), &pdpt_phys);
    if (status != PAGING_OK) return status;

    uint64_t pd_phys;
    status = ensure_user_table(
        boot, pdpt_phys, (uint16_t)((virt >> 30) & 0x1ffu), &pd_phys);
    if (status != PAGING_OK) return status;

    uint64_t pt_phys;
    status = ensure_user_table(
        boot, pd_phys, (uint16_t)((virt >> 21) & 0x1ffu), &pt_phys);
    if (status != PAGING_OK) return status;

    uint64_t *pt = (uint64_t *)phys_ptr(boot, pt_phys);
    if (!pt) return PAGING_UNSUPPORTED_LAYOUT;

    uint16_t index = (uint16_t)((virt >> 12) & 0x1ffu);
    uint64_t wanted = phys | PAGE_PRESENT | PAGE_USER;
    if (writable) wanted |= PAGE_RW;
    if (!executable) wanted |= PAGE_NX;

    if (pt[index] & PAGE_PRESENT) return PAGING_MAPPING_CONFLICT;
    pt[index] = wanted;
    return PAGING_OK;
}

static int ranges_overlap(
    uint64_t a_start,
    uint64_t a_end,
    uint64_t b_start,
    uint64_t b_end
) {
    return a_start < b_end && b_start < a_end;
}

static int kernel_phys_for_virtual(
    const boot_context_t *boot,
    uint64_t virt,
    uint64_t *phys_out
) {
    if (!boot || !phys_out ||
        virt < boot->kernel_virt_start || virt >= boot->kernel_virt_end) {
        return 0;
    }
    uint64_t offset = virt - boot->kernel_virt_start;
    if (UINT64_MAX - boot->kernel_phys_start < offset) return 0;
    *phys_out = boot->kernel_phys_start + offset;
    return 1;
}

static int kernel_section_phys_range(
    const boot_context_t *boot,
    const char *start_symbol,
    const char *end_symbol,
    uint64_t *start_out,
    uint64_t *end_out
) {
    uint64_t start_virt = (uint64_t)(uintptr_t)start_symbol;
    uint64_t end_virt = (uint64_t)(uintptr_t)end_symbol;
    if (!start_out || !end_out || start_virt >= end_virt) return 0;
    if (!kernel_phys_for_virtual(boot, start_virt, start_out)) return 0;

    uint64_t last_phys;
    if (!kernel_phys_for_virtual(boot, end_virt - 1u, &last_phys) ||
        last_phys == UINT64_MAX) {
        return 0;
    }
    *end_out = last_phys + 1u;
    return 1;
}

static uint64_t direct_flags_for_page(
    const boot_context_t *boot,
    uint64_t phys
) {
    uint64_t flags = PAGE_RW | PAGE_NX;
    uint64_t text_start;
    uint64_t text_end;
    uint64_t ro_start;
    uint64_t ro_end;

    if (kernel_section_phys_range(
            boot, __text_start, __text_end, &text_start, &text_end) &&
        phys >= align_down(text_start, PAGE_SIZE) &&
        phys < align_down(text_end + PAGE_SIZE - 1u, PAGE_SIZE)) {
        flags = PAGE_NX;
    }

    if (kernel_section_phys_range(
            boot, __rodata_start, __rodata_end, &ro_start, &ro_end) &&
        phys >= align_down(ro_start, PAGE_SIZE) &&
        phys < align_down(ro_end + PAGE_SIZE - 1u, PAGE_SIZE)) {
        flags = PAGE_NX;
    }

    uint64_t page_end = phys + PAGE_SIZE;
    if (ranges_overlap(
            phys, page_end,
            boot->framebuffer_phys_start,
            boot->framebuffer_phys_end)) {
        flags = PAGE_RW | PAGE_NX | PAGE_PCD | PAGE_PWT;
    }

    return flags;
}

static int chunk_needs_4k_mapping(
    const boot_context_t *boot,
    uint64_t phys
) {
    uint64_t end = phys + HUGE_PAGE_SIZE;
    if (ranges_overlap(
            phys, end, boot->kernel_phys_start, boot->kernel_phys_end)) {
        return 1;
    }
    if (ranges_overlap(
            phys, end,
            boot->framebuffer_phys_start,
            boot->framebuffer_phys_end)) {
        return 1;
    }
    return 0;
}

static paging_status_t map_direct_range(
    const boot_context_t *boot,
    uint64_t root_phys,
    uint64_t limit
) {
    for (uint64_t phys = 0; phys < limit; phys += HUGE_PAGE_SIZE) {
        uint64_t virt = boot->physical_memory_offset + phys;

        if (!chunk_needs_4k_mapping(boot, phys)) {
            paging_status_t status = map_2m_flags(
                boot, root_phys, virt, phys, PAGE_RW | PAGE_NX);
            if (status != PAGING_OK) return status;
            continue;
        }

        for (uint64_t offset = 0; offset < HUGE_PAGE_SIZE; offset += PAGE_SIZE) {
            uint64_t page_phys = phys + offset;
            if (page_phys >= limit) break;
            paging_status_t status = map_4k_flags(
                boot, root_phys,
                boot->physical_memory_offset + page_phys,
                page_phys,
                direct_flags_for_page(boot, page_phys),
                0
            );
            if (status != PAGING_OK) return status;
        }
    }
    return PAGING_OK;
}

static paging_status_t map_kernel_virtual_range(
    const boot_context_t *boot,
    uint64_t root_phys,
    uint64_t start_virt,
    uint64_t end_virt,
    uint64_t flags
) {
    if (start_virt >= end_virt) return PAGING_OK;

    uint64_t page_start = align_down(start_virt, PAGE_SIZE);
    uint64_t page_end;
    if (!align_up(end_virt, PAGE_SIZE, &page_end)) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    for (uint64_t virt = page_start; virt < page_end; virt += PAGE_SIZE) {
        uint64_t phys;
        if (!kernel_phys_for_virtual(boot, virt, &phys)) {
            return PAGING_UNSUPPORTED_LAYOUT;
        }
        paging_status_t status = map_4k_flags(
            boot, root_phys, virt, align_down(phys, PAGE_SIZE), flags, 0);
        if (status != PAGING_OK) return status;
    }
    return PAGING_OK;
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

    uint64_t new_direct_limit;
    if (!physical_map_limit(boot, &new_direct_limit)) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }
    if (UINT64_MAX - boot->physical_memory_offset < new_direct_limit - 1u) {
        return PAGING_UNSUPPORTED_LAYOUT;
    }

    uint64_t root_phys;
    paging_status_t status = alloc_table(boot, &root_phys);
    if (status != PAGING_OK) return status;

    status = map_direct_range(boot, root_phys, new_direct_limit);
    if (status != PAGING_OK) return status;

    uint64_t text_start = (uint64_t)(uintptr_t)__text_start;
    uint64_t text_end = (uint64_t)(uintptr_t)__text_end;
    uint64_t rodata_start = (uint64_t)(uintptr_t)__rodata_start;
    uint64_t rodata_end = (uint64_t)(uintptr_t)__rodata_end;
    uint64_t data_start = (uint64_t)(uintptr_t)__data_start;
    uint64_t data_end = (uint64_t)(uintptr_t)__data_end;
    uint64_t stack_start = (uint64_t)(uintptr_t)__paging_stack_start;
    uint64_t stack_end = (uint64_t)(uintptr_t)__paging_stack_end;

    status = map_kernel_virtual_range(
        boot, root_phys, boot->kernel_virt_start, text_start, PAGE_NX);
    if (status != PAGING_OK) return status;

    status = map_kernel_virtual_range(
        boot, root_phys, text_start, text_end, 0);
    if (status != PAGING_OK) return status;

    status = map_kernel_virtual_range(
        boot, root_phys, rodata_start, rodata_end, PAGE_NX);
    if (status != PAGING_OK) return status;

    status = map_kernel_virtual_range(
        boot, root_phys, data_start, data_end, PAGE_RW | PAGE_NX);
    if (status != PAGING_OK) return status;

    status = map_kernel_virtual_range(
        boot, root_phys, stack_start, stack_end, PAGE_RW | PAGE_NX);
    if (status != PAGING_OK) return status;

    active_boot = boot;
    kernel_space.root_phys = root_phys;
    direct_limit = new_direct_limit;
    mmio_next = MMIO_BASE;
    temp_mapped = 0;

    *root_phys_out = root_phys;
    return PAGING_OK;
}

const paging_address_space_t *paging_kernel_address_space(void) {
    return kernel_space.root_phys ? &kernel_space : 0;
}

uint64_t paging_current_root(void) {
    return kernel_space.root_phys;
}

static int query_root(
    const boot_context_t *boot,
    uint64_t root_phys,
    uint64_t virt,
    paging_mapping_t *mapping
) {
    if (!boot || !root_phys || !mapping) return 0;

    mapping->physical_address = 0;
    mapping->present = 0;
    mapping->writable = 0;
    mapping->executable = 0;
    mapping->huge = 0;
    mapping->cache_disabled = 0;
    mapping->user = 0;

    uint64_t *pml4 = (uint64_t *)phys_ptr(boot, root_phys);
    if (!pml4) return 0;
    uint64_t e1 = pml4[(virt >> 39) & 0x1ffu];
    if ((e1 & PAGE_PRESENT) == 0) return 1;

    uint64_t *pdpt = (uint64_t *)phys_ptr(boot, e1 & PAGE_MASK);
    if (!pdpt) return 0;
    uint64_t e2 = pdpt[(virt >> 30) & 0x1ffu];
    if ((e2 & PAGE_PRESENT) == 0) return 1;

    uint64_t *pd = (uint64_t *)phys_ptr(boot, e2 & PAGE_MASK);
    if (!pd) return 0;
    uint64_t e3 = pd[(virt >> 21) & 0x1ffu];
    if ((e3 & PAGE_PRESENT) == 0) return 1;

    if (e3 & PAGE_PS) {
        mapping->present = 1;
        mapping->huge = 1;
        mapping->writable = (e3 & PAGE_RW) != 0;
        mapping->executable = (e3 & PAGE_NX) == 0;
        mapping->cache_disabled = (e3 & PAGE_PCD) != 0;
        mapping->user = ((e1 & PAGE_USER) != 0) &&
                        ((e2 & PAGE_USER) != 0) &&
                        ((e3 & PAGE_USER) != 0);
        mapping->physical_address =
            (e3 & PAGE_MASK) + (virt & (HUGE_PAGE_SIZE - 1u));
        return 1;
    }

    uint64_t *pt = (uint64_t *)phys_ptr(boot, e3 & PAGE_MASK);
    if (!pt) return 0;
    uint64_t e4 = pt[(virt >> 12) & 0x1ffu];
    if ((e4 & PAGE_PRESENT) == 0) return 1;

    mapping->present = 1;
    mapping->writable = (e4 & PAGE_RW) != 0;
    mapping->executable = (e4 & PAGE_NX) == 0;
    mapping->cache_disabled = (e4 & PAGE_PCD) != 0;
    mapping->user = ((e1 & PAGE_USER) != 0) &&
                    ((e2 & PAGE_USER) != 0) &&
                    ((e3 & PAGE_USER) != 0) &&
                    ((e4 & PAGE_USER) != 0);
    mapping->physical_address =
        (e4 & PAGE_MASK) + (virt & (PAGE_SIZE - 1u));
    return 1;
}

int paging_query(uint64_t virtual_address, paging_mapping_t *mapping) {
    return query_root(
        active_boot, kernel_space.root_phys, virtual_address, mapping);
}

int paging_verify_kernel_layout(void) {
    if (!active_boot || !kernel_space.root_phys) return 0;

    paging_mapping_t text;
    paging_mapping_t rodata;
    paging_mapping_t data;
    paging_mapping_t stack;
    paging_mapping_t guard_low;
    paging_mapping_t guard_high;
    paging_mapping_t framebuffer;

    if (!paging_query((uint64_t)(uintptr_t)__text_start, &text) ||
        !paging_query((uint64_t)(uintptr_t)__rodata_start, &rodata) ||
        !paging_query((uint64_t)(uintptr_t)__data_start, &data) ||
        !paging_query((uint64_t)(uintptr_t)__paging_stack_start, &stack) ||
        !paging_query((uint64_t)(uintptr_t)__paging_guard_low_start, &guard_low) ||
        !paging_query((uint64_t)(uintptr_t)__paging_guard_high_start, &guard_high) ||
        !paging_query((uint64_t)(uintptr_t)active_boot->framebuffer.address, &framebuffer)) {
        return 0;
    }

    return text.present && !text.writable && text.executable &&
           rodata.present && !rodata.writable && !rodata.executable &&
           data.present && data.writable && !data.executable &&
           stack.present && stack.writable && !stack.executable &&
           !guard_low.present && !guard_high.present &&
           framebuffer.present && framebuffer.writable &&
           !framebuffer.executable;
}

paging_status_t paging_create_user_address_space(paging_address_space_t *space_out) {
    if (!active_boot || !kernel_space.root_phys || !space_out) {
        return PAGING_BAD_ARGUMENT;
    }

    uint64_t root_phys;
    paging_status_t status = alloc_table(active_boot, &root_phys);
    if (status != PAGING_OK) return status;

    uint64_t *kernel_root =
        (uint64_t *)phys_ptr(active_boot, kernel_space.root_phys);
    uint64_t *user_root = (uint64_t *)phys_ptr(active_boot, root_phys);
    if (!kernel_root || !user_root) return PAGING_UNSUPPORTED_LAYOUT;

    /*
     * Copy the kernel's supervisor mappings, then build U/S mappings only in
     * the dedicated user window. ensure_user_table() refuses to promote an
     * inherited supervisor branch to user-accessible.
     */
    for (uint32_t i = 0; i < 512u; ++i) user_root[i] = kernel_root[i];

    space_out->root_phys = root_phys;
    return PAGING_OK;
}

paging_status_t paging_map_user_page(const paging_address_space_t *space,
                                     uint64_t virtual_address,
                                     uint64_t physical_address,
                                     int writable,
                                     int executable) {
    if (!active_boot || !space || !space->root_phys) return PAGING_BAD_ARGUMENT;
    return map_user_4k(active_boot, space->root_phys, virtual_address,
                       physical_address, writable, executable);
}

int paging_query_address_space(const paging_address_space_t *space,
                               uint64_t virtual_address,
                               paging_mapping_t *mapping) {
    if (!active_boot || !space || !space->root_phys) return 0;
    return query_root(active_boot, space->root_phys, virtual_address, mapping);
}

int paging_user_range_accessible(uint64_t root_phys,
                                 uint64_t address,
                                 uint64_t length,
                                 int writable) {
    if (!active_boot || !root_phys || length == 0 ||
        address < USER_MAP_BASE || address >= USER_MAP_LIMIT ||
        UINT64_MAX - address < length ||
        address + length > USER_MAP_LIMIT) {
        return 0;
    }

    uint64_t first = align_down(address, PAGE_SIZE);
    uint64_t last = align_down(address + length - 1u, PAGE_SIZE);
    for (uint64_t page = first;; page += PAGE_SIZE) {
        paging_mapping_t mapping;
        if (!query_root(active_boot, root_phys, page, &mapping) ||
            !mapping.present || !mapping.user ||
            (writable && !mapping.writable)) {
            return 0;
        }

        if (page == last) break;
        if (UINT64_MAX - page < PAGE_SIZE) return 0;
    }

    return 1;
}

void *paging_direct_pointer(uint64_t physical_address) {
    if (!active_boot || physical_address >= direct_limit ||
        UINT64_MAX - active_boot->physical_memory_offset < physical_address) {
        return 0;
    }
    return (void *)(uintptr_t)(
        active_boot->physical_memory_offset + physical_address);
}

void *paging_map_mmio(uint64_t physical_address, uint64_t size) {
    if (!active_boot || !kernel_space.root_phys || size == 0) return 0;

    uint64_t phys_page = align_down(physical_address, PAGE_SIZE);
    uint64_t offset = physical_address - phys_page;
    uint64_t span;
    if (UINT64_MAX - offset < size ||
        !align_up(offset + size, PAGE_SIZE, &span) ||
        UINT64_MAX - mmio_next < span ||
        mmio_next + span > MMIO_LIMIT) {
        return 0;
    }

    uint64_t virt_page = mmio_next;
    for (uint64_t done = 0; done < span; done += PAGE_SIZE) {
        paging_status_t status = map_4k_flags(
            active_boot,
            kernel_space.root_phys,
            virt_page + done,
            phys_page + done,
            PAGE_RW | PAGE_NX | PAGE_PCD | PAGE_PWT,
            0
        );
        if (status != PAGING_OK) return 0;
        __asm__ volatile ("invlpg (%0)" :: "r"(virt_page + done) : "memory");
    }

    mmio_next += span;
    return (void *)(uintptr_t)(virt_page + offset);
}

void *paging_temp_map(uint64_t physical_address) {
    if (!active_boot || !kernel_space.root_phys) return 0;

    uint64_t phys_page = align_down(physical_address, PAGE_SIZE);
    uint64_t offset = physical_address - phys_page;
    paging_status_t status = map_4k_flags(
        active_boot,
        kernel_space.root_phys,
        TEMP_MAP_BASE,
        phys_page,
        PAGE_RW | PAGE_NX | PAGE_PCD,
        1
    );
    if (status != PAGING_OK) return 0;

    __asm__ volatile ("invlpg (%0)" :: "r"(TEMP_MAP_BASE) : "memory");
    temp_mapped = 1;
    return (void *)(uintptr_t)(TEMP_MAP_BASE + offset);
}

void paging_temp_unmap(void) {
    if (!active_boot || !kernel_space.root_phys || !temp_mapped) return;

    uint64_t *pte;
    if (pte_for_virtual(
            active_boot, kernel_space.root_phys, TEMP_MAP_BASE, 0, &pte) ==
        PAGING_OK) {
        *pte = 0;
        __asm__ volatile ("invlpg (%0)" :: "r"(TEMP_MAP_BASE) : "memory");
    }
    temp_mapped = 0;
}

const char *paging_status_string(paging_status_t status) {
    switch (status) {
        case PAGING_OK: return "ok";
        case PAGING_BAD_ARGUMENT: return "bad argument";
        case PAGING_NO_MEMORY: return "no memory for page tables";
        case PAGING_UNSUPPORTED_LAYOUT: return "unsupported physical/virtual layout";
        case PAGING_MAPPING_CONFLICT: return "page-table mapping conflict";
        case PAGING_WINDOW_EXHAUSTED: return "virtual mapping window exhausted";
        default: return "unknown paging error";
    }
}
