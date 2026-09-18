#ifndef JOSHOS_GDT_H
#define JOSHOS_GDT_H

#define GDT_KERNEL_CODE_SELECTOR 0x08u
#define GDT_KERNEL_DATA_SELECTOR 0x10u
#define GDT_TSS_SELECTOR 0x18u
#define GDT_DOUBLE_FAULT_IST_INDEX 1u

int gdt_init(void);

#endif
