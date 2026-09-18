#include "gdt.h"
#include <stdint.h>

#define DOUBLE_FAULT_STACK_SIZE 16384u

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss64_t;

_Static_assert(sizeof(gdtr_t) == 10, "x86-64 GDTR layout changed");
_Static_assert(sizeof(tss64_t) == 104, "x86-64 TSS layout changed");
_Static_assert(GDT_KERNEL_CODE_SELECTOR == 0x08u, "assembly selector must match GDT");
_Static_assert(GDT_KERNEL_DATA_SELECTOR == 0x10u, "assembly selector must match GDT");
_Static_assert(GDT_TSS_SELECTOR == 0x18u, "assembly selector must match GDT");
_Static_assert(GDT_USER_DATA_SELECTOR == 0x2Bu, "Ring 3 data selector must match GDT");
_Static_assert(GDT_USER_CODE_SELECTOR == 0x33u, "Ring 3 code selector must match GDT");

static uint64_t gdt[7] __attribute__((aligned(16)));
static tss64_t tss __attribute__((aligned(16)));
static uint8_t double_fault_stack[DOUBLE_FAULT_STACK_SIZE] __attribute__((aligned(16)));

static void build_tss_descriptor(uint64_t base, uint32_t limit) {
    uint64_t descriptor = 0;
    descriptor |= (uint64_t)(limit & 0xFFFFu);
    descriptor |= (base & 0xFFFFu) << 16;
    descriptor |= ((base >> 16) & 0xFFu) << 32;
    descriptor |= UINT64_C(0x89) << 40;
    descriptor |= ((uint64_t)(limit >> 16) & 0x0Fu) << 48;
    descriptor |= ((base >> 24) & 0xFFu) << 56;
    gdt[3] = descriptor;
    gdt[4] = base >> 32;
}

int gdt_init(void) {
    /*
     * Long mode ignores base/limit for normal code and data segments, but the
     * descriptors still need valid types. The TSS descriptor remains fully
     * significant and points at Josh-owned storage.
     */
    gdt[0] = 0;
    gdt[1] = UINT64_C(0x00209A0000000000);
    gdt[2] = UINT64_C(0x0000920000000000);
    gdt[5] = UINT64_C(0x0000F20000000000);
    gdt[6] = UINT64_C(0x0020FA0000000000);

    tss.ist1 = (uint64_t)(uintptr_t)(double_fault_stack + sizeof(double_fault_stack));
    tss.iomap_base = (uint16_t)sizeof(tss);
    build_tss_descriptor((uint64_t)(uintptr_t)&tss, (uint32_t)(sizeof(tss) - 1));

    gdtr_t gdtr = {
        .limit = (uint16_t)(sizeof(gdt) - 1),
        .base = (uint64_t)(uintptr_t)gdt
    };

    __asm__ volatile (
        "cli\n\t"
        "lgdt %0\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "xorw %%ax, %%ax\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw $0x18, %%ax\n\t"
        "ltr %%ax\n\t"
        :
        : "m"(gdtr)
        : "rax", "memory"
    );

    uint16_t cs;
    uint16_t tr;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("str %0" : "=r"(tr));

    return cs == GDT_KERNEL_CODE_SELECTOR && tr == GDT_TSS_SELECTOR;
}


void gdt_set_kernel_stack(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
