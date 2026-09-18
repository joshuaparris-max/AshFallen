.section .bss
.align 16
paging_transition_stack:
    .skip 32768
paging_transition_stack_top:

.section .text
.global paging_activate
.type paging_activate, @function
paging_activate:
    cli
    movq %rdi, %cr3
    leaq paging_transition_stack_top(%rip), %rsp
    xorq %rbp, %rbp
    jmp *%rsi
.size paging_activate, .-paging_activate
