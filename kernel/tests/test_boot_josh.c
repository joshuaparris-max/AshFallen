#include "boot_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void valid_info(JoshBootInfo *info, JoshMemoryMapEntry *entries, uint32_t *framebuffer) {
    memset(info, 0, sizeof(*info));
    memset(entries, 0, sizeof(JoshMemoryMapEntry) * 2);
    info->magic = JOSH_BOOT_INFO_MAGIC;
    info->abi_major = JOSH_BOOT_ABI_MAJOR;
    info->abi_minor = JOSH_BOOT_ABI_MINOR;
    info->total_size = sizeof(*info);
    info->flags = JOSH_BOOT_FLAG_MEMORY_MAP | JOSH_BOOT_FLAG_FRAMEBUFFER;
    info->memory_map_address = (uintptr_t)entries;
    info->memory_map_entries = 2;
    info->memory_map_entry_size = sizeof(*entries);
    entries[0].base = 0x100000;
    entries[0].length = 64u * 1024u * 1024u;
    entries[0].type = JOSH_MEMORY_USABLE;
    entries[1].base = 0;
    entries[1].length = 0x100000;
    entries[1].type = JOSH_MEMORY_RESERVED;
    info->framebuffer.address = (uintptr_t)framebuffer;
    info->framebuffer.width = 8;
    info->framebuffer.height = 8;
    info->framebuffer.pitch = 32;
    info->framebuffer.bpp = 32;
    info->framebuffer.red_mask_size = 8; info->framebuffer.red_mask_shift = 16;
    info->framebuffer.green_mask_size = 8; info->framebuffer.green_mask_shift = 8;
    info->framebuffer.blue_mask_size = 8; info->framebuffer.blue_mask_shift = 0;
    info->kernel_phys_start = 0x200000;
    info->kernel_phys_end = 0x210000;
    info->kernel_virt_start = UINT64_C(0xffffffff80000000);
    info->kernel_virt_end = UINT64_C(0xffffffff80010000);
    info->kernel_entry = UINT64_C(0xffffffff80001000);
}

int main(void) {
    JoshBootInfo info;
    JoshMemoryMapEntry entries[2];
    uint32_t framebuffer[64];
    boot_context_t context;

    valid_info(&info, entries, framebuffer);
    info.framebuffer.address = 0xe0000000u;
    expect("valid boot info", boot_josh_context_init(&context, &info) == BOOT_OK);
    expect("usable memory", context.usable_memory_mib == 64);
    expect("framebuffer copied", (uintptr_t)context.framebuffer.address == 0xe0000000u);
    expect("kernel physical base copied", context.kernel_phys_base == 0x200000u);
    expect("kernel virtual base copied", context.kernel_virt_base == UINT64_C(0xffffffff80000000));

    valid_info(&info, entries, framebuffer);
    info.magic ^= 1;
    expect("bad magic rejected", boot_josh_context_init(&context, &info) == BOOT_INVALID_BOOT_INFO);

    valid_info(&info, entries, framebuffer);
    info.flags &= ~JOSH_BOOT_FLAG_MEMORY_MAP;
    expect("missing required map flag rejected", boot_josh_context_init(&context, &info) == BOOT_INVALID_BOOT_INFO);

    valid_info(&info, entries, framebuffer);
    info.framebuffer.address = 0xe0000000u;
    info.framebuffer.pitch = 4;
    expect("bad framebuffer pitch rejected", boot_josh_context_init(&context, &info) == BOOT_UNSUPPORTED_FRAMEBUFFER);

    valid_info(&info, entries, framebuffer);
    info.framebuffer.address = 0xe0000000u;
    entries[0].base = UINT64_MAX - 10;
    entries[0].length = 20;
    expect("overflowing memory entry rejected", boot_josh_context_init(&context, &info) == BOOT_INVALID_BOOT_INFO);

    if (failures) return 1;
    puts("Josh Boot Protocol adapter tests passed");
    return 0;
}
