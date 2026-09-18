.section .text

.extern syscall_dispatch

.global syscall_entry
.type syscall_entry, @function
syscall_entry:
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
     * Preserve the exact frame pointer in a callee-saved register while C
     * runs. scheduler_yield may replace the frame contents with another
     * process, but every process maps kernel storage at the same high-half VA.
     */
    movq %rsp, %r12
    movq %rsp, %rdi
    andq $-16, %rsp
    call syscall_dispatch
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
.size syscall_entry, .-syscall_entry

.section .note.GNU-stack,"",@progbits
