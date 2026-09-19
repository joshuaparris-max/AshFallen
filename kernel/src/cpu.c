#include "cpu.h"

#define CPUID_FEAT_EDX_TSC   (1u << 4)
#define CPUID_FEAT_EDX_MSR   (1u << 5)
#define CPUID_FEAT_EDX_PAE   (1u << 6)
#define CPUID_FEAT_EDX_APIC  (1u << 9)
#define CPUID_FEAT_ECX_X2APIC (1u << 21)
#define CPUID_EXT_EDX_NX     (1u << 20)
#define CPUID_7_EBX_SMEP     (1u << 7)
#define CPUID_7_EBX_SMAP     (1u << 20)
#define CPUID_EXT7_EDX_INVARIANT_TSC (1u << 8)

#define MSR_EFER UINT32_C(0xc0000080)
#define EFER_NXE (UINT64_C(1) << 11)

static cpu_cpuid_leaf_t cpuid_count(uint32_t leaf, uint32_t subleaf) {
    cpu_cpuid_leaf_t out;
    __asm__ volatile (
        "cpuid"
        : "=a"(out.eax), "=b"(out.ebx), "=c"(out.ecx), "=d"(out.edx)
        : "a"(leaf), "c"(subleaf)
    );
    return out;
}

cpu_features_t cpu_features_decode(
    uint32_t max_basic,
    uint32_t max_extended,
    cpu_cpuid_leaf_t leaf1,
    cpu_cpuid_leaf_t leaf7,
    cpu_cpuid_leaf_t ext1,
    cpu_cpuid_leaf_t ext7
) {
    cpu_features_t f = {0};
    f.max_basic_leaf = max_basic;
    f.max_extended_leaf = max_extended;
    f.initial_apic_id = leaf1.ebx >> 24;
    f.has_tsc = (leaf1.edx & CPUID_FEAT_EDX_TSC) != 0;
    f.has_msr = (leaf1.edx & CPUID_FEAT_EDX_MSR) != 0;
    f.has_pae = (leaf1.edx & CPUID_FEAT_EDX_PAE) != 0;
    f.has_apic = (leaf1.edx & CPUID_FEAT_EDX_APIC) != 0;
    f.has_x2apic = (leaf1.ecx & CPUID_FEAT_ECX_X2APIC) != 0;
    f.has_nx = max_extended >= UINT32_C(0x80000001) &&
               (ext1.edx & CPUID_EXT_EDX_NX) != 0;
    f.has_invariant_tsc = max_extended >= UINT32_C(0x80000007) &&
                          (ext7.edx & CPUID_EXT7_EDX_INVARIANT_TSC) != 0;
    f.has_smep = max_basic >= 7 && (leaf7.ebx & CPUID_7_EBX_SMEP) != 0;
    f.has_smap = max_basic >= 7 && (leaf7.ebx & CPUID_7_EBX_SMAP) != 0;
    return f;
}

cpu_features_t cpu_detect(void) {
    cpu_cpuid_leaf_t basic = cpuid_count(0, 0);
    cpu_cpuid_leaf_t extended = cpuid_count(UINT32_C(0x80000000), 0);
    cpu_cpuid_leaf_t leaf1 = {0};
    cpu_cpuid_leaf_t leaf7 = {0};
    cpu_cpuid_leaf_t ext1 = {0};
    cpu_cpuid_leaf_t ext7 = {0};

    if (basic.eax >= 1) leaf1 = cpuid_count(1, 0);
    if (basic.eax >= 7) leaf7 = cpuid_count(7, 0);
    if (extended.eax >= UINT32_C(0x80000001)) {
        ext1 = cpuid_count(UINT32_C(0x80000001), 0);
    }
    if (extended.eax >= UINT32_C(0x80000007)) {
        ext7 = cpuid_count(UINT32_C(0x80000007), 0);
    }

    return cpu_features_decode(
        basic.eax, extended.eax, leaf1, leaf7, ext1, ext7);
}

int cpu_required_features_present(const cpu_features_t *features) {
    return features &&
           features->max_basic_leaf >= 1 &&
           features->has_msr &&
           features->has_apic &&
           features->has_tsc &&
           features->has_pae &&
           features->has_nx;
}

uint64_t cpu_read_msr(uint32_t msr) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

void cpu_write_msr(uint32_t msr, uint64_t value) {
    __asm__ volatile (
        "wrmsr"
        :
        : "c"(msr), "a"((uint32_t)value), "d"((uint32_t)(value >> 32))
        : "memory"
    );
}

int cpu_enable_nx(const cpu_features_t *features) {
    if (!features || !features->has_msr || !features->has_nx) return 0;
    uint64_t efer = cpu_read_msr(MSR_EFER);
    cpu_write_msr(MSR_EFER, efer | EFER_NXE);
    return (cpu_read_msr(MSR_EFER) & EFER_NXE) != 0;
}

uint64_t cpu_read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

void cpu_write_cr3(uint64_t root_phys) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(root_phys) : "memory");
}

uint64_t cpu_read_tsc(void) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}
