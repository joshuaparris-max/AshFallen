#include "../src/syscall_abi.h"

.section .text
.global _start
.type _start, @function
_start:
    movq %rdi, %r12

    movq $JOSH_SYS_ABI_VERSION, %rax
    int $0x80
    cmpq $JOSH_SYSCALL_ABI_VERSION, %rax
    jne .Lfail

    /* Prove the kernel rejects an unmapped userspace pointer. */
    movq $JOSH_SYS_WRITE, %rax
    movabsq $0x0000000000500000, %rdi
    movq $4, %rsi
    int $0x80
    cmpq $-JOSH_ERR_EFAULT, %rax
    jne .Lfail

    leaq msg_efault(%rip), %rdi
    movq $msg_efault_len, %rsi
    movq $JOSH_SYS_WRITE, %rax
    int $0x80
    cmpq $msg_efault_len, %rax
    jne .Lfail

    cmpq $1, %r12
    jne .Lfirst_two
    leaq msg_init_one(%rip), %rdi
    movq $msg_init_one_len, %rsi
    jmp .Lwrite_first
.Lfirst_two:
    leaq msg_init_two(%rip), %rdi
    movq $msg_init_two_len, %rsi
.Lwrite_first:
    movq $JOSH_SYS_WRITE, %rax
    int $0x80

    movq $JOSH_SYS_YIELD, %rax
    int $0x80
    testq %rax, %rax
    jne .Lfail

    cmpq $1, %r12
    jne .Lresume_two
    leaq msg_resume_one(%rip), %rdi
    movq $msg_resume_one_len, %rsi
    jmp .Lwrite_resume
.Lresume_two:
    leaq msg_resume_two(%rip), %rdi
    movq $msg_resume_two_len, %rsi
.Lwrite_resume:
    movq $JOSH_SYS_WRITE, %rax
    int $0x80

    movq $JOSH_SYS_EXIT, %rax
    movq %r12, %rdi
    int $0x80

.Lfail:
    ud2
.size _start, .-_start

.section .rodata
msg_efault:
    .ascii "JOSHOS_USER_EFAULT_OK\n"
.set msg_efault_len, .-msg_efault

msg_init_one:
    .ascii "JOSHOS_USER_INIT_1\n"
.set msg_init_one_len, .-msg_init_one

msg_init_two:
    .ascii "JOSHOS_USER_INIT_2\n"
.set msg_init_two_len, .-msg_init_two

msg_resume_one:
    .ascii "JOSHOS_USER_RESUMED_1\n"
.set msg_resume_one_len, .-msg_resume_one

msg_resume_two:
    .ascii "JOSHOS_USER_RESUMED_2\n"
.set msg_resume_two_len, .-msg_resume_two

.section .note.GNU-stack,"",@progbits
