#include "keyboard.h"
#include "io.h"

static int shift_down;

static char translate(uint8_t scancode) {
    static const char normal[58] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b','\t',
        'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
        'a','s','d','f','g','h','j','k','l',';', '\'', '`',0,'\\',
        'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
    };
    static const char shifted[58] = {
        0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b','\t',
        'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
        'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
        'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
    };

    if (scancode >= 58) return 0;
    return shift_down ? shifted[scancode] : normal[scancode];
}

char keyboard_poll(void) {
    if ((inb(0x64) & 1) == 0) return 0;
    uint8_t scancode = inb(0x60);

    if (scancode == 0x2A || scancode == 0x36) {
        shift_down = 1;
        return 0;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_down = 0;
        return 0;
    }
    if (scancode & 0x80) return 0;
    return translate(scancode);
}

void keyboard_reboot(void) {
    while (inb(0x64) & 0x02) { }
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile ("hlt");
}
