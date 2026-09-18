#include "timer.h"
#include <stdint.h>
#include <stdio.h>

static int failures;
static void expect(const char *name, int ok) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

int main(void) {
    uint16_t divisor;
    uint32_t actual_hz;
    uint64_t tick_ns;

    expect("100 Hz parameters",
           timer_calculate_pit(100, &divisor, &actual_hz, &tick_ns));
    expect("100 Hz divisor", divisor == 11932u);
    expect("100 Hz actual near target", actual_hz >= 99u && actual_hz <= 100u);
    expect("100 Hz tick near 10ms",
           tick_ns >= UINT64_C(9990000) &&
           tick_ns <= UINT64_C(10010000));

    expect("1000 Hz parameters",
           timer_calculate_pit(1000, &divisor, &actual_hz, &tick_ns));
    expect("1000 Hz divisor", divisor == 1193u);
    expect("reject too slow",
           !timer_calculate_pit(1, &divisor, &actual_hz, &tick_ns));
    expect("reject null",
           !timer_calculate_pit(100, 0, &actual_hz, &tick_ns));

    if (failures) return 1;
    puts("PIT timer calculation tests passed");
    return 0;
}
