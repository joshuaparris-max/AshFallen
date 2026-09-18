#ifndef JOSHOS_BOOT_H
#define JOSHOS_BOOT_H

#include <stdbool.h>
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
} boot_context_t;

typedef enum {
    BOOT_OK = 0,
    BOOT_UNSUPPORTED_PROTOCOL,
    BOOT_NO_FRAMEBUFFER,
    BOOT_UNSUPPORTED_FRAMEBUFFER
} boot_status_t;

/*
 * Build a Josh-owned boot context from the active boot protocol.
 *
 * Today the implementation is Limine. Keeping the rest of the kernel behind
 * this boundary means a future JoshBootloader/JoshBootInfo adapter can replace
 * Limine without teaching graphics, shell or desktop code about boot protocols.
 */
boot_status_t boot_context_init(boot_context_t *context);

#endif
