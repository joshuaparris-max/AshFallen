#ifndef JOSHOS_BOOT_H
#define JOSHOS_BOOT_H

#include <stdint.h>

#define BOOT_MEMORY_MAX_ENTRIES 128u

typedef enum {
    BOOT_MEMORY_RESERVED = 0,
    BOOT_MEMORY_USABLE = 1,
    BOOT_MEMORY_ACPI_RECLAIMABLE = 2,
    BOOT_MEMORY_ACPI_NVS = 3,
    BOOT_MEMORY_BAD = 4
} boot_memory_type_t;

typedef struct {
    uint64_t base;
    uint64_t length;
    boot_memory_type_t type;
    uint32_t flags;
} boot_memory_region_t;


typedef struct {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
} boot_framebuffer_t;

typedef struct {
    boot_framebuffer_t framebuffer;
    boot_memory_region_t memory_map[BOOT_MEMORY_MAX_ENTRIES];
    uint32_t memory_map_count;
    uint64_t usable_memory_mib;
    uint64_t rsdp_phys;
    uint64_t smbios_phys;
} boot_context_t;

typedef enum {
    BOOT_OK = 0,
    BOOT_UNSUPPORTED_PROTOCOL,
    BOOT_INVALID_BOOT_INFO,
    BOOT_NO_MEMORY_MAP,
    BOOT_NO_FRAMEBUFFER,
    BOOT_UNSUPPORTED_FRAMEBUFFER
} boot_status_t;

boot_status_t boot_context_init(boot_context_t *context,
                                uint64_t loader_magic1,
                                uint64_t loader_magic2,
                                const void *loader_payload);

#endif
