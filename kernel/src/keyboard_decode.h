#ifndef JOSHOS_KEYBOARD_DECODE_H
#define JOSHOS_KEYBOARD_DECODE_H

#include "input.h"
#include <stdint.h>

typedef struct {
    uint16_t modifiers;
    uint8_t extended;
} keyboard_decode_state_t;

void keyboard_decode_reset(keyboard_decode_state_t *state);
int keyboard_decode_scancode(
    keyboard_decode_state_t *state,
    uint8_t scancode,
    input_event_t *event_out
);

#endif
