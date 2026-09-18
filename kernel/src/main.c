#include <stdint.h>
#include "boot.h"
#include "desktop.h"
#include "gfx.h"
#include "keyboard.h"
#include "serial.h"
#include "shell.h"

static void halt_forever(void) {
    for (;;) __asm__ volatile ("hlt");
}

static void report_boot_error(boot_status_t status) {
    switch (status) {
        case BOOT_UNSUPPORTED_PROTOCOL:
            serial_write("JOSHOS_ERROR_UNSUPPORTED_BOOT_PROTOCOL\n");
            break;
        case BOOT_INVALID_BOOT_INFO:
            serial_write("JOSHOS_ERROR_INVALID_BOOT_INFO\n");
            break;
        case BOOT_NO_MEMORY_MAP:
            serial_write("JOSHOS_ERROR_NO_MEMORY_MAP\n");
            break;
        case BOOT_NO_FRAMEBUFFER:
            serial_write("JOSHOS_ERROR_NO_FRAMEBUFFER\n");
            break;
        case BOOT_UNSUPPORTED_FRAMEBUFFER:
            serial_write("JOSHOS_ERROR_UNSUPPORTED_FRAMEBUFFER\n");
            break;
        case BOOT_OK:
        default:
            serial_write("JOSHOS_ERROR_UNKNOWN_BOOT_STATE\n");
            break;
    }
}

void kmain(uint64_t loader_magic1, uint64_t loader_magic2, const void *loader_payload) {
    serial_init();
    serial_write("JOSHOS_KERNEL_ENTERED\n");

    boot_context_t boot;
    boot_status_t status = boot_context_init(&boot, loader_magic1, loader_magic2, loader_payload);
    if (status != BOOT_OK) {
        report_boot_error(status);
        halt_forever();
    }

    serial_write("JOSHOS_BOOT_ADAPTER_OK\n");

    gfx_init(&boot.framebuffer);
    desktop_layout_t layout = desktop_draw();
    shell_init(layout.terminal_x, layout.terminal_y, layout.terminal_w, layout.terminal_h,
               boot.usable_memory_mib);

    serial_write("JOSHOS_BOOT_OK\n");

    for (;;) {
        char key = keyboard_poll();
        if (key) shell_handle_key(key);
        __asm__ volatile ("pause");
    }
}
