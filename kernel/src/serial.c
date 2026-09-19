#include "serial.h"
#include "io.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0) { }
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *text) {
    while (text && *text) {
        if (*text == '\n') serial_putc('\r');
        serial_putc(*text++);
    }
}

void serial_write_n(const char *text, uint64_t length) {
    if (!text) return;
    for (uint64_t i = 0; i < length; ++i) {
        char ch = text[i];
        if (ch == '\n') serial_putc('\r');
        serial_putc(ch);
    }
}

void serial_write_hex64(uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_putc(digits[(value >> shift) & 0x0Fu]);
    }
}
