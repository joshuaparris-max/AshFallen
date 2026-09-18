.section .text

.global context_switch
.type context_switch, @function
context_switch:
    /*
     * SysV AMD64 callee-saved context:
     *   rbx, rbp, r12-r15, rsp and the return RIP already on the stack.
     */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)
    movq %rsi, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.size context_switch, .-context_switch

.section .note.GNU-stack,"",@progbits
