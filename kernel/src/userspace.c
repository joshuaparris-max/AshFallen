#include "userspace.h"
#include "gdt.h"
#include "paging.h"
#include "pmm.h"
#include "serial.h"
#include <stdint.h>

#define USER_CODE_BASE UINT64_C(0x0000400000400000)
#define USER_STACK_BASE UINT64_C(0x00007fffffffd000)
#define USER_STACK_TOP  (USER_STACK_BASE + PMM_PAGE_SIZE)
#define USER_KERNEL_STACK_SIZE 16384u

extern const uint8_t user_probe_start[];
extern const uint8_t user_probe_end[];
extern __attribute__((noreturn))
void user_enter(uint64_t root_phys, uint64_t entry, uint64_t stack_top);

static uint8_t user_kernel_stack[USER_KERNEL_STACK_SIZE] __attribute__((aligned(16)));

static void zero_page(void *page) {
    uint64_t *words = (uint64_t *)page;
    for (uint32_t i = 0; i < PMM_PAGE_SIZE / sizeof(uint64_t); ++i) words[i] = 0;
}

static int copy_probe(uint64_t physical) {
    uint64_t size = (uint64_t)(user_probe_end - user_probe_start);
    if (size == 0 || size > PMM_PAGE_SIZE) return 0;

    uint8_t *destination = (uint8_t *)paging_direct_pointer(physical);
    if (!destination) return 0;
    zero_page(destination);

    for (uint64_t i = 0; i < size; ++i) destination[i] = user_probe_start[i];
    return 1;
}

__attribute__((noreturn)) void userspace_ring3_self_test(void) {
    paging_address_space_t space = {0};
    if (paging_create_user_address_space(&space) != PAGING_OK) {
        serial_write("JOSHOS_ERROR_USER_ADDRESS_SPACE\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    uint64_t code_phys = pmm_alloc_frame();
    uint64_t stack_phys = pmm_alloc_frame();
    if (code_phys == UINT64_MAX || stack_phys == UINT64_MAX) {
        serial_write("JOSHOS_ERROR_USER_FRAMES\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    if (!copy_probe(code_phys)) {
        serial_write("JOSHOS_ERROR_USER_IMAGE_COPY\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    void *stack_page = paging_direct_pointer(stack_phys);
    if (!stack_page) {
        serial_write("JOSHOS_ERROR_USER_STACK_ACCESS\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    zero_page(stack_page);

    if (paging_map_user_page(&space, USER_CODE_BASE, code_phys, 0, 1) != PAGING_OK ||
        paging_map_user_page(&space, USER_STACK_BASE, stack_phys, 1, 0) != PAGING_OK) {
        serial_write("JOSHOS_ERROR_USER_PAGE_MAP\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    paging_mapping_t code_mapping;
    paging_mapping_t stack_mapping;
    if (!paging_query_address_space(&space, USER_CODE_BASE, &code_mapping) ||
        !paging_query_address_space(&space, USER_STACK_BASE, &stack_mapping) ||
        !code_mapping.present || !code_mapping.user ||
        code_mapping.writable || !code_mapping.executable ||
        !stack_mapping.present || !stack_mapping.user ||
        !stack_mapping.writable || stack_mapping.executable) {
        serial_write("JOSHOS_ERROR_USER_PERMISSIONS\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    serial_write("JOSHOS_USER_PAGING_OK\n");

    uintptr_t kernel_stack_top =
        (uintptr_t)(user_kernel_stack + sizeof(user_kernel_stack));
    kernel_stack_top &= ~(uintptr_t)0x0fu;
    gdt_set_kernel_stack((uint64_t)kernel_stack_top);

    serial_write("JOSHOS_RING3_ENTER\n");
    user_enter(space.root_phys, USER_CODE_BASE, USER_STACK_TOP);
}
