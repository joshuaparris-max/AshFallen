#include "smp.h"
#include "apic.h"
#include "heap.h"
#include "interrupts.h"
#include <stddef.h>
#include <stdint.h>

#define SMP_IPI_WAIT_SPINS 10000000u

static smp_cpu_info_t cpu_infos[SMP_MAX_CPUS];
static uint32_t cpu_count;
static uint32_t bsp_index;
static int initialised;

static void smp_ipi_interrupt(uint8_t vector, void *context) {
    (void)vector;
    (void)context;

    uint32_t local_id = apic_local_id();
    for (uint32_t i = 0; i < cpu_count; ++i) {
        if (cpu_infos[i].apic_id == local_id) {
            __atomic_add_fetch(
                &cpu_infos[i].ipi_count, 1u, __ATOMIC_RELAXED);
            return;
        }
    }
}

smp_status_t smp_init(
    const acpi_platform_info_t *platform,
    uint32_t bsp_apic_id
) {
    initialised = 0;
    cpu_count = 0;
    bsp_index = UINT32_MAX;

    if (!platform || platform->cpu_count == 0) return SMP_BAD_ARGUMENT;
    if (platform->cpu_count > SMP_MAX_CPUS) return SMP_TOO_MANY_CPUS;

    if (!smp_build_topology(
            platform,
            bsp_apic_id,
            cpu_infos,
            SMP_MAX_CPUS,
            &cpu_count,
            &bsp_index)) {
        return SMP_BSP_NOT_FOUND;
    }

    for (uint32_t i = 0; i < cpu_count; ++i) {
        void *stack = kmalloc(SMP_KERNEL_STACK_BYTES);
        if (!stack) return SMP_NO_MEMORY;
        cpu_infos[i].stack_base = (uint64_t)(uintptr_t)stack;
        cpu_infos[i].stack_top =
            cpu_infos[i].stack_base + SMP_KERNEL_STACK_BYTES;
    }

    if (!interrupts_register_handler(
            SMP_IPI_VECTOR, smp_ipi_interrupt, 0)) {
        return SMP_INTERRUPT_SETUP_FAILED;
    }

    initialised = 1;
    return SMP_OK;
}

uint32_t smp_cpu_count(void) {
    return initialised ? cpu_count : 0;
}

uint32_t smp_online_cpu_count(void) {
    if (!initialised) return 0;
    uint32_t online = 0;
    for (uint32_t i = 0; i < cpu_count; ++i) {
        if (cpu_infos[i].state == SMP_CPU_BSP_ONLINE ||
            cpu_infos[i].state == SMP_CPU_ONLINE) {
            online++;
        }
    }
    return online;
}

const smp_cpu_info_t *smp_cpu(uint32_t index) {
    if (!initialised || index >= cpu_count) return 0;
    return &cpu_infos[index];
}

const smp_cpu_info_t *smp_current_cpu(void) {
    if (!initialised) return 0;
    uint32_t local_id = apic_local_id();
    for (uint32_t i = 0; i < cpu_count; ++i) {
        if (cpu_infos[i].apic_id == local_id) return &cpu_infos[i];
    }
    return 0;
}

int smp_self_ipi_test(void) {
    if (!initialised || bsp_index >= cpu_count ||
        !interrupts_are_enabled()) {
        return 0;
    }

    smp_cpu_info_t *bsp = &cpu_infos[bsp_index];
    uint64_t before =
        __atomic_load_n(&bsp->ipi_count, __ATOMIC_RELAXED);

    if (apic_send_ipi(bsp->apic_id, SMP_IPI_VECTOR) != APIC_OK) {
        return 0;
    }

    for (uint32_t i = 0; i < SMP_IPI_WAIT_SPINS; ++i) {
        if (__atomic_load_n(
                &bsp->ipi_count, __ATOMIC_RELAXED) > before) {
            return 1;
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

const char *smp_status_string(smp_status_t status) {
    switch (status) {
        case SMP_OK: return "ok";
        case SMP_BAD_ARGUMENT: return "bad argument";
        case SMP_BSP_NOT_FOUND: return "BSP missing from ACPI topology";
        case SMP_TOO_MANY_CPUS: return "CPU topology exceeds kernel limit";
        case SMP_NO_MEMORY: return "per-CPU stack allocation failed";
        case SMP_INTERRUPT_SETUP_FAILED: return "IPI interrupt setup failed";
        case SMP_IPI_FAILED: return "IPI send failed";
        case SMP_TIMEOUT: return "SMP operation timed out";
        default: return "unknown SMP error";
    }
}
