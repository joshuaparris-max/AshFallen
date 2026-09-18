#ifndef JOSHOS_JOSH_BOOT_PROTOCOL_H
#define JOSHOS_JOSH_BOOT_PROTOCOL_H

#include <stdint.h>

#define JOSH_BOOT_INFO_MAGIC UINT64_C(0x30494248534f4a4a)
#define JOSH_BOOT_ABI_MAJOR 0u
#define JOSH_BOOT_ABI_MINOR 1u

#define JOSH_LOADER_MAGIC1 UINT64_C(0x544f4f4248534f4a)
#define JOSH_LOADER_MAGIC2 UINT64_C(0x0030564f544f5250)

#define JOSH_BOOT_FLAG_MEMORY_MAP (1u << 0)
#define JOSH_BOOT_FLAG_FRAMEBUFFER (1u << 1)
#define JOSH_BOOT_FLAG_RSDP        (1u << 2)
#define JOSH_BOOT_FLAG_SMBIOS      (1u << 3)
#define JOSH_BOOT_FLAG_CMDLINE     (1u << 4)
#define JOSH_BOOT_FLAG_LOADER_NAME (1u << 5)

#define JOSH_FIRMWARE_LEGACY_BIOS 1u

#define JOSH_MEMORY_USABLE            1u
#define JOSH_MEMORY_RESERVED          2u
#define JOSH_MEMORY_ACPI_RECLAIMABLE  3u
#define JOSH_MEMORY_ACPI_NVS          4u
#define JOSH_MEMORY_BAD               5u

#define JOSH_BOOT_MAX_MEMORY_ENTRIES 128u

typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t flags;
} JoshMemoryMapEntry;

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t red_mask_size;
    uint32_t red_mask_shift;
    uint32_t green_mask_size;
    uint32_t green_mask_shift;
    uint32_t blue_mask_size;
    uint32_t blue_mask_shift;
} JoshFramebufferInfo;

typedef struct __attribute__((packed)) {
    uint64_t magic;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint32_t total_size;
    uint32_t flags;
    uint32_t firmware_type;
    uint32_t boot_drive;
    uint64_t memory_map_address;
    uint32_t memory_map_entries;
    uint32_t memory_map_entry_size;
    JoshFramebufferInfo framebuffer;
    uint64_t kernel_phys_start;
    uint64_t kernel_phys_end;
    uint64_t kernel_virt_start;
    uint64_t kernel_virt_end;
    uint64_t rsdp_phys;
    uint64_t smbios_phys;
    uint64_t command_line_address;
    uint32_t command_line_length;
    uint32_t reserved0;
    uint64_t bootloader_name_address;
    uint32_t bootloader_name_length;
    uint32_t reserved1;
    uint64_t kernel_entry;
    uint64_t reserved[5];
} JoshBootInfo;

_Static_assert(sizeof(JoshMemoryMapEntry) == 24, "Josh memory-map ABI changed");
_Static_assert(sizeof(JoshFramebufferInfo) == 48, "Josh framebuffer ABI changed");
_Static_assert(sizeof(JoshBootInfo) == 224, "Josh boot-info ABI changed");

#endif
