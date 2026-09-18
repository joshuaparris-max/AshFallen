#include <stdint.h>
#include "boot.h"
#include "desktop.h"
#include "gdt.h"
#include "gfx.h"
#include "interrupts.h"
#include "keyboard.h"
#include "pmm.h"
#include "serial.h"
#include "shell.h"

static void halt_forever(void) {
    for (;;) __asm__ volatile ("hlt");
}

static void report_boot_error(boot_status_t status) {
    switch (status) {
        case BOOT_UNSUPPORTED_PROTOCOL:
            serial_write("JOSHOS_ERROR_UNSUPPORTED_BOOT_PROTOCOL\n");
            break;
        case BOOT_INVALID_BOOT_INFO:
            serial_write("JOSHOS_ERROR_INVALID_BOOT_INFO\n");
            break;
        case BOOT_NO_MEMORY_MAP:
            serial_write("JOSHOS_ERROR_NO_MEMORY_MAP\n");
            break;
        case BOOT_NO_FRAMEBUFFER:
            serial_write("JOSHOS_ERROR_NO_FRAMEBUFFER\n");
            break;
        case BOOT_UNSUPPORTED_FRAMEBUFFER:
            serial_write("JOSHOS_ERROR_UNSUPPORTED_FRAMEBUFFER\n");
            break;
        case BOOT_NO_PHYSICAL_MAP:
            serial_write("JOSHOS_ERROR_NO_PHYSICAL_MAP\n");
            break;
        case BOOT_OK:
        default:
            serial_write("JOSHOS_ERROR_UNKNOWN_BOOT_STATE\n");
            break;
    }
}

#ifdef JOSHOS_FAULT_TEST_PAGE
static void trigger_page_fault(const boot_context_t *boot) {
    const uint64_t target = UINT64_C(0x00007ffffffff000);
    uint64_t cr3 = 0;
    __asm__ volatile ("movq %%cr3, %0" : "=r"(cr3));

    uint64_t pml4_phys = cr3 & UINT64_C(0x000ffffffffff000);
    uint64_t pml4_virt = boot->physical_memory_offset + pml4_phys;
    volatile uint64_t *pml4 = (volatile uint64_t *)(uintptr_t)pml4_virt;
    uint64_t pml4_index = (target >> 39) & UINT64_C(0x1ff);

    /*
     * Make the target deterministically non-present. This keeps the exception
     * test independent of whichever bootstrap mappings Limine happens to
     * install on a particular version.
     */
    pml4[pml4_index] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"((uintptr_t)target) : "memory");

    serial_write("JOSHOS_FAULT_TEST_PAGE\n");
    *(volatile uint64_t *)(uintptr_t)target = UINT64_C(1);
    serial_write("JOSHOS_ERROR_PAGE_TEST_RETURNED\n");
    halt_forever();
}
#endif

void kmain(uint64_t loader_magic1, uint64_t loader_magic2, const void *loader_payload) {
    serial_init();
    serial_write("JOSHOS_KERNEL_ENTERED\n");

    if (!gdt_init()) {
        serial_write("JOSHOS_ERROR_GDT_TSS_INIT\n");
        halt_forever();
    }
    serial_write("JOSHOS_GDT_TSS_OK\n");

    interrupts_init();
    serial_write("JOSHOS_IDT_OK\n");

#ifdef JOSHOS_FAULT_TEST_UD2
    serial_write("JOSHOS_FAULT_TEST_UD2\n");
    __asm__ volatile (
        "movabs $0x1122334455667788, %%rax\n\t"
        "movabs $0x8877665544332211, %%r15\n\t"
        "ud2"
        :
        :
        : "rax", "r15", "memory"
    );
#endif

#ifdef JOSHOS_FAULT_TEST_DIVIDE
    serial_write("JOSHOS_FAULT_TEST_DIVIDE\n");
    __asm__ volatile (
        "mov $1, %%eax\n\t"
        "xor %%edx, %%edx\n\t"
        "xor %%ecx, %%ecx\n\t"
        "div %%ecx"
        :
        :
        : "rax", "rcx", "rdx", "memory"
    );
    serial_write("JOSHOS_ERROR_DIVIDE_TEST_RETURNED\n");
    halt_forever();
#endif

#ifdef JOSHOS_FAULT_TEST_GP
    serial_write("JOSHOS_FAULT_TEST_GP\n");
    __asm__ volatile (
        "movw $0xffff, %%ax\n\t"
        "movw %%ax, %%ds"
        :
        :
        : "rax", "memory"
    );
    serial_write("JOSHOS_ERROR_GP_TEST_RETURNED\n");
    halt_forever();
#endif

#ifdef JOSHOS_FAULT_TEST_DOUBLE_FAULT
    serial_write("JOSHOS_FAULT_TEST_DOUBLE_FAULT\n");
    interrupts_arm_double_fault_test();
    __asm__ volatile (
        "movw $0xffff, %%ax\n\t"
        "movw %%ax, %%ds"
        :
        :
        : "rax", "memory"
    );
    serial_write("JOSHOS_ERROR_DOUBLE_FAULT_TEST_RETURNED\n");
    halt_forever();
#endif

    boot_context_t boot;
    boot_status_t status = boot_context_init(&boot, loader_magic1, loader_magic2, loader_payload);
    if (status != BOOT_OK) {
        report_boot_error(status);
        halt_forever();
    }

    serial_write("JOSHOS_BOOT_ADAPTER_OK\n");

#ifdef JOSHOS_FAULT_TEST_PAGE
    trigger_page_fault(&boot);
#endif

    if (boot.rsdp_phys != 0) {
        serial_write("JOSHOS_RSDP_OK\n");
    }
    if (boot.smbios_phys != 0) {
        serial_write("JOSHOS_SMBIOS_OK\n");
    }

    pmm_status_t pmm_status = pmm_init(&boot);
    if (pmm_status != PMM_OK) {
        serial_write("JOSHOS_ERROR_PMM_INIT\n");
        serial_write(pmm_status_string(pmm_status));
        serial_write("\n");
        halt_forever();
    }
    serial_write("JOSHOS_PMM_OK\n");

    if (!pmm_self_test(4096)) {
        serial_write("JOSHOS_ERROR_PMM_STRESS\n");
        halt_forever();
    }
    serial_write("JOSHOS_PMM_STRESS_OK\n");

    gfx_init(&boot.framebuffer);
    desktop_layout_t layout = desktop_draw();
    shell_init(layout.terminal_x, layout.terminal_y, layout.terminal_w, layout.terminal_h,
               boot.usable_memory_mib);

    serial_write("JOSHOS_BOOT_OK\n");

    for (;;) {
        char key = keyboard_poll();
        if (key) shell_handle_key(key);
        __asm__ volatile ("pause");
    }
}
