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

static boot_memory_type_t map_memory_type(uint32_t type) {
    switch (type) {
        case JOSH_MEMORY_USABLE: return BOOT_MEMORY_USABLE;
        case JOSH_MEMORY_ACPI_RECLAIMABLE: return BOOT_MEMORY_ACPI_RECLAIMABLE;
        case JOSH_MEMORY_ACPI_NVS: return BOOT_MEMORY_ACPI_NVS;
        case JOSH_MEMORY_BAD: return BOOT_MEMORY_BAD;
        case JOSH_MEMORY_RESERVED:
        default:
            return BOOT_MEMORY_RESERVED;
    }
}

static boot_status_t copy_memory_map(
    boot_context_t *context,
    const JoshBootInfo *info,
    uint64_t *usable_bytes_out
) {
    if (!context || !info || !usable_bytes_out ||
        info->memory_map_address == 0 || info->memory_map_entries == 0 ||
        info->memory_map_entries > JOSH_BOOT_MAX_MEMORY_ENTRIES ||
        info->memory_map_entries > BOOT_MEMORY_MAX_ENTRIES ||
        info->memory_map_entry_size < sizeof(JoshMemoryMapEntry) ||
        info->memory_map_entry_size > 4096u) {
        return BOOT_NO_MEMORY_MAP;
    }

    context->memory_map_count = 0;
    uint64_t usable_bytes = 0;
    const uint8_t *base = (const uint8_t *)(uintptr_t)info->memory_map_address;

    for (uint32_t i = 0; i < info->memory_map_entries; ++i) {
        const JoshMemoryMapEntry *entry = (const JoshMemoryMapEntry *)(base +
            (uint64_t)i * info->memory_map_entry_size);

        if (entry->length == 0) continue;
        if (add_overflows_u64(entry->base, entry->length)) {
            return BOOT_INVALID_BOOT_INFO;
        }
        if (context->memory_map_count >= BOOT_MEMORY_MAX_ENTRIES) {
            return BOOT_NO_MEMORY_MAP;
        }

        boot_memory_region_t *out =
            &context->memory_map[context->memory_map_count++];
        out->base = entry->base;
        out->length = entry->length;
        out->type = map_memory_type(entry->type);
        out->flags = entry->flags;

        if (out->type == BOOT_MEMORY_USABLE) {
            if (add_overflows_u64(usable_bytes, out->length)) {
                usable_bytes = UINT64_MAX;
            } else {
                usable_bytes += out->length;
            }
        }
    }

    if (context->memory_map_count == 0) return BOOT_NO_MEMORY_MAP;
    *usable_bytes_out = usable_bytes;
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
    boot_status_t status = copy_memory_map(context, info, &usable_bytes);
    if (status != BOOT_OK) return status;
    status = validate_framebuffer(&info->framebuffer);
    if (status != BOOT_OK) return status;

    if ((info->flags & JOSH_BOOT_FLAG_RSDP) != 0 && info->rsdp_phys == 0) {
        return BOOT_INVALID_BOOT_INFO;
    }
    if ((info->flags & JOSH_BOOT_FLAG_SMBIOS) != 0 && info->smbios_phys == 0) {
        return BOOT_INVALID_BOOT_INFO;
    }

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
    context->physical_memory_offset = 0;
    context->physical_memory_limit = FOUR_GIB;
    context->kernel_phys_start = info->kernel_phys_start;
    context->kernel_phys_end = info->kernel_phys_end;
    context->kernel_virt_start = info->kernel_virt_start;
    context->kernel_virt_end = info->kernel_virt_end;
    context->rsdp_phys =
        (info->flags & JOSH_BOOT_FLAG_RSDP) != 0 ? info->rsdp_phys : 0;
    context->smbios_phys =
        (info->flags & JOSH_BOOT_FLAG_SMBIOS) != 0 ? info->smbios_phys : 0;
    return BOOT_OK;
}
