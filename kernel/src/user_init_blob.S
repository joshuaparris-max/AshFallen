.section .rodata.user_init_elf,"a",@progbits
.align 16

.global user_init_elf_start
.global user_init_elf_end
.type user_init_elf_start, @object

user_init_elf_start:
    .incbin "user/bin/init.elf"
user_init_elf_end:

.size user_init_elf_start, user_init_elf_end-user_init_elf_start

.section .note.GNU-stack,"",@progbits
