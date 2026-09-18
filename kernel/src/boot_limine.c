#include "boot_internal.h"
#include <limine.h>
#include <stddef.h>
#include <stdint.h>

extern char __kernel_start[];
extern char __kernel_end[];

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_smbios_request smbios_request = {
    .id = LIMINE_SMBIOS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static int mask_valid(uint8_t size, uint8_t shift) {
    if (size == 0 || size > 32 || shift >= 32) return 0;
    return (uint16_t)size + (uint16_t)shift <= 32;
}

static int framebuffer_valid(const struct limine_framebuffer *fb) {
    if (!fb || !fb->address) return 0;
    if (fb->memory_model != LIMINE_FRAMEBUFFER_RGB || fb->bpp != 32) return 0;
    if (fb->width == 0 || fb->height == 0 || fb->pitch == 0) return 0;
    if (fb->width > UINT64_MAX / 4u) return 0;
    uint64_t minimum_pitch = fb->width * 4u;
    if (fb->pitch < minimum_pitch || (fb->pitch & 3u) != 0u) return 0;
    if (fb->height > UINT64_MAX / fb->pitch) return 0;
    if (!mask_valid(fb->red_mask_size, fb->red_mask_shift) ||
        !mask_valid(fb->green_mask_size, fb->green_mask_shift) ||
        !mask_valid(fb->blue_mask_size, fb->blue_mask_shift)) return 0;
    return 1;
}

static int add_overflows_u64(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b;
}

static uint64_t pointer_to_phys(const void *pointer, uint64_t hhdm_offset) {
    if (!pointer) return 0;
    uint64_t address = (uint64_t)(uintptr_t)pointer;
    if (address < hhdm_offset) return 0;
    return address - hhdm_offset;
}

static boot_status_t find_framebuffer_phys(
    const struct limine_framebuffer *fb,
    uint64_t *start_out,
    uint64_t *end_out
) {
    if (!fb || !start_out || !end_out || !memmap_request.response) {
        return BOOT_NO_PHYSICAL_MAP;
    }

    uint64_t bytes = fb->pitch * fb->height;
    if (bytes == 0) return BOOT_NO_PHYSICAL_MAP;

    const struct limine_memmap_entry *match = 0;
    for (uint64_t i = 0; i < memmap_request.response->entry_count; ++i) {
        const struct limine_memmap_entry *entry = memmap_request.response->entries[i];
        if (!entry || entry->type != LIMINE_MEMMAP_FRAMEBUFFER ||
            entry->length < bytes || add_overflows_u64(entry->base, bytes)) {
            continue;
        }
        if (match) return BOOT_NO_PHYSICAL_MAP;
        match = entry;
    }

    if (!match) return BOOT_NO_PHYSICAL_MAP;
    *start_out = match->base;
    *end_out = match->base + bytes;
    return BOOT_OK;
}

static boot_status_t copy_memory_map(boot_context_t *context, uint64_t *usable_mib_out) {
    if (!context || !usable_mib_out || !memmap_request.response ||
        memmap_request.response->entry_count == 0 ||
        memmap_request.response->entry_count > BOOT_MEMORY_MAX_ENTRIES) {
        return BOOT_NO_MEMORY_MAP;
    }

    context->memory_map_count = 0;
    uint64_t usable_bytes = 0;

    for (uint64_t i = 0; i < memmap_request.response->entry_count; ++i) {
        const struct limine_memmap_entry *entry = memmap_request.response->entries[i];
        if (!entry || entry->length == 0) continue;
        if (add_overflows_u64(entry->base, entry->length)) {
            return BOOT_INVALID_BOOT_INFO;
        }

        boot_memory_region_t *out =
            &context->memory_map[context->memory_map_count++];
        out->base = entry->base;
        out->length = entry->length;
        out->type = entry->type == LIMINE_MEMMAP_USABLE
            ? BOOT_MEMORY_USABLE
            : BOOT_MEMORY_RESERVED;
        out->flags = 0;

        if (out->type == BOOT_MEMORY_USABLE) {
            if (add_overflows_u64(usable_bytes, out->length)) {
                usable_bytes = UINT64_MAX;
            } else {
                usable_bytes += out->length;
            }
        }
    }

    if (context->memory_map_count == 0) return BOOT_NO_MEMORY_MAP;
    *usable_mib_out = usable_bytes / (1024u * 1024u);
    return BOOT_OK;
}

boot_status_t boot_limine_context_init(boot_context_t *context) {
    if (!context) return BOOT_UNSUPPORTED_PROTOCOL;
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) return BOOT_UNSUPPORTED_PROTOCOL;
    if (!framebuffer_request.response || framebuffer_request.response->framebuffer_count < 1) {
        return BOOT_NO_FRAMEBUFFER;
    }
    if (!hhdm_request.response || !executable_address_request.response) {
        return BOOT_NO_PHYSICAL_MAP;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    if (!framebuffer_valid(fb)) return BOOT_UNSUPPORTED_FRAMEBUFFER;

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

    boot_status_t memory_status = copy_memory_map(context, &context->usable_memory_mib);
    if (memory_status != BOOT_OK) return memory_status;

    boot_status_t framebuffer_phys_status =
        find_framebuffer_phys(fb, &context->framebuffer_phys_start,
                              &context->framebuffer_phys_end);
    if (framebuffer_phys_status != BOOT_OK) return framebuffer_phys_status;

    if (add_overflows_u64(hhdm_request.response->offset,
                          context->framebuffer_phys_start)) {
        return BOOT_NO_PHYSICAL_MAP;
    }
    context->framebuffer.address = (void *)(uintptr_t)(
        hhdm_request.response->offset + context->framebuffer_phys_start);

    uint64_t kernel_size = (uint64_t)(uintptr_t)__kernel_end -
                           (uint64_t)(uintptr_t)__kernel_start;
    if (kernel_size == 0 ||
        add_overflows_u64(executable_address_request.response->physical_base, kernel_size) ||
        add_overflows_u64(executable_address_request.response->virtual_base, kernel_size)) {
        return BOOT_INVALID_BOOT_INFO;
    }

    context->physical_memory_offset = hhdm_request.response->offset;
    context->physical_memory_limit = BOOT_PHYSICAL_UNLIMITED;
    context->kernel_phys_start = executable_address_request.response->physical_base;
    context->kernel_phys_end = executable_address_request.response->physical_base + kernel_size;
    context->kernel_virt_start = executable_address_request.response->virtual_base;
    context->kernel_virt_end = executable_address_request.response->virtual_base + kernel_size;

    context->rsdp_phys = rsdp_request.response
        ? pointer_to_phys(rsdp_request.response->address, context->physical_memory_offset)
        : 0;

    context->smbios_phys = 0;
    if (smbios_request.response) {
        const void *entry = smbios_request.response->entry_64
            ? smbios_request.response->entry_64
            : smbios_request.response->entry_32;
        context->smbios_phys = pointer_to_phys(entry, context->physical_memory_offset);
    }

    return BOOT_OK;
}
