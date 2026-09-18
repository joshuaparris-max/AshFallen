.section .text

.equ USER_DATA_SELECTOR, 0x2b
.equ USER_CODE_SELECTOR, 0x33

.global user_enter
.type user_enter, @function
user_enter:
    cli
    movq %rdi, %cr3

    movw $USER_DATA_SELECTOR, %ax
    movw %ax, %ds
    movw %ax, %es

    pushq $USER_DATA_SELECTOR
    pushq %rdx
    pushq $0x2
    pushq $USER_CODE_SELECTOR
    pushq %rsi
    iretq
.size user_enter, .-user_enter

.section .note.GNU-stack,"",@progbits
