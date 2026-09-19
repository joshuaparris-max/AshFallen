#include "cpu.h"
#include <stdio.h>

static int failures;

static void expect(const char *name, int ok) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

int main(void) {
    cpu_cpuid_leaf_t leaf1 = {
        .ebx = UINT32_C(0x2a000000),
        .ecx = UINT32_C(1) << 21,
        .edx = (UINT32_C(1) << 4) | (UINT32_C(1) << 5) |
               (UINT32_C(1) << 6) | (UINT32_C(1) << 9)
    };
    cpu_cpuid_leaf_t leaf7 = {
        .ebx = (UINT32_C(1) << 7) | (UINT32_C(1) << 20)
    };
    cpu_cpuid_leaf_t ext1 = {.edx = UINT32_C(1) << 20};
    cpu_cpuid_leaf_t ext7 = {.edx = UINT32_C(1) << 8};

    cpu_features_t f = cpu_features_decode(
        7, UINT32_C(0x80000008), leaf1, leaf7, ext1, ext7);

    expect("APIC id", f.initial_apic_id == 0x2a);
    expect("required features", cpu_required_features_present(&f));
    expect("x2APIC", f.has_x2apic);
    expect("NX", f.has_nx);
    expect("invariant TSC", f.has_invariant_tsc);
    expect("SMEP", f.has_smep);
    expect("SMAP", f.has_smap);

    ext1.edx = 0;
    f = cpu_features_decode(
        7, UINT32_C(0x80000008), leaf1, leaf7, ext1, ext7);
    expect("NX required", !cpu_required_features_present(&f));

    leaf1.edx &= ~(UINT32_C(1) << 9);
    f = cpu_features_decode(
        7, UINT32_C(0x80000008), leaf1, leaf7, ext1, ext7);
    expect("APIC required", !cpu_required_features_present(&f));

    cpu_features_t detected = cpu_detect();
    expect("host CPUID detection", detected.max_basic_leaf >= 1);

    if (failures) return 1;
    puts("CPU feature decode tests passed");
    return 0;
}
