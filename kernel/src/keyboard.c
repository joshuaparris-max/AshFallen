#include "keyboard.h"
#include "apic.h"
#include "input.h"
#include "interrupts.h"
#include "io.h"
#include "keyboard_decode.h"

#define PS2_DATA 0x60u
#define PS2_STATUS_COMMAND 0x64u
#define PS2_STATUS_OUTPUT_FULL 0x01u
#define PS2_STATUS_INPUT_FULL 0x02u
#define PS2_COMMAND_READ_CONFIG 0x20u
#define PS2_COMMAND_WRITE_CONFIG 0x60u
#define PS2_CONFIG_IRQ1 0x01u
#define PS2_CONFIG_DISABLE_KEYBOARD 0x10u
#define PS2_CONFIG_TRANSLATION 0x40u
#define KEYBOARD_VECTOR 33u
#define PS2_WAIT_LIMIT 100000u

static keyboard_decode_state_t decode_state;
static uint64_t irq_count;

static int wait_input_empty(void) {
    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i) {
        if ((inb(PS2_STATUS_COMMAND) & PS2_STATUS_INPUT_FULL) == 0) {
            return 1;
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

static int wait_output_full(void) {
    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i) {
        if ((inb(PS2_STATUS_COMMAND) & PS2_STATUS_OUTPUT_FULL) != 0) {
            return 1;
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

static void drain_output(void) {
    for (uint32_t i = 0; i < 32u; ++i) {
        if ((inb(PS2_STATUS_COMMAND) & PS2_STATUS_OUTPUT_FULL) == 0) {
            return;
        }
        (void)inb(PS2_DATA);
    }
}

static void keyboard_interrupt(uint8_t vector, void *context) {
    (void)vector;
    (void)context;

    uint8_t status = inb(PS2_STATUS_COMMAND);
    if ((status & PS2_STATUS_OUTPUT_FULL) == 0) return;

    uint8_t scancode = inb(PS2_DATA);
    input_event_t event;
    if (keyboard_decode_scancode(&decode_state, scancode, &event)) {
        (void)input_push_isr(&event);
    }
    __atomic_add_fetch(&irq_count, 1u, __ATOMIC_RELAXED);
}

keyboard_status_t keyboard_init(void) {
    keyboard_decode_reset(&decode_state);
    __atomic_store_n(&irq_count, 0, __ATOMIC_RELAXED);
    drain_output();

    if (!wait_input_empty()) return KEYBOARD_CONTROLLER_TIMEOUT;
    outb(PS2_STATUS_COMMAND, PS2_COMMAND_READ_CONFIG);
    if (!wait_output_full()) return KEYBOARD_CONTROLLER_TIMEOUT;

    uint8_t config = inb(PS2_DATA);
    config |= PS2_CONFIG_IRQ1 | PS2_CONFIG_TRANSLATION;
    config &= (uint8_t)~PS2_CONFIG_DISABLE_KEYBOARD;

    if (!wait_input_empty()) return KEYBOARD_CONTROLLER_TIMEOUT;
    outb(PS2_STATUS_COMMAND, PS2_COMMAND_WRITE_CONFIG);
    if (!wait_input_empty()) return KEYBOARD_CONTROLLER_TIMEOUT;
    outb(PS2_DATA, config);

    if (!interrupts_register_handler(
            KEYBOARD_VECTOR, keyboard_interrupt, 0)) {
        return KEYBOARD_INTERRUPT_SETUP_FAILED;
    }

    apic_status_t route =
        apic_route_isa_irq(1, KEYBOARD_VECTOR, 0);
    if (route != APIC_OK) return KEYBOARD_ROUTE_FAILED;

    return KEYBOARD_OK;
}

uint64_t keyboard_interrupt_count(void) {
    return __atomic_load_n(&irq_count, __ATOMIC_RELAXED);
}

char keyboard_poll(void) {
    input_event_t event;
    while (input_pop(&event)) {
        if (event.type == INPUT_EVENT_KEY &&
            event.pressed &&
            event.character != 0) {
            return event.character;
        }
    }
    return 0;
}

void keyboard_reboot(void) {
    interrupts_disable();
    if (wait_input_empty()) {
        outb(PS2_STATUS_COMMAND, 0xfe);
    }
    for (;;) __asm__ volatile ("hlt");
}

const char *keyboard_status_string(keyboard_status_t status) {
    switch (status) {
        case KEYBOARD_OK: return "ok";
        case KEYBOARD_CONTROLLER_TIMEOUT: return "PS2 controller timeout";
        case KEYBOARD_INTERRUPT_SETUP_FAILED: return "keyboard interrupt setup failed";
        case KEYBOARD_ROUTE_FAILED: return "keyboard IOAPIC route failed";
        default: return "unknown keyboard error";
    }
}
