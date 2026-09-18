#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "desktop.h"
#include "gfx.h"
#include "keyboard.h"
#include "serial.h"
#include "shell.h"

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

static void halt_forever(void) {
    for (;;) __asm__ volatile ("hlt");
}

static uint64_t usable_memory_mib(void) {
    if (!memmap_request.response) return 0;
    uint64_t bytes = 0;
    for (uint64_t i = 0; i < memmap_request.response->entry_count; ++i) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) bytes += entry->length;
    }
    return bytes / (1024 * 1024);
}

void kmain(void) {
    serial_init();
    serial_write("JOSHOS_KERNEL_ENTERED\n");

    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        serial_write("JOSHOS_ERROR_UNSUPPORTED_LIMINE_REVISION\n");
        halt_forever();
    }

    if (!framebuffer_request.response || framebuffer_request.response->framebuffer_count < 1) {
        serial_write("JOSHOS_ERROR_NO_FRAMEBUFFER\n");
        halt_forever();
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    if (!fb || fb->memory_model != LIMINE_FRAMEBUFFER_RGB || fb->bpp != 32) {
        serial_write("JOSHOS_ERROR_UNSUPPORTED_FRAMEBUFFER\n");
        halt_forever();
    }

    gfx_init(fb);
    desktop_layout_t layout = desktop_draw();
    shell_init(layout.terminal_x, layout.terminal_y, layout.terminal_w, layout.terminal_h,
               usable_memory_mib());

    serial_write("JOSHOS_BOOT_OK\n");

    for (;;) {
        char key = keyboard_poll();
        if (key) shell_handle_key(key);
        __asm__ volatile ("pause");
    }
}
