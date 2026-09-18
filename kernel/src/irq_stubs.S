.section .text

.extern interrupt_dispatch

.macro IRQ_STUB name vector
    .global \name
    .type \name, @function
\name:
    cld
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    /*
     * Preserve the exact interrupt-frame stack pointer in a callee-saved
     * register, then satisfy the SysV call alignment requirement.
     */
    movq %rsp, %r12
    andq $-16, %rsp
    movq $\vector, %rdi
    call interrupt_dispatch
    movq %r12, %rsp

    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    iretq
    .size \name, .-\name
.endm

IRQ_STUB irq_stub_32 32
IRQ_STUB irq_stub_33 33
IRQ_STUB irq_stub_254 254
IRQ_STUB irq_stub_255 255

.section .note.GNU-stack,"",@progbits
