#include "timer.h"
#include "apic.h"
#include "interrupts.h"
#include "io.h"
#include <stdint.h>

#define PIT_COMMAND 0x43u
#define PIT_CHANNEL0 0x40u
#define PIT_RATE_GENERATOR 0x34u
#define TIMER_VECTOR 32u

static uint64_t tick_count;
static uint64_t nanoseconds_per_tick;
static uint32_t configured_hz;
static int initialised;

static void timer_interrupt(uint8_t vector, void *context) {
    (void)vector;
    (void)context;
    __atomic_add_fetch(&tick_count, 1u, __ATOMIC_RELAXED);
}

timer_status_t timer_init(uint32_t requested_hz) {
    uint16_t divisor;
    uint32_t actual_hz;
    uint64_t tick_ns;
    if (!timer_calculate_pit(
            requested_hz, &divisor, &actual_hz, &tick_ns)) {
        return TIMER_BAD_ARGUMENT;
    }

    if (!interrupts_register_handler(
            TIMER_VECTOR, timer_interrupt, 0)) {
        return TIMER_INTERRUPT_SETUP_FAILED;
    }

    apic_status_t route =
        apic_route_isa_irq(0, TIMER_VECTOR, 1);
    if (route != APIC_OK) return TIMER_ROUTE_FAILED;

    __atomic_store_n(&tick_count, 0, __ATOMIC_RELAXED);
    nanoseconds_per_tick = tick_ns;
    configured_hz = actual_hz;

    outb(PIT_COMMAND, PIT_RATE_GENERATOR);
    outb(PIT_CHANNEL0, (uint8_t)divisor);
    outb(PIT_CHANNEL0, (uint8_t)(divisor >> 8));

    route = apic_set_isa_irq_mask(0, 0);
    if (route != APIC_OK) return TIMER_ROUTE_FAILED;

    initialised = 1;
    return TIMER_OK;
}

uint64_t timer_ticks(void) {
    return __atomic_load_n(&tick_count, __ATOMIC_RELAXED);
}

uint32_t timer_frequency_hz(void) {
    return initialised ? configured_hz : 0;
}

uint64_t timer_now_ns(void) {
    uint64_t ticks = timer_ticks();
    if (!initialised || nanoseconds_per_tick == 0) return 0;
    if (ticks > UINT64_MAX / nanoseconds_per_tick) return UINT64_MAX;
    return ticks * nanoseconds_per_tick;
}

int timer_sleep_ms(uint64_t milliseconds) {
    if (!initialised || !interrupts_are_enabled()) return 0;
    if (milliseconds > UINT64_MAX / UINT64_C(1000000)) return 0;

    uint64_t duration = milliseconds * UINT64_C(1000000);
    uint64_t start = timer_now_ns();
    uint64_t deadline =
        UINT64_MAX - start < duration ? UINT64_MAX : start + duration;

    while (timer_now_ns() < deadline) {
        __asm__ volatile ("hlt");
    }
    return 1;
}

const char *timer_status_string(timer_status_t status) {
    switch (status) {
        case TIMER_OK: return "ok";
        case TIMER_BAD_ARGUMENT: return "bad PIT frequency";
        case TIMER_INTERRUPT_SETUP_FAILED: return "timer interrupt setup failed";
        case TIMER_ROUTE_FAILED: return "timer IOAPIC route failed";
        default: return "unknown timer error";
    }
}
