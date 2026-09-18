.section .rodata.user_probe,"a",@progbits
.global user_probe_start
.global user_probe_end

user_probe_start:
    movq $1, %rax
    leaq user_probe_message(%rip), %rdi
    movq $(user_probe_message_end-user_probe_message), %rsi
    int $0x80

    cmpq $(user_probe_message_end-user_probe_message), %rax
    jne 1f

    movq $3, %rax
    int $0x80
    testq %rax, %rax
    jz 1f

    movq $2, %rax
    xorq %rdi, %rdi
    int $0x80
    ud2

1:
    movq $2, %rax
    movq $1, %rdi
    int $0x80
    ud2

user_probe_message:
    .ascii "JOSHOS_USERSPACE_HELLO\n"
user_probe_message_end:
user_probe_end:

.section .note.GNU-stack,"",@progbits
