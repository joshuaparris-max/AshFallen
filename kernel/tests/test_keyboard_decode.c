#include "keyboard_decode.h"
#include <stdio.h>

static int failures;

static void expect(const char *name, int ok) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

int main(void) {
    keyboard_decode_state_t state;
    input_event_t event;
    keyboard_decode_reset(&state);

    expect("a make", keyboard_decode_scancode(&state, 0x1e, &event));
    expect("a character",
           event.pressed && event.character == 'a' &&
           event.code == 0x1e);

    expect("a break", keyboard_decode_scancode(&state, 0x9e, &event));
    expect("a release",
           !event.pressed && event.character == 0);

    expect("shift make",
           keyboard_decode_scancode(&state, 0x2a, &event));
    expect("shift modifier set",
           (event.modifiers & INPUT_MOD_SHIFT) != 0);

    expect("shifted a",
           keyboard_decode_scancode(&state, 0x1e, &event));
    expect("A character", event.character == 'A');

    expect("shift break",
           keyboard_decode_scancode(&state, 0xaa, &event));
    expect("shift modifier cleared",
           (event.modifiers & INPUT_MOD_SHIFT) == 0);

    expect("extended prefix consumed",
           !keyboard_decode_scancode(&state, 0xe0, &event));
    expect("extended key",
           keyboard_decode_scancode(&state, 0x48, &event));
    expect("extended code",
           event.code == 0x148 && event.character == 0);

    expect("ctrl make",
           keyboard_decode_scancode(&state, 0x1d, &event));
    expect("ctrl set", (event.modifiers & INPUT_MOD_CTRL) != 0);
    expect("ctrl break",
           keyboard_decode_scancode(&state, 0x9d, &event));
    expect("ctrl clear", (event.modifiers & INPUT_MOD_CTRL) == 0);

    if (failures) return 1;
    puts("PS/2 keyboard decoder tests passed");
    return 0;
}
