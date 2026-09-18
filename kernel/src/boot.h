#ifndef JOSHOS_BOOT_H
#define JOSHOS_BOOT_H

#include <stdint.h>

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
    uint64_t usable_memory_mib;
    uint64_t kernel_phys_base;
    uint64_t kernel_virt_base;
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
