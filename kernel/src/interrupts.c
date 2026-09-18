#include "interrupts.h"
#include "serial.h"
#include <stdint.h>

#define IDT_ENTRIES 256
#define IDT_GATE_INTERRUPT 0x8E

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

typedef struct {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

static idt_entry_t idt[IDT_ENTRIES];

static __attribute__((noreturn)) void halt_forever(void) {
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

static uint16_t current_code_selector(void) {
    uint16_t selector;
    __asm__ volatile ("mov %%cs, %0" : "=r"(selector));
    return selector;
}

static void idt_set_gate(uint8_t vector, uintptr_t handler, uint16_t selector) {
    idt_entry_t *entry = &idt[vector];
    entry->offset_low = (uint16_t)(handler & 0xFFFFu);
    entry->selector = selector;
    entry->ist = 0;
    entry->type_attr = IDT_GATE_INTERRUPT;
    entry->offset_mid = (uint16_t)((handler >> 16) & 0xFFFFu);
    entry->offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFFu);
    entry->reserved = 0;
}

static __attribute__((noreturn)) void panic_exception(uint64_t vector,
                                                      uint64_t error_code,
                                                      const interrupt_frame_t *frame) {
    __asm__ volatile ("cli");

    serial_write("JOSHOS_PANIC_EXCEPTION\n");
    serial_write("VECTOR=");
    serial_write_hex64(vector);
    serial_write("\nERROR=");
    serial_write_hex64(error_code);
    serial_write("\nRIP=");
    serial_write_hex64(frame ? frame->rip : 0);
    serial_write("\nCS=");
    serial_write_hex64(frame ? frame->cs : 0);
    serial_write("\nRFLAGS=");
    serial_write_hex64(frame ? frame->rflags : 0);

    if (vector == 14) {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        serial_write("\nCR2=");
        serial_write_hex64(cr2);
    }

    serial_write("\nJOSHOS_PANIC_HALT\n");
    halt_forever();
}

#define EXCEPTION_NO_ERROR(name, vector) \
    __attribute__((interrupt)) static void name(interrupt_frame_t *frame) { \
        panic_exception((vector), 0, frame); \
    }

#define EXCEPTION_WITH_ERROR(name, vector) \
    __attribute__((interrupt)) static void name(interrupt_frame_t *frame, uint64_t error_code) { \
        panic_exception((vector), error_code, frame); \
    }

EXCEPTION_NO_ERROR(exception_0, 0)
EXCEPTION_NO_ERROR(exception_1, 1)
EXCEPTION_NO_ERROR(exception_2, 2)
EXCEPTION_NO_ERROR(exception_3, 3)
EXCEPTION_NO_ERROR(exception_4, 4)
EXCEPTION_NO_ERROR(exception_5, 5)
EXCEPTION_NO_ERROR(exception_6, 6)
EXCEPTION_NO_ERROR(exception_7, 7)
EXCEPTION_WITH_ERROR(exception_8, 8)
EXCEPTION_NO_ERROR(exception_9, 9)
EXCEPTION_WITH_ERROR(exception_10, 10)
EXCEPTION_WITH_ERROR(exception_11, 11)
EXCEPTION_WITH_ERROR(exception_12, 12)
EXCEPTION_WITH_ERROR(exception_13, 13)
EXCEPTION_WITH_ERROR(exception_14, 14)
EXCEPTION_NO_ERROR(exception_15, 15)
EXCEPTION_NO_ERROR(exception_16, 16)
EXCEPTION_WITH_ERROR(exception_17, 17)
EXCEPTION_NO_ERROR(exception_18, 18)
EXCEPTION_NO_ERROR(exception_19, 19)
EXCEPTION_NO_ERROR(exception_20, 20)
EXCEPTION_WITH_ERROR(exception_21, 21)
EXCEPTION_NO_ERROR(exception_22, 22)
EXCEPTION_NO_ERROR(exception_23, 23)
EXCEPTION_NO_ERROR(exception_24, 24)
EXCEPTION_NO_ERROR(exception_25, 25)
EXCEPTION_NO_ERROR(exception_26, 26)
EXCEPTION_NO_ERROR(exception_27, 27)
EXCEPTION_NO_ERROR(exception_28, 28)
EXCEPTION_WITH_ERROR(exception_29, 29)
EXCEPTION_WITH_ERROR(exception_30, 30)
EXCEPTION_NO_ERROR(exception_31, 31)

void interrupts_init(void) {
    /* Hardware IRQs stay disabled until the interrupt-controller milestone. */
    __asm__ volatile ("cli" ::: "memory");

    static const uintptr_t exception_handlers[32] = {
        (uintptr_t)exception_0,  (uintptr_t)exception_1,
        (uintptr_t)exception_2,  (uintptr_t)exception_3,
        (uintptr_t)exception_4,  (uintptr_t)exception_5,
        (uintptr_t)exception_6,  (uintptr_t)exception_7,
        (uintptr_t)exception_8,  (uintptr_t)exception_9,
        (uintptr_t)exception_10, (uintptr_t)exception_11,
        (uintptr_t)exception_12, (uintptr_t)exception_13,
        (uintptr_t)exception_14, (uintptr_t)exception_15,
        (uintptr_t)exception_16, (uintptr_t)exception_17,
        (uintptr_t)exception_18, (uintptr_t)exception_19,
        (uintptr_t)exception_20, (uintptr_t)exception_21,
        (uintptr_t)exception_22, (uintptr_t)exception_23,
        (uintptr_t)exception_24, (uintptr_t)exception_25,
        (uintptr_t)exception_26, (uintptr_t)exception_27,
        (uintptr_t)exception_28, (uintptr_t)exception_29,
        (uintptr_t)exception_30, (uintptr_t)exception_31
    };

    uint16_t selector = current_code_selector();
    for (uint8_t vector = 0; vector < 32; ++vector) {
        idt_set_gate(vector, exception_handlers[vector], selector);
    }

    idtr_t idtr = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base = (uint64_t)(uintptr_t)idt
    };
    __asm__ volatile ("lidt %0" :: "m"(idtr) : "memory");
}
