#include "interrupts.h"
#include "apic.h"
#include "gdt.h"
#include "serial.h"
#include <stdint.h>

#define IDT_ENTRIES 256
#define IDT_GATE_INTERRUPT 0x8E
#define IDT_GATE_USER_INTERRUPT 0xEE

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

extern const uintptr_t exception_stub_table[32];
extern void irq_stub_32(void);
extern void irq_stub_33(void);
extern void irq_stub_254(void);
extern void irq_stub_255(void);
extern void syscall_entry(void);

_Static_assert(__builtin_offsetof(exception_frame_t, rax) == 0, "exception frame rax offset");
_Static_assert(__builtin_offsetof(exception_frame_t, r15) == 112, "exception frame r15 offset");
_Static_assert(__builtin_offsetof(exception_frame_t, vector) == 120, "exception frame vector offset");
_Static_assert(__builtin_offsetof(exception_frame_t, error_code) == 128, "exception frame error offset");
_Static_assert(__builtin_offsetof(exception_frame_t, rip) == 136, "exception frame rip offset");
_Static_assert(__builtin_offsetof(exception_frame_t, rflags) == 152, "exception frame rflags offset");
_Static_assert(__builtin_offsetof(exception_frame_t, stack_rsp) == 160, "exception frame stack offset");

static idt_entry_t idt[IDT_ENTRIES];
static interrupt_handler_t handlers[IDT_ENTRIES];
static void *handler_contexts[IDT_ENTRIES];
static uint64_t unhandled_count;

static __attribute__((noreturn)) void halt_forever(void) {
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

static uint16_t current_code_selector(void) {
    uint16_t selector;
    __asm__ volatile ("mov %%cs, %0" : "=r"(selector));
    return selector;
}

static void idt_set_gate(uint8_t vector, uintptr_t handler, uint16_t selector, uint8_t ist) {
    idt_entry_t *entry = &idt[vector];
    entry->offset_low = (uint16_t)(handler & 0xFFFFu);
    entry->selector = selector;
    entry->ist = ist & 0x07u;
    entry->type_attr = IDT_GATE_INTERRUPT;
    entry->offset_mid = (uint16_t)((handler >> 16) & 0xFFFFu);
    entry->offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFFu);
    entry->reserved = 0;
}

static void serial_field(const char *name, uint64_t value) {
    serial_write(name);
    serial_write("=");
    serial_write_hex64(value);
    serial_write("\n");
}

static int exception_has_saved_stack(const exception_frame_t *frame) {
    return frame->vector == 8 || (frame->cs & 3u) != 0;
}

static uint64_t interrupted_rsp(const exception_frame_t *frame) {
    if (exception_has_saved_stack(frame)) {
        return frame->stack_rsp;
    }

    /*
     * Without a CPU stack switch, the address where old RSP would have been
     * pushed is exactly the interrupted RSP: immediately above RFLAGS.
     */
    return (uint64_t)(uintptr_t)&frame->stack_rsp;
}

__attribute__((noreturn)) void exception_dispatch(exception_frame_t *frame) {
    __asm__ volatile ("cli");

    serial_write("JOSHOS_PANIC_EXCEPTION\n");
    serial_field("VECTOR", frame->vector);
    serial_field("ERROR", frame->error_code);
    serial_field("RIP", frame->rip);
    serial_field("CS", frame->cs);
    serial_field("RFLAGS", frame->rflags);

    serial_write("JOSHOS_REGISTER_DUMP\n");
    serial_field("RAX", frame->rax);
    serial_field("RBX", frame->rbx);
    serial_field("RCX", frame->rcx);
    serial_field("RDX", frame->rdx);
    serial_field("RSI", frame->rsi);
    serial_field("RDI", frame->rdi);
    serial_field("RBP", frame->rbp);
    serial_field("RSP", interrupted_rsp(frame));
    serial_field("R8", frame->r8);
    serial_field("R9", frame->r9);
    serial_field("R10", frame->r10);
    serial_field("R11", frame->r11);
    serial_field("R12", frame->r12);
    serial_field("R13", frame->r13);
    serial_field("R14", frame->r14);
    serial_field("R15", frame->r15);

    if (exception_has_saved_stack(frame)) {
        serial_field("SS", frame->stack_ss);
    }

    if (frame->vector == 14) {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        serial_field("CR2", cr2);
    }

    serial_write("JOSHOS_PANIC_HALT\n");
    halt_forever();
}

uint64_t interrupts_save_disable(void) {
    uint64_t flags;
    __asm__ volatile (
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );
    return flags;
}

void interrupts_restore(uint64_t flags) {
    if ((flags & (UINT64_C(1) << 9)) != 0) {
        __asm__ volatile ("sti" ::: "memory");
    }
}

void interrupts_enable(void) {
    __asm__ volatile ("sti" ::: "memory");
}

void interrupts_disable(void) {
    __asm__ volatile ("cli" ::: "memory");
}

int interrupts_are_enabled(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags));
    return (flags & (UINT64_C(1) << 9)) != 0;
}

int interrupts_register_handler(
    uint8_t vector,
    interrupt_handler_t handler,
    void *context
) {
    if (vector < 32 || !handler) return 0;

    uint64_t flags = interrupts_save_disable();
    handlers[vector] = handler;
    handler_contexts[vector] = context;
    interrupts_restore(flags);
    return 1;
}

uint64_t interrupts_unhandled_count(void) {
    return unhandled_count;
}

void interrupt_dispatch(uint64_t vector) {
    if (vector >= IDT_ENTRIES) return;

    interrupt_handler_t handler = handlers[vector];
    if (handler) {
        handler((uint8_t)vector, handler_contexts[vector]);
    } else if (vector != 255) {
        unhandled_count++;
    }

    if (vector >= 32 && vector < 255) {
        apic_eoi();
    }
}

void interrupts_init(void) {
    /* Hardware IRQs stay disabled until the interrupt-controller milestone. */
    __asm__ volatile ("cli" ::: "memory");

    uint16_t selector = current_code_selector();
    for (uint8_t vector = 0; vector < 32; ++vector) {
        uint8_t ist = vector == 8 ? GDT_DOUBLE_FAULT_IST_INDEX : 0;
        idt_set_gate(vector, exception_stub_table[vector], selector, ist);
    }

    idt_set_gate(32, (uintptr_t)irq_stub_32, selector, 0);
    idt_set_gate(33, (uintptr_t)irq_stub_33, selector, 0);
    idt_set_gate(254, (uintptr_t)irq_stub_254, selector, 0);
    idt_set_gate(255, (uintptr_t)irq_stub_255, selector, 0);

    idt_set_gate(0x80u, (uintptr_t)syscall_entry, selector, 0);
    idt[0x80u].type_attr = IDT_GATE_USER_INTERRUPT;

    idtr_t idtr = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base = (uint64_t)(uintptr_t)idt
    };
    __asm__ volatile ("lidt %0" :: "m"(idtr) : "memory");
}

#ifdef JOSHOS_FAULT_TEST_DOUBLE_FAULT
void interrupts_arm_double_fault_test(void) {
    /*
     * Make #GP delivery itself fail by replacing its code selector with the
     * null selector. Triggering #GP after this forces the CPU down the #DF path.
     * Vector 8 remains valid and uses TSS IST1, so the panic path gets a fresh
     * emergency stack instead of recursively using the damaged delivery path.
     */
    idt[13].selector = 0;
}
#endif
