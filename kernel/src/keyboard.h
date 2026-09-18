#ifndef JOSHOS_KEYBOARD_H
#define JOSHOS_KEYBOARD_H

#include <stdint.h>

typedef enum {
    KEYBOARD_OK = 0,
    KEYBOARD_CONTROLLER_TIMEOUT,
    KEYBOARD_INTERRUPT_SETUP_FAILED,
    KEYBOARD_ROUTE_FAILED
} keyboard_status_t;

keyboard_status_t keyboard_init(void);
uint64_t keyboard_interrupt_count(void);
char keyboard_poll(void);
void keyboard_reboot(void);
const char *keyboard_status_string(keyboard_status_t status);

#endif
