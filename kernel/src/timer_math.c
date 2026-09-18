#include "timer.h"

#define PIT_INPUT_HZ UINT64_C(1193182)

int timer_calculate_pit(
    uint32_t requested_hz,
    uint16_t *divisor_out,
    uint32_t *actual_hz_out,
    uint64_t *tick_ns_out
) {
    if (!divisor_out || !actual_hz_out || !tick_ns_out ||
        requested_hz < 19u || requested_hz > 10000u) {
        return 0;
    }

    uint64_t divisor =
        (PIT_INPUT_HZ + requested_hz / 2u) / requested_hz;
    if (divisor == 0 || divisor > UINT16_MAX) return 0;

    uint64_t tick_ns =
        (divisor * UINT64_C(1000000000)) / PIT_INPUT_HZ;
    if (tick_ns == 0) return 0;

    *divisor_out = (uint16_t)divisor;
    *actual_hz_out = (uint32_t)(PIT_INPUT_HZ / divisor);
    *tick_ns_out = tick_ns;
    return 1;
}
