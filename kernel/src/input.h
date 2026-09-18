#ifndef JOSHOS_INPUT_H
#define JOSHOS_INPUT_H

#include <stdint.h>

typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEY = 1,
    INPUT_EVENT_POINTER = 2
} input_event_type_t;

#define INPUT_MOD_SHIFT (1u << 0)
#define INPUT_MOD_CTRL  (1u << 1)
#define INPUT_MOD_ALT   (1u << 2)

typedef struct {
    input_event_type_t type;
    uint16_t code;
    uint16_t modifiers;
    int32_t value_x;
    int32_t value_y;
    uint8_t pressed;
    char character;
} input_event_t;

void input_reset(void);
int input_push_isr(const input_event_t *event);
int input_pop(input_event_t *event);
uint64_t input_dropped_events(void);

#endif
