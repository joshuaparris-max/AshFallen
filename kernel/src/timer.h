#ifndef JOSHOS_TIMER_H
#define JOSHOS_TIMER_H

#include <stdint.h>

typedef enum {
    TIMER_OK = 0,
    TIMER_BAD_ARGUMENT,
    TIMER_INTERRUPT_SETUP_FAILED,
    TIMER_ROUTE_FAILED
} timer_status_t;

int timer_calculate_pit(
    uint32_t requested_hz,
    uint16_t *divisor_out,
    uint32_t *actual_hz_out,
    uint64_t *tick_ns_out
);

timer_status_t timer_init(uint32_t requested_hz);
uint64_t timer_ticks(void);
uint64_t timer_now_ns(void);
int timer_sleep_ms(uint64_t milliseconds);
const char *timer_status_string(timer_status_t status);

#endif
