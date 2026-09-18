#include "boot.h"
#include <limine.h>
#include <stddef.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static uint64_t usable_memory_mib(void) {
    if (!memmap_request.response) return 0;

    uint64_t bytes = 0;
    for (uint64_t i = 0; i < memmap_request.response->entry_count; ++i) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) bytes += entry->length;
    }
    return bytes / (1024u * 1024u);
}

boot_status_t boot_context_init(boot_context_t *context) {
    if (!context) return BOOT_UNSUPPORTED_PROTOCOL;

    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        return BOOT_UNSUPPORTED_PROTOCOL;
    }

    if (!framebuffer_request.response ||
        framebuffer_request.response->framebuffer_count < 1) {
        return BOOT_NO_FRAMEBUFFER;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    if (!fb || fb->memory_model != LIMINE_FRAMEBUFFER_RGB || fb->bpp != 32) {
        return BOOT_UNSUPPORTED_FRAMEBUFFER;
    }

    context->framebuffer.address = fb->address;
    context->framebuffer.width = fb->width;
    context->framebuffer.height = fb->height;
    context->framebuffer.pitch = fb->pitch;
    context->framebuffer.bpp = fb->bpp;
    context->framebuffer.red_mask_size = fb->red_mask_size;
    context->framebuffer.red_mask_shift = fb->red_mask_shift;
    context->framebuffer.green_mask_size = fb->green_mask_size;
    context->framebuffer.green_mask_shift = fb->green_mask_shift;
    context->framebuffer.blue_mask_size = fb->blue_mask_size;
    context->framebuffer.blue_mask_shift = fb->blue_mask_shift;
    context->usable_memory_mib = usable_memory_mib();

    return BOOT_OK;
}
