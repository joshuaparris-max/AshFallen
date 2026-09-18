#include "smp.h"
#include <stdio.h>
#include <string.h>

static int failures;
static void expect(const char *name, int ok) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

int main(void) {
    acpi_platform_info_t platform;
    memset(&platform, 0, sizeof(platform));
    platform.cpu_count = 4;
    for (uint32_t i = 0; i < platform.cpu_count; ++i) {
        platform.cpus[i].acpi_processor_id = 100u + i;
        platform.cpus[i].apic_id = 8u + i;
        platform.cpus[i].flags = 1;
    }

    smp_cpu_info_t cpus[SMP_MAX_CPUS];
    uint32_t count = 0;
    uint32_t bsp = UINT32_MAX;

    expect("topology build",
           smp_build_topology(
               &platform, 10, cpus, SMP_MAX_CPUS, &count, &bsp));
    expect("count", count == 4);
    expect("BSP index", bsp == 2);
    expect("BSP state", cpus[2].state == SMP_CPU_BSP_ONLINE);
    expect("AP discovered",
           cpus[0].state == SMP_CPU_DISCOVERED &&
           cpus[1].state == SMP_CPU_DISCOVERED &&
           cpus[3].state == SMP_CPU_DISCOVERED);
    expect("identity copied",
           cpus[3].apic_id == 11 &&
           cpus[3].acpi_processor_id == 103);

    expect("missing BSP rejected",
           !smp_build_topology(
               &platform, 99, cpus, SMP_MAX_CPUS, &count, &bsp));

    platform.cpus[3].apic_id = platform.cpus[2].apic_id;
    expect("duplicate APIC id rejected",
           !smp_build_topology(
               &platform, 10, cpus, SMP_MAX_CPUS, &count, &bsp));

    if (failures) return 1;
    puts("SMP topology tests passed");
    return 0;
}
