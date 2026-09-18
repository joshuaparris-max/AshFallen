#include "apic.h"
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
    uint64_t entry = 0;

    expect("ISA conforming route",
           apic_build_redirection(0x21, 2, 0, 0, &entry));
    expect("vector", (entry & 0xffu) == 0x21);
    expect("destination", (entry >> 56) == 2);
    expect("conforming high edge",
           (entry & ((UINT64_C(1) << 13) | (UINT64_C(1) << 15))) == 0);
    expect("unmasked", (entry & (UINT64_C(1) << 16)) == 0);

    expect("low level route",
           apic_build_redirection(0x20, 7, 0x000f, 1, &entry));
    expect("active low", (entry & (UINT64_C(1) << 13)) != 0);
    expect("level", (entry & (UINT64_C(1) << 15)) != 0);
    expect("masked", (entry & (UINT64_C(1) << 16)) != 0);

    expect("explicit high edge",
           apic_build_redirection(0x22, 1, 0x0005, 0, &entry));
    expect("explicit high edge bits",
           (entry & ((UINT64_C(1) << 13) | (UINT64_C(1) << 15))) == 0);

    expect("reserved polarity rejected",
           !apic_build_redirection(0x20, 0, 0x0002, 0, &entry));
    expect("reserved trigger rejected",
           !apic_build_redirection(0x20, 0, 0x0008, 0, &entry));
    expect("exception vector rejected",
           !apic_build_redirection(0x1f, 0, 0, 0, &entry));
    expect("spurious vector rejected",
           !apic_build_redirection(0xff, 0, 0, 0, &entry));
    expect("xAPIC destination bound",
           !apic_build_redirection(0x20, 256, 0, 0, &entry));

    if (failures) return 1;
    puts("IOAPIC route encoding tests passed");
    return 0;
}
