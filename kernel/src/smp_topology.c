#include "smp.h"

int smp_build_topology(
    const acpi_platform_info_t *platform,
    uint32_t bsp_apic_id,
    smp_cpu_info_t *cpus,
    uint32_t capacity,
    uint32_t *count_out,
    uint32_t *bsp_index_out
) {
    if (!platform || !cpus || !count_out || !bsp_index_out ||
        capacity == 0 || platform->cpu_count == 0 ||
        platform->cpu_count > capacity ||
        platform->cpu_count > SMP_MAX_CPUS) {
        return 0;
    }

    uint32_t bsp_index = UINT32_MAX;
    for (uint32_t i = 0; i < platform->cpu_count; ++i) {
        const acpi_cpu_info_t *source = &platform->cpus[i];

        for (uint32_t j = 0; j < i; ++j) {
            if (cpus[j].apic_id == source->apic_id) return 0;
        }

        cpus[i].logical_index = i;
        cpus[i].apic_id = source->apic_id;
        cpus[i].acpi_processor_id = source->acpi_processor_id;
        cpus[i].state = source->apic_id == bsp_apic_id
            ? SMP_CPU_BSP_ONLINE
            : SMP_CPU_DISCOVERED;
        cpus[i].stack_base = 0;
        cpus[i].stack_top = 0;
        cpus[i].ipi_count = 0;

        if (source->apic_id == bsp_apic_id) {
            if (bsp_index != UINT32_MAX) return 0;
            bsp_index = i;
        }
    }

    if (bsp_index == UINT32_MAX) return 0;
    *count_out = platform->cpu_count;
    *bsp_index_out = bsp_index;
    return 1;
}
