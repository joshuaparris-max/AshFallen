#include <stdint.h>
#include "timer.h"
#include "userspace.h"
#include "input.h"
#include "apic.h"
#include "acpi.h"
#include "boot.h"
#include "cpu.h"
#include "desktop.h"
#include "e1000.h"
#include "gdt.h"
#include "gfx.h"
#include "heap.h"
#include "interrupts.h"
#include "keyboard.h"
#include "net.h"
#include "pmm.h"
#include "paging.h"
#include "pci.h"
#include "serial.h"
#include "scheduler.h"
#include "shell.h"
#include "smp.h"

static boot_context_t boot_context;
static cpu_features_t cpu_features;
static acpi_platform_info_t platform_info;

static void halt_forever(void) {
    for (;;) __asm__ volatile ("hlt");
}

static __attribute__((noreturn)) void kernel_after_paging(void) {
    if ((cpu_read_cr3() & UINT64_C(0x000ffffffffff000)) !=
        paging_current_root()) {
        serial_write("JOSHOS_ERROR_PAGING_CR3\n");
        halt_forever();
    }
    serial_write("JOSHOS_PAGING_OWNED_OK\n");

    if (!paging_verify_kernel_layout()) {
        serial_write("JOSHOS_ERROR_PAGING_PERMISSIONS\n");
        halt_forever();
    }
    serial_write("JOSHOS_PAGING_PERMISSIONS_OK\n");

    scheduler_init();
    if (!scheduler_self_test()) {
        serial_write("JOSHOS_ERROR_SCHEDULER_TEST\n");
        halt_forever();
    }
    serial_write("JOSHOS_SCHEDULER_OK\n");

#ifdef JOSHOS_RING3_TEST
    userspace_ring3_self_test();
#endif

    heap_status_t heap_status = heap_kernel_init(&boot_context);
    if (heap_status != HEAP_OK) {
        serial_write("JOSHOS_ERROR_HEAP_INIT\n");
        serial_write(heap_status_string(heap_status));
        serial_write("\n");
        halt_forever();
    }
    serial_write("JOSHOS_HEAP_OK\n");

    if (!heap_self_test()) {
        serial_write("JOSHOS_ERROR_HEAP_SELF_TEST\n");
        halt_forever();
    }
    serial_write("JOSHOS_HEAP_SELF_TEST_OK\n");

    pci_init();
    if (pci_device_count() == 0) {
        serial_write("JOSHOS_ERROR_PCI_ENUMERATION\n");
        halt_forever();
    }
    serial_write("JOSHOS_PCI_OK\n");

    if (!net_self_test()) {
        serial_write("JOSHOS_ERROR_NET_LOOPBACK\n");
        halt_forever();
    }
    serial_write("JOSHOS_NET_LOOPBACK_OK\n");

    uint32_t net_device_id = 0;
    e1000_status_t e1000_status = e1000_init(&net_device_id);
    if (e1000_status != E1000_OK) {
        serial_write("JOSHOS_ERROR_E1000_INIT\n");
        serial_write(e1000_status_string(e1000_status));
        serial_write("\n");
        halt_forever();
    }
    serial_write("JOSHOS_E1000_OK\n");

    acpi_status_t acpi_status =
        acpi_kernel_discover(&boot_context, &platform_info);
    if (acpi_status != ACPI_OK) {
        serial_write("JOSHOS_ERROR_ACPI_MADT\n");
        serial_write(acpi_status_string(acpi_status));
        serial_write("\n");
        halt_forever();
    }
    if (platform_info.cpu_count == 0 ||
        platform_info.ioapic_count == 0) {
        serial_write("JOSHOS_ERROR_ACPI_TOPOLOGY\n");
        halt_forever();
    }
    serial_write("JOSHOS_ACPI_MADT_OK\n");

    apic_status_t apic_status =
        apic_init(&platform_info, &cpu_features);
    if (apic_status != APIC_OK) {
        serial_write("JOSHOS_ERROR_APIC_INIT\n");
        serial_write(apic_status_string(apic_status));
        serial_write("\n");
        halt_forever();
    }
    serial_write("JOSHOS_APIC_OK\n");
    serial_write("JOSHOS_IOAPIC_OK\n");

    smp_status_t smp_status =
        smp_init(&platform_info, apic_local_id());
    if (smp_status != SMP_OK) {
        serial_write("JOSHOS_ERROR_SMP_INIT\n");
        serial_write(smp_status_string(smp_status));
        serial_write("\n");
        halt_forever();
    }
    serial_write("JOSHOS_SMP_TOPOLOGY_OK\n");
    serial_write("JOSHOS_PERCPU_STACKS_OK\n");
    if (smp_cpu_count() > 1) {
        serial_write("JOSHOS_SMP_MULTICPU_OK\n");
    }

    input_reset();

    timer_status_t timer_status = timer_init(100);
    if (timer_status != TIMER_OK) {
        serial_write("JOSHOS_ERROR_TIMER_INIT\n");
        serial_write(timer_status_string(timer_status));
        serial_write("\n");
        halt_forever();
    }
    serial_write("JOSHOS_TIMER_CONFIG_OK\n");

    keyboard_status_t keyboard_status = keyboard_init();
    if (keyboard_status != KEYBOARD_OK) {
        serial_write("JOSHOS_ERROR_KEYBOARD_INIT\n");
        serial_write(keyboard_status_string(keyboard_status));
        serial_write("\n");
        halt_forever();
    }
    serial_write("JOSHOS_KEYBOARD_IRQ_CONFIG_OK\n");

    gfx_init(&boot_context.framebuffer);
    desktop_layout_t layout = desktop_draw();
    shell_init(layout.terminal_x, layout.terminal_y, layout.terminal_w, layout.terminal_h,
               boot_context.usable_memory_mib);

    interrupts_enable();

    uint64_t timer_before = timer_ticks();
    if (!timer_sleep_ms(30)) {
        serial_write("JOSHOS_ERROR_TIMER_SLEEP\n");
        halt_forever();
    }
    uint64_t timer_after = timer_ticks();
    if (timer_after <= timer_before ||
        timer_now_ns() == 0 ||
        timer_frequency_hz() == 0) {
        serial_write("JOSHOS_ERROR_TIMER_IRQ\n");
        halt_forever();
    }
    serial_write("JOSHOS_TIMER_IRQ_OK\n");

    if (!smp_self_ipi_test()) {
        serial_write("JOSHOS_ERROR_IPI_TEST\n");
        halt_forever();
    }
    serial_write("JOSHOS_IPI_OK\n");
    serial_write("JOSHOS_INTERRUPT_INPUT_READY\n");

    serial_write("JOSHOS_BOOT_OK\n");

    int keyboard_irq_proven = 0;
    for (;;) {
        e1000_poll();

        input_event_t event;
        int handled = 0;
        while (input_pop(&event)) {
            handled = 1;
            if (event.type == INPUT_EVENT_KEY &&
                event.pressed &&
                event.character != 0) {
                if (!keyboard_irq_proven &&
                    event.character == 'a' &&
                    keyboard_interrupt_count() != 0) {
                    serial_write("JOSHOS_KEYBOARD_IRQ_OK\n");
                    keyboard_irq_proven = 1;
                }
                shell_handle_key(event.character);
            }
        }

        if (!handled) {
            __asm__ volatile ("hlt");
        }
    }
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

void kmain(uint64_t loader_magic1, uint64_t loader_magic2, const void *loader_payload) {
    serial_init();
    serial_write("JOSHOS_KERNEL_ENTERED\n");

    cpu_features = cpu_detect();
    if (!cpu_required_features_present(&cpu_features)) {
        serial_write("JOSHOS_ERROR_CPU_FEATURES\n");
        halt_forever();
    }
    serial_write("JOSHOS_CPU_FEATURES_OK\n");

    if (!cpu_enable_nx(&cpu_features)) {
        serial_write("JOSHOS_ERROR_NX_ENABLE\n");
        halt_forever();
    }
    serial_write("JOSHOS_NX_OK\n");

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

#ifdef JOSHOS_FAULT_TEST_PAGE
    serial_write("JOSHOS_FAULT_TEST_PAGE\n");
    __asm__ volatile (
        "movabs $0x00007ffffffff000, %%rax\n\t"
        "movq $0x1, (%%rax)"
        :
        :
        : "rax", "memory"
    );
    serial_write("JOSHOS_ERROR_PAGE_TEST_RETURNED\n");
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

    boot_status_t status = boot_context_init(&boot_context, loader_magic1, loader_magic2, loader_payload);
    if (status != BOOT_OK) {
        report_boot_error(status);
        halt_forever();
    }

    serial_write("JOSHOS_BOOT_ADAPTER_OK\n");

    if (boot_context.rsdp_phys != 0) {
        serial_write("JOSHOS_RSDP_OK\n");
    }
    if (boot_context.smbios_phys != 0) {
        serial_write("JOSHOS_SMBIOS_OK\n");
    }

    pmm_status_t pmm_status = pmm_init(&boot_context);
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

    uint64_t paging_root = 0;
    paging_status_t paging_status = paging_init(&boot_context, &paging_root);
    if (paging_status != PAGING_OK) {
        serial_write("JOSHOS_ERROR_PAGING_INIT\n");
        serial_write(paging_status_string(paging_status));
        serial_write("\n");
        halt_forever();
    }
    serial_write("JOSHOS_PAGING_TABLES_OK\n");

    paging_activate(paging_root, kernel_after_paging);
}
