#include "input.h"

#define INPUT_QUEUE_CAPACITY 128u
#define INPUT_QUEUE_MASK (INPUT_QUEUE_CAPACITY - 1u)

_Static_assert(
    (INPUT_QUEUE_CAPACITY & (INPUT_QUEUE_CAPACITY - 1u)) == 0,
    "input queue capacity must be a power of two"
);

static input_event_t queue[INPUT_QUEUE_CAPACITY];
static uint32_t head;
static uint32_t tail;
static uint64_t dropped;

void input_reset(void) {
    __atomic_store_n(&head, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&tail, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&dropped, 0u, __ATOMIC_RELAXED);
}

int input_push_isr(const input_event_t *event) {
    if (!event || event->type == INPUT_EVENT_NONE) return 0;

    uint32_t current_head =
        __atomic_load_n(&head, __ATOMIC_RELAXED);
    uint32_t next = (current_head + 1u) & INPUT_QUEUE_MASK;
    uint32_t current_tail =
        __atomic_load_n(&tail, __ATOMIC_ACQUIRE);

    if (next == current_tail) {
        __atomic_add_fetch(&dropped, 1u, __ATOMIC_RELAXED);
        return 0;
    }

    queue[current_head] = *event;
    __atomic_store_n(&head, next, __ATOMIC_RELEASE);
    return 1;
}

int input_pop(input_event_t *event) {
    if (!event) return 0;

    uint32_t current_tail =
        __atomic_load_n(&tail, __ATOMIC_RELAXED);
    uint32_t current_head =
        __atomic_load_n(&head, __ATOMIC_ACQUIRE);
    if (current_tail == current_head) return 0;

    *event = queue[current_tail];
    __atomic_store_n(
        &tail,
        (current_tail + 1u) & INPUT_QUEUE_MASK,
        __ATOMIC_RELEASE
    );
    return 1;
}

uint64_t input_dropped_events(void) {
    return __atomic_load_n(&dropped, __ATOMIC_RELAXED);
}
