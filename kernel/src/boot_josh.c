#include "boot_internal.h"
#include <stddef.h>
#include <stdint.h>

#define FOUR_GIB UINT64_C(0x100000000)

static int add_overflows_u64(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b;
}

static int mask_valid(uint32_t size, uint32_t shift) {
    if (size == 0 || size > 32 || shift >= 32) return 0;
    return size + shift <= 32;
}

static boot_status_t validate_framebuffer(const JoshFramebufferInfo *fb) {
    if (!fb || fb->address == 0) return BOOT_NO_FRAMEBUFFER;
    if (fb->bpp != 32 || fb->width == 0 || fb->height == 0 || fb->pitch == 0) {
        return BOOT_UNSUPPORTED_FRAMEBUFFER;
    }
    if (fb->width > UINT32_MAX / 4u || fb->pitch < fb->width * 4u || (fb->pitch & 3u) != 0u) {
        return BOOT_UNSUPPORTED_FRAMEBUFFER;
    }
    uint64_t framebuffer_bytes = (uint64_t)fb->pitch * (uint64_t)fb->height;
    if (fb->address >= FOUR_GIB || framebuffer_bytes > FOUR_GIB - fb->address) {
        return BOOT_UNSUPPORTED_FRAMEBUFFER;
    }
    if (!mask_valid(fb->red_mask_size, fb->red_mask_shift) ||
        !mask_valid(fb->green_mask_size, fb->green_mask_shift) ||
        !mask_valid(fb->blue_mask_size, fb->blue_mask_shift)) {
        return BOOT_UNSUPPORTED_FRAMEBUFFER;
    }
    return BOOT_OK;
}

static boot_status_t usable_memory_bytes(const JoshBootInfo *info, uint64_t *bytes_out) {
    if (!info || !bytes_out || info->memory_map_address == 0 || info->memory_map_entries == 0 ||
        info->memory_map_entries > JOSH_BOOT_MAX_MEMORY_ENTRIES ||
        info->memory_map_entry_size < sizeof(JoshMemoryMapEntry) ||
        info->memory_map_entry_size > 4096u) {
        return BOOT_NO_MEMORY_MAP;
    }

    const uint8_t *base = (const uint8_t *)(uintptr_t)info->memory_map_address;
    uint64_t bytes = 0;
    for (uint32_t i = 0; i < info->memory_map_entries; ++i) {
        const JoshMemoryMapEntry *entry = (const JoshMemoryMapEntry *)(base +
            (uint64_t)i * info->memory_map_entry_size);
        if (entry->length != 0 && add_overflows_u64(entry->base, entry->length)) {
            return BOOT_INVALID_BOOT_INFO;
        }
        if (entry->type != JOSH_MEMORY_USABLE || entry->length == 0) continue;
        if (add_overflows_u64(bytes, entry->length)) bytes = UINT64_MAX;
        else bytes += entry->length;
    }
    *bytes_out = bytes;
    return BOOT_OK;
}

boot_status_t boot_josh_context_init(boot_context_t *context, const JoshBootInfo *info) {
    if (!context || !info) return BOOT_INVALID_BOOT_INFO;
    if (info->magic != JOSH_BOOT_INFO_MAGIC || info->abi_major != JOSH_BOOT_ABI_MAJOR ||
        info->abi_minor < JOSH_BOOT_ABI_MINOR || info->total_size < sizeof(JoshBootInfo)) {
        return BOOT_INVALID_BOOT_INFO;
    }
    const uint32_t required = JOSH_BOOT_FLAG_MEMORY_MAP | JOSH_BOOT_FLAG_FRAMEBUFFER;
    if ((info->flags & required) != required) return BOOT_INVALID_BOOT_INFO;
    if (info->kernel_phys_start >= info->kernel_phys_end ||
        info->kernel_virt_start >= info->kernel_virt_end || info->kernel_entry == 0) {
        return BOOT_INVALID_BOOT_INFO;
    }

    uint64_t usable_bytes = 0;
    boot_status_t status = usable_memory_bytes(info, &usable_bytes);
    if (status != BOOT_OK) return status;
    status = validate_framebuffer(&info->framebuffer);
    if (status != BOOT_OK) return status;

    context->framebuffer.address = (void *)(uintptr_t)info->framebuffer.address;
    context->framebuffer.width = info->framebuffer.width;
    context->framebuffer.height = info->framebuffer.height;
    context->framebuffer.pitch = info->framebuffer.pitch;
    context->framebuffer.bpp = (uint16_t)info->framebuffer.bpp;
    context->framebuffer.red_mask_size = (uint8_t)info->framebuffer.red_mask_size;
    context->framebuffer.red_mask_shift = (uint8_t)info->framebuffer.red_mask_shift;
    context->framebuffer.green_mask_size = (uint8_t)info->framebuffer.green_mask_size;
    context->framebuffer.green_mask_shift = (uint8_t)info->framebuffer.green_mask_shift;
    context->framebuffer.blue_mask_size = (uint8_t)info->framebuffer.blue_mask_size;
    context->framebuffer.blue_mask_shift = (uint8_t)info->framebuffer.blue_mask_shift;
    context->usable_memory_mib = usable_bytes / (1024u * 1024u);
    return BOOT_OK;
}
