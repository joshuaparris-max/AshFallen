#include <stdint.h>
#include "ahci.h"
#include "block.h"
#include "boot.h"
#include "cpu.h"
#include "desktop.h"
#include "gdt.h"
#include "gfx.h"
#include "heap.h"
#include "interrupts.h"
#include "keyboard.h"
#include "pci.h"
#include "pmm.h"
#include "paging.h"
#include "partition.h"
#include "serial.h"
#include "shell.h"

static boot_context_t boot_context;
static pci_device_t ahci_controller;
static int ahci_controller_present;


static void serial_partition_result(const josh_block_device_t *block) {
    josh_partition_summary_t summary;
    josh_partition_status_t status =
        josh_partition_scan(block, &summary, 0, 0);

    if (status == JOSH_PARTITION_OK) {
        serial_write("JOSHOS_PARTITION_SCAN_OK\n");
    } else if (status == JOSH_PARTITION_NO_TABLE ||
               status == JOSH_PARTITION_NO_PARTITIONS) {
        serial_write("JOSHOS_PARTITION_NONE\n");
    } else {
        serial_write("JOSHOS_PARTITION_SCAN_ERROR\n");
        serial_write(josh_partition_status_string(status));
        serial_write("\n");
    }
}

#ifdef JOSHOS_AHCI_PERSIST_TEST
static int marker_matches(const uint8_t *sector) {
    static const char marker[] = "JOSHOS_AHCI_PERSIST_V1";
    for (uint32_t i = 0; i < sizeof(marker); ++i) {
        if (sector[i] != (uint8_t)marker[i]) return 0;
    }
    return 1;
}

static void write_marker(uint8_t *sector) {
    static const char marker[] = "JOSHOS_AHCI_PERSIST_V1";
    for (uint32_t i = 0; i < JOSH_BLOCK_SECTOR_SIZE; ++i) sector[i] = 0;
    for (uint32_t i = 0; i < sizeof(marker); ++i) {
        sector[i] = (uint8_t)marker[i];
    }
}

static void run_ahci_persistence_test(josh_block_device_t *block) {
    static uint8_t sector[JOSH_BLOCK_SECTOR_SIZE];
    const uint64_t marker_lba = 8;

    if (!josh_block_range_valid(block, marker_lba, 1) ||
        josh_block_read(block, marker_lba, 1, sector) != 0) {
        serial_write("JOSHOS_AHCI_PERSIST_READ_ERROR\n");
        return;
    }

    if (marker_matches(sector)) {
        serial_write("JOSHOS_AHCI_PERSIST_OK\n");
        return;
    }

    write_marker(sector);
    if (josh_block_write(block, marker_lba, 1, sector) != 0) {
        serial_write("JOSHOS_AHCI_PERSIST_WRITE_ERROR\n");
        return;
    }

    for (uint32_t i = 0; i < JOSH_BLOCK_SECTOR_SIZE; ++i) sector[i] = 0;
    if (josh_block_read(block, marker_lba, 1, sector) != 0 ||
        !marker_matches(sector)) {
        serial_write("JOSHOS_AHCI_PERSIST_VERIFY_ERROR\n");
        return;
    }

    serial_write("JOSHOS_AHCI_PERSIST_WRITTEN\n");
}
#endif

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

    if (ahci_controller_present) {
        static ahci_device_t ahci;
        static josh_block_device_t block;
        ahci_status_t ahci_status =
            ahci_init(&ahci_controller, &boot_context, &ahci);

        if (ahci_status == AHCI_OK &&
            ahci_make_block_device(&ahci, &block) == 0) {
            serial_write("JOSHOS_AHCI_OK\n");
            serial_partition_result(&block);
#ifdef JOSHOS_AHCI_PERSIST_TEST
            run_ahci_persistence_test(&block);
#endif
        } else if (ahci_status == AHCI_NO_SATA_DEVICE) {
            serial_write("JOSHOS_AHCI_NO_DISK\n");
        } else {
            serial_write("JOSHOS_AHCI_ERROR\n");
            serial_write(ahci_status_string(ahci_status));
            serial_write("\n");
        }
    }

    gfx_init(&boot_context.framebuffer);
    desktop_layout_t layout = desktop_draw();
    shell_init(layout.terminal_x, layout.terminal_y, layout.terminal_w, layout.terminal_h,
               boot_context.usable_memory_mib);

    serial_write("JOSHOS_BOOT_OK\n");

    for (;;) {
        char key = keyboard_poll();
        if (key) shell_handle_key(key);
        __asm__ volatile ("pause");
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

    cpu_features_t cpu = cpu_detect();
    if (!cpu_required_features_present(&cpu)) {
        serial_write("JOSHOS_ERROR_CPU_FEATURES\n");
        halt_forever();
    }
    serial_write("JOSHOS_CPU_FEATURES_OK\n");

    if (!cpu_enable_nx(&cpu)) {
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

    pci_scan_summary_t pci_summary;
    if (pci_init(&pci_summary) != 0) {
        serial_write("JOSHOS_ERROR_PCI_INIT\n");
        halt_forever();
    }
    ahci_controller_present =
        pci_get_first_storage(PCI_STORAGE_AHCI, &ahci_controller) == 0;

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
