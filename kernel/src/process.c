#include "process.h"
#include "elf64.h"
#include "gdt.h"
#include <stdint.h>

#define PAGE_SIZE UINT64_C(4096)
#define PAGE_MASK (PAGE_SIZE - 1u)
#define KERNEL_PT_COUNT 8u
#define KERNEL_STACK_PAGES 4u
#define USER_STACK_PAGE (PROCESS_USER_REGION_PAGES - 1u)
#define USER_STACK_BASE (PROCESS_USER_STACK_TOP - PAGE_SIZE)

#define PTE_PRESENT UINT64_C(0x001)
#define PTE_WRITE   UINT64_C(0x002)
#define PTE_USER    UINT64_C(0x004)

typedef struct __attribute__((aligned(4096))) {
    uint64_t pml4[512];
    uint64_t user_pdpt[512];
    uint64_t user_pd[512];
    uint64_t user_pt[512];

    uint64_t kernel_pdpt[512];
    uint64_t kernel_pd[512];
    uint64_t kernel_pt[KERNEL_PT_COUNT][512];

    uint8_t user_pages[PROCESS_USER_REGION_PAGES][4096];
    uint8_t kernel_stack[KERNEL_STACK_PAGES * 4096];
} process_storage_t;

typedef struct {
    process_t *process;
    const boot_context_t *boot;
} segment_context_t;

static process_storage_t process_storage[PROCESS_MAX] __attribute__((aligned(4096)));

extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
extern const uint8_t user_init_elf_start[];
extern const uint8_t user_init_elf_end[];

static void zero_bytes(void *pointer, size_t length) {
    volatile uint8_t *bytes = (volatile uint8_t *)pointer;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

static void copy_bytes(void *destination, const void *source, size_t length) {
    volatile uint8_t *out = (volatile uint8_t *)destination;
    const volatile uint8_t *in = (const volatile uint8_t *)source;
    for (size_t i = 0; i < length; ++i) out[i] = in[i];
}

static int add_overflows_u64(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b;
}

static int kernel_virtual_to_physical(const boot_context_t *boot,
                                      const void *pointer,
                                      uint64_t *physical_out) {
    if (!boot || !pointer || !physical_out) return 0;
    uint64_t virtual_address = (uint64_t)(uintptr_t)pointer;
    if (virtual_address < boot->kernel_virt_base) return 0;

    uint64_t offset = virtual_address - boot->kernel_virt_base;
    if (add_overflows_u64(boot->kernel_phys_base, offset)) return 0;
    *physical_out = boot->kernel_phys_base + offset;
    return 1;
}

static int table_link(const boot_context_t *boot,
                      uint64_t *entry,
                      const void *table,
                      uint64_t flags) {
    uint64_t physical;
    if (!kernel_virtual_to_physical(boot, table, &physical)) return 0;
    if ((physical & PAGE_MASK) != 0) return 0;
    *entry = physical | flags;
    return 1;
}

static int build_address_space(process_t *process,
                               process_storage_t *storage,
                               const boot_context_t *boot) {
    uint64_t kernel_start = (uint64_t)(uintptr_t)__kernel_start;
    uint64_t kernel_end = (uint64_t)(uintptr_t)__kernel_end;
    if (!process || !storage || !boot || kernel_end <= kernel_start) return 0;
    if ((kernel_start & PAGE_MASK) != 0) return 0;
    if (add_overflows_u64(kernel_end, PAGE_MASK)) return 0;

    uint64_t kernel_page_end = (kernel_end + PAGE_MASK) & ~PAGE_MASK;
    if (kernel_start < boot->kernel_virt_base) return 0;

    uint16_t kernel_pml4 = (uint16_t)((kernel_start >> 39) & 0x1ffu);
    uint16_t kernel_pdpt = (uint16_t)((kernel_start >> 30) & 0x1ffu);
    uint64_t kernel_last = kernel_page_end - 1u;
    if (((kernel_last >> 39) & 0x1ffu) != kernel_pml4 ||
        ((kernel_last >> 30) & 0x1ffu) != kernel_pdpt) {
        return 0;
    }

    uint16_t first_pd = (uint16_t)((kernel_start >> 21) & 0x1ffu);
    uint16_t last_pd = (uint16_t)((kernel_last >> 21) & 0x1ffu);
    uint16_t pd_count = (uint16_t)(last_pd - first_pd + 1u);
    if (pd_count > KERNEL_PT_COUNT) return 0;

    if (!table_link(boot, &storage->pml4[kernel_pml4],
                    storage->kernel_pdpt, PTE_PRESENT | PTE_WRITE) ||
        !table_link(boot, &storage->kernel_pdpt[kernel_pdpt],
                    storage->kernel_pd, PTE_PRESENT | PTE_WRITE)) {
        return 0;
    }

    for (uint16_t pd = first_pd; pd <= last_pd; ++pd) {
        uint16_t slot = (uint16_t)(pd - first_pd);
        if (!table_link(boot, &storage->kernel_pd[pd],
                        storage->kernel_pt[slot], PTE_PRESENT | PTE_WRITE)) {
            return 0;
        }
    }

    for (uint64_t virtual_address = kernel_start;
         virtual_address < kernel_page_end;
         virtual_address += PAGE_SIZE) {
        uint64_t offset = virtual_address - boot->kernel_virt_base;
        if (add_overflows_u64(boot->kernel_phys_base, offset)) return 0;
        uint64_t physical = boot->kernel_phys_base + offset;
        if ((physical & PAGE_MASK) != 0) return 0;

        uint16_t pd = (uint16_t)((virtual_address >> 21) & 0x1ffu);
        uint16_t pt = (uint16_t)((virtual_address >> 12) & 0x1ffu);
        uint16_t slot = (uint16_t)(pd - first_pd);
        storage->kernel_pt[slot][pt] = physical | PTE_PRESENT | PTE_WRITE;
    }

    if (!table_link(boot, &storage->pml4[0],
                    storage->user_pdpt, PTE_PRESENT | PTE_WRITE | PTE_USER) ||
        !table_link(boot, &storage->user_pdpt[0],
                    storage->user_pd, PTE_PRESENT | PTE_WRITE | PTE_USER) ||
        !table_link(boot, &storage->user_pd[2],
                    storage->user_pt, PTE_PRESENT | PTE_WRITE | PTE_USER)) {
        return 0;
    }

    uint64_t root_physical;
    if (!kernel_virtual_to_physical(boot, storage->pml4, &root_physical) ||
        (root_physical & PAGE_MASK) != 0) {
        return 0;
    }
    process->cr3_phys = root_physical;
    return 1;
}

static int map_user_page(process_t *process,
                         const boot_context_t *boot,
                         uint32_t page_index,
                         int writable,
                         int executable) {
    if (!process || !boot || page_index >= PROCESS_USER_REGION_PAGES) return 0;
    process_storage_t *storage = (process_storage_t *)process->storage;

    uint64_t physical;
    if (!kernel_virtual_to_physical(boot, storage->user_pages[page_index], &physical) ||
        (physical & PAGE_MASK) != 0) {
        return 0;
    }

    uint64_t flags = PTE_PRESENT | PTE_USER;
    if (writable) flags |= PTE_WRITE;
    storage->user_pt[page_index] = physical | flags;
    process->mapped_pages[page_index] = 1;
    if (writable) process->writable_pages[page_index] = 1;
    if (executable) process->executable_pages[page_index] = 1;
    return 1;
}

static int load_segment(void *opaque,
                        uint64_t virtual_address,
                        const uint8_t *file_data,
                        uint64_t file_size,
                        uint64_t memory_size,
                        uint32_t flags) {
    segment_context_t *context = (segment_context_t *)opaque;
    process_t *process = context->process;
    process_storage_t *storage = (process_storage_t *)process->storage;

    if (memory_size == 0) return 1;
    if (virtual_address < PROCESS_USER_BASE ||
        add_overflows_u64(virtual_address, memory_size) ||
        virtual_address + memory_size > USER_STACK_BASE) {
        return 0;
    }
    if (file_size > (uint64_t)SIZE_MAX || memory_size > (uint64_t)SIZE_MAX) return 0;

    uint64_t first = (virtual_address - PROCESS_USER_BASE) / PAGE_SIZE;
    uint64_t last = (virtual_address + memory_size - 1u - PROCESS_USER_BASE) / PAGE_SIZE;
    for (uint64_t page = first; page <= last; ++page) {
        int writable = (flags & ELF64_PF_W) != 0 || process->writable_pages[page] != 0;
        int executable = (flags & ELF64_PF_X) != 0 || process->executable_pages[page] != 0;
        if (!map_user_page(process, context->boot, (uint32_t)page, writable, executable)) {
            return 0;
        }
    }

    size_t offset = (size_t)(virtual_address - PROCESS_USER_BASE);
    uint8_t *destination = &storage->user_pages[0][0] + offset;
    zero_bytes(destination, (size_t)memory_size);
    copy_bytes(destination, file_data, (size_t)file_size);
    return 1;
}

int process_create_init(process_t *process,
                        uint32_t pid,
                        uint32_t storage_slot,
                        const boot_context_t *boot) {
    if (!process || !boot || storage_slot >= PROCESS_MAX || pid == 0) return 0;

    zero_bytes(process, sizeof(*process));
    process_storage_t *storage = &process_storage[storage_slot];
    zero_bytes(storage, sizeof(*storage));
    process->storage = storage;
    process->pid = pid;

    if (!build_address_space(process, storage, boot)) return 0;

    uint64_t elf_start = (uint64_t)(uintptr_t)user_init_elf_start;
    uint64_t elf_end = (uint64_t)(uintptr_t)user_init_elf_end;
    if (elf_end <= elf_start || elf_end - elf_start > (uint64_t)SIZE_MAX) return 0;

    segment_context_t writer_context = {
        .process = process,
        .boot = boot
    };
    uint64_t entry = 0;
    if (elf64_load_image(user_init_elf_start,
                         (size_t)(elf_end - elf_start),
                         load_segment,
                         &writer_context,
                         &entry) != ELF64_OK) {
        return 0;
    }

    if (!map_user_page(process, boot, USER_STACK_PAGE, 1, 0)) return 0;
    if (entry < PROCESS_USER_BASE || entry >= USER_STACK_BASE) return 0;
    uint64_t entry_page = (entry - PROCESS_USER_BASE) / PAGE_SIZE;
    if (!process->mapped_pages[entry_page] || !process->executable_pages[entry_page]) return 0;

    process->kernel_stack_top =
        (uint64_t)(uintptr_t)(storage->kernel_stack + sizeof(storage->kernel_stack));
    if ((process->kernel_stack_top & 0xfu) != 0) return 0;

    process->context.rdi = pid;
    process->context.rip = entry;
    process->context.cs = GDT_USER_CODE_SELECTOR;
    process->context.rflags = UINT64_C(0x2);
    process->context.rsp = PROCESS_USER_STACK_TOP;
    process->context.ss = GDT_USER_DATA_SELECTOR;
    process->state = PROCESS_RUNNABLE;
    return 1;
}

int process_copy_from_user(const process_t *process,
                           uint64_t user_address,
                           void *destination,
                           size_t length) {
    if (!process || (!destination && length != 0)) return 0;
    if (length == 0) return 1;
    if (user_address < PROCESS_USER_BASE ||
        add_overflows_u64(user_address, (uint64_t)length) ||
        user_address + (uint64_t)length > PROCESS_USER_STACK_TOP) {
        return 0;
    }

    uint64_t first = (user_address - PROCESS_USER_BASE) / PAGE_SIZE;
    uint64_t last = (user_address + (uint64_t)length - 1u - PROCESS_USER_BASE) / PAGE_SIZE;
    for (uint64_t page = first; page <= last; ++page) {
        if (page >= PROCESS_USER_REGION_PAGES || !process->mapped_pages[page]) return 0;
    }

    const process_storage_t *storage = (const process_storage_t *)process->storage;
    size_t offset = (size_t)(user_address - PROCESS_USER_BASE);
    const volatile uint8_t *source = &storage->user_pages[0][0] + offset;
    volatile uint8_t *out = (volatile uint8_t *)destination;
    for (size_t i = 0; i < length; ++i) out[i] = source[i];
    return 1;
}
