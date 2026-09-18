OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(0x05);
}

SECTIONS
{
    . = 0x0000000000400000;

    .text : ALIGN(0x1000) {
        *(.text .text.*)
        *(.rodata .rodata.*)
    } :text

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
        *(.comment)
    }
}
