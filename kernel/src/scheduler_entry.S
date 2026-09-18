.section .text

.global scheduler_enter_user
.type scheduler_enter_user, @function
scheduler_enter_user:
    movq %rsp, scheduler_saved_kernel_rsp(%rip)
    movq %cr3, %rax
    movq %rax, scheduler_saved_kernel_cr3(%rip)
    movq %rbx, scheduler_saved_rbx(%rip)
    movq %rbp, scheduler_saved_rbp(%rip)
    movq %r12, scheduler_saved_r12(%rip)
    movq %r13, scheduler_saved_r13(%rip)
    movq %r14, scheduler_saved_r14(%rip)
    movq %r15, scheduler_saved_r15(%rip)

    leaq scheduler_bootstrap_stack_top(%rip), %rsp

    /* Build the initial IRETQ frame from syscall_frame_t. */
    pushq 152(%rdi)
    pushq 144(%rdi)
    pushq 136(%rdi)
    pushq 128(%rdi)
    pushq 120(%rdi)

    movq %rsi, %cr3

    /*
     * Keep the context pointer in RDX and load RDX last. Offsets are checked
     * with C static assertions in scheduler.c.
     */
    movq %rdi, %rdx
    movq 0(%rdx), %rax
    movq 8(%rdx), %rbx
    movq 16(%rdx), %rcx
    movq 32(%rdx), %rsi
    movq 40(%rdx), %rdi
    movq 48(%rdx), %rbp
    movq 56(%rdx), %r8
    movq 64(%rdx), %r9
    movq 72(%rdx), %r10
    movq 80(%rdx), %r11
    movq 88(%rdx), %r12
    movq 96(%rdx), %r13
    movq 104(%rdx), %r14
    movq 112(%rdx), %r15
    movq 24(%rdx), %rdx
    iretq
.size scheduler_enter_user, .-scheduler_enter_user

.global scheduler_return_to_kernel
.type scheduler_return_to_kernel, @function
scheduler_return_to_kernel:
    movq scheduler_saved_kernel_cr3(%rip), %rax
    movq %rax, %cr3
    movq scheduler_saved_rbx(%rip), %rbx
    movq scheduler_saved_rbp(%rip), %rbp
    movq scheduler_saved_r12(%rip), %r12
    movq scheduler_saved_r13(%rip), %r13
    movq scheduler_saved_r14(%rip), %r14
    movq scheduler_saved_r15(%rip), %r15
    movq scheduler_saved_kernel_rsp(%rip), %rsp
    ret
.size scheduler_return_to_kernel, .-scheduler_return_to_kernel

.section .bss
.align 8
scheduler_saved_kernel_rsp:
    .quad 0
scheduler_saved_kernel_cr3:
    .quad 0
scheduler_saved_rbx:
    .quad 0
scheduler_saved_rbp:
    .quad 0
scheduler_saved_r12:
    .quad 0
scheduler_saved_r13:
    .quad 0
scheduler_saved_r14:
    .quad 0
scheduler_saved_r15:
    .quad 0

.align 16
scheduler_bootstrap_stack:
    .skip 16384
scheduler_bootstrap_stack_top:

.section .note.GNU-stack,"",@progbits
