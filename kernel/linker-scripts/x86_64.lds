OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS
{
    limine_requests PT_LOAD FLAGS(0x06);
    text PT_LOAD FLAGS(0x05);
    rodata PT_LOAD FLAGS(0x04);
    data PT_LOAD FLAGS(0x06);
}

SECTIONS
{
    . = 0xffffffff80000000;
    __kernel_start = .;

    .limine_requests : {
        __limine_requests_start = .;
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
        __limine_requests_end = .;
    } :limine_requests

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __text_start = .;
    .text : { *(.text .text.*) } :text
    __text_end = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __rodata_start = .;
    .rodata : { *(.rodata .rodata.*) } :rodata
    __rodata_end = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __data_start = .;
    .data : { *(.data .data.*) } :data
    .bss : { *(.bss .bss.*) *(COMMON) } :data
    __data_end = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .paging_stack (NOLOAD) : {
        __paging_guard_low_start = .;
        . += 0x1000;
        __paging_stack_start = .;
        KEEP(*(.paging_stack))
        __paging_stack_end = .;
        . = ALIGN(CONSTANT(MAXPAGESIZE));
        __paging_guard_high_start = .;
        . += 0x1000;
        __paging_guard_high_end = .;
    } :data

    __kernel_end = .;

    /DISCARD/ : { *(.eh_frame*) *(.note .note.*) }
}
