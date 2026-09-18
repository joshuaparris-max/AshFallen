#ifndef JOSHOS_SMP_H
#define JOSHOS_SMP_H

#include "acpi.h"
#include <stdint.h>

#define SMP_MAX_CPUS ACPI_MAX_CPUS
#define SMP_KERNEL_STACK_BYTES 32768u
#define SMP_IPI_VECTOR 254u

typedef enum {
    SMP_CPU_DISCOVERED = 0,
    SMP_CPU_BSP_ONLINE,
    SMP_CPU_STARTING,
    SMP_CPU_ONLINE,
    SMP_CPU_FAILED
} smp_cpu_state_t;

typedef struct {
    uint32_t logical_index;
    uint32_t apic_id;
    uint32_t acpi_processor_id;
    smp_cpu_state_t state;
    uint64_t stack_base;
    uint64_t stack_top;
    uint64_t ipi_count;
} smp_cpu_info_t;

typedef enum {
    SMP_OK = 0,
    SMP_BAD_ARGUMENT,
    SMP_BSP_NOT_FOUND,
    SMP_TOO_MANY_CPUS,
    SMP_NO_MEMORY,
    SMP_INTERRUPT_SETUP_FAILED,
    SMP_IPI_FAILED,
    SMP_TIMEOUT
} smp_status_t;

int smp_build_topology(
    const acpi_platform_info_t *platform,
    uint32_t bsp_apic_id,
    smp_cpu_info_t *cpus,
    uint32_t capacity,
    uint32_t *count_out,
    uint32_t *bsp_index_out
);

smp_status_t smp_init(
    const acpi_platform_info_t *platform,
    uint32_t bsp_apic_id
);
uint32_t smp_cpu_count(void);
uint32_t smp_online_cpu_count(void);
const smp_cpu_info_t *smp_cpu(uint32_t index);
const smp_cpu_info_t *smp_current_cpu(void);
int smp_self_ipi_test(void);
const char *smp_status_string(smp_status_t status);

#endif
