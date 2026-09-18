.section .text

.extern exception_dispatch

.macro EXCEPTION_NO_ERROR vector
    .global exception_stub_\vector
    .type exception_stub_\vector, @function
exception_stub_\vector:
    pushq $0
    pushq $\vector
    jmp exception_common_entry
    .size exception_stub_\vector, .-exception_stub_\vector
.endm

.macro EXCEPTION_WITH_ERROR vector
    .global exception_stub_\vector
    .type exception_stub_\vector, @function
exception_stub_\vector:
    pushq $\vector
    jmp exception_common_entry
    .size exception_stub_\vector, .-exception_stub_\vector
.endm

/*
 * The CPU has already saved RIP, CS and RFLAGS, plus an error code for the
 * exceptions that define one. The stubs above normalise the stack so that
 * every exception has:
 *
 *   vector, error, RIP, CS, RFLAGS, [old RSP, old SS]
 *
 * We then push every general-purpose register in reverse struct order so
 * %rsp points directly at exception_frame_t.
 */
.type exception_common_entry, @function
exception_common_entry:
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

    movq %rsp, %rdi
    andq $-16, %rsp
    call exception_dispatch

    /* exception_dispatch is noreturn; make a bad return fail loudly. */
    ud2
.size exception_common_entry, .-exception_common_entry

EXCEPTION_NO_ERROR 0
EXCEPTION_NO_ERROR 1
EXCEPTION_NO_ERROR 2
EXCEPTION_NO_ERROR 3
EXCEPTION_NO_ERROR 4
EXCEPTION_NO_ERROR 5
EXCEPTION_NO_ERROR 6
EXCEPTION_NO_ERROR 7
EXCEPTION_WITH_ERROR 8
EXCEPTION_NO_ERROR 9
EXCEPTION_WITH_ERROR 10
EXCEPTION_WITH_ERROR 11
EXCEPTION_WITH_ERROR 12
EXCEPTION_WITH_ERROR 13
EXCEPTION_WITH_ERROR 14
EXCEPTION_NO_ERROR 15
EXCEPTION_NO_ERROR 16
EXCEPTION_WITH_ERROR 17
EXCEPTION_NO_ERROR 18
EXCEPTION_NO_ERROR 19
EXCEPTION_NO_ERROR 20
EXCEPTION_WITH_ERROR 21
EXCEPTION_NO_ERROR 22
EXCEPTION_NO_ERROR 23
EXCEPTION_NO_ERROR 24
EXCEPTION_NO_ERROR 25
EXCEPTION_NO_ERROR 26
EXCEPTION_NO_ERROR 27
EXCEPTION_NO_ERROR 28
EXCEPTION_WITH_ERROR 29
EXCEPTION_WITH_ERROR 30
EXCEPTION_NO_ERROR 31

.section .rodata
.align 8
.global exception_stub_table
.type exception_stub_table, @object
exception_stub_table:
    .quad exception_stub_0
    .quad exception_stub_1
    .quad exception_stub_2
    .quad exception_stub_3
    .quad exception_stub_4
    .quad exception_stub_5
    .quad exception_stub_6
    .quad exception_stub_7
    .quad exception_stub_8
    .quad exception_stub_9
    .quad exception_stub_10
    .quad exception_stub_11
    .quad exception_stub_12
    .quad exception_stub_13
    .quad exception_stub_14
    .quad exception_stub_15
    .quad exception_stub_16
    .quad exception_stub_17
    .quad exception_stub_18
    .quad exception_stub_19
    .quad exception_stub_20
    .quad exception_stub_21
    .quad exception_stub_22
    .quad exception_stub_23
    .quad exception_stub_24
    .quad exception_stub_25
    .quad exception_stub_26
    .quad exception_stub_27
    .quad exception_stub_28
    .quad exception_stub_29
    .quad exception_stub_30
    .quad exception_stub_31
.size exception_stub_table, .-exception_stub_table

.section .note.GNU-stack,"",@progbits
