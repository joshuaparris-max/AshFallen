#ifndef JOSHOS_CPU_H
#define JOSHOS_CPU_H

#include <stdint.h>

typedef struct {
    uint32_t max_basic_leaf;
    uint32_t max_extended_leaf;
    uint32_t initial_apic_id;
    uint8_t has_msr;
    uint8_t has_apic;
    uint8_t has_x2apic;
    uint8_t has_tsc;
    uint8_t has_pae;
    uint8_t has_nx;
    uint8_t has_invariant_tsc;
    uint8_t has_smep;
    uint8_t has_smap;
} cpu_features_t;

typedef struct {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} cpu_cpuid_leaf_t;

cpu_features_t cpu_features_decode(
    uint32_t max_basic,
    uint32_t max_extended,
    cpu_cpuid_leaf_t leaf1,
    cpu_cpuid_leaf_t leaf7,
    cpu_cpuid_leaf_t ext1,
    cpu_cpuid_leaf_t ext7
);

cpu_features_t cpu_detect(void);
int cpu_required_features_present(const cpu_features_t *features);
int cpu_enable_nx(const cpu_features_t *features);
uint64_t cpu_read_msr(uint32_t msr);
void cpu_write_msr(uint32_t msr, uint64_t value);
uint64_t cpu_read_cr3(void);
void cpu_write_cr3(uint64_t root_phys);
uint64_t cpu_read_tsc(void);

#endif
