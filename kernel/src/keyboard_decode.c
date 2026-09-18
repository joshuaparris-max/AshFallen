#include "keyboard_decode.h"

static char translate(uint8_t code, int shifted) {
    static const char normal[58] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b','\t',
        'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
        'a','s','d','f','g','h','j','k','l',';', '\'', '`',0,'\\',
        'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
    };
    static const char shifted_map[58] = {
        0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b','\t',
        'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
        'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
        'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
    };

    if (code >= 58) return 0;
    return shifted ? shifted_map[code] : normal[code];
}

void keyboard_decode_reset(keyboard_decode_state_t *state) {
    if (!state) return;
    state->modifiers = 0;
    state->extended = 0;
}

int keyboard_decode_scancode(
    keyboard_decode_state_t *state,
    uint8_t scancode,
    input_event_t *event_out
) {
    if (!state || !event_out) return 0;

    if (scancode == 0xe0) {
        state->extended = 1;
        return 0;
    }
    if (scancode == 0xe1) {
        state->extended = 0;
        return 0;
    }

    uint8_t released = (scancode & 0x80u) != 0;
    uint8_t code = scancode & 0x7fu;
    uint16_t extended_bit = state->extended ? UINT16_C(0x100) : 0;
    state->extended = 0;

    if (code == 0x2a || code == 0x36) {
        if (released) state->modifiers &= ~INPUT_MOD_SHIFT;
        else state->modifiers |= INPUT_MOD_SHIFT;
    } else if (code == 0x1d) {
        if (released) state->modifiers &= ~INPUT_MOD_CTRL;
        else state->modifiers |= INPUT_MOD_CTRL;
    } else if (code == 0x38) {
        if (released) state->modifiers &= ~INPUT_MOD_ALT;
        else state->modifiers |= INPUT_MOD_ALT;
    }

    event_out->type = INPUT_EVENT_KEY;
    event_out->code = extended_bit | code;
    event_out->modifiers = state->modifiers;
    event_out->value_x = 0;
    event_out->value_y = 0;
    event_out->pressed = released ? 0 : 1;
    event_out->character = (!released && extended_bit == 0)
        ? translate(code, (state->modifiers & INPUT_MOD_SHIFT) != 0)
        : 0;
    return 1;
}
