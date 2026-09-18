#include "input.h"
#include <stdio.h>

static int failures;

static void expect(const char *name, int ok) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

int main(void) {
    input_reset();

    input_event_t event = {
        .type = INPUT_EVENT_KEY,
        .code = 0x1e,
        .modifiers = INPUT_MOD_SHIFT,
        .pressed = 1,
        .character = 'A'
    };
    expect("push", input_push_isr(&event));

    input_event_t out;
    expect("pop", input_pop(&out));
    expect("payload",
           out.type == INPUT_EVENT_KEY &&
           out.code == 0x1e &&
           out.modifiers == INPUT_MOD_SHIFT &&
           out.pressed == 1 &&
           out.character == 'A');
    expect("empty", !input_pop(&out));

    input_reset();
    for (unsigned i = 0; i < 127; ++i) {
        event.code = (uint16_t)i;
        expect("fill queue", input_push_isr(&event));
    }
    expect("full queue rejects", !input_push_isr(&event));
    expect("drop counted", input_dropped_events() == 1);

    for (unsigned i = 0; i < 127; ++i) {
        expect("drain queue", input_pop(&out));
        expect("FIFO order", out.code == i);
    }
    expect("drained", !input_pop(&out));

    if (failures) return 1;
    puts("input queue tests passed");
    return 0;
}
