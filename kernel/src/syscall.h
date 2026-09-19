#ifndef JOSHOS_SYSCALL_H
#define JOSHOS_SYSCALL_H

#include <stdint.h>

#define JOSH_SYS_WRITE UINT64_C(1)
#define JOSH_SYS_EXIT  UINT64_C(2)
#define JOSH_SYS_GETPID UINT64_C(3)

typedef struct {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} syscall_frame_t;

_Static_assert(__builtin_offsetof(syscall_frame_t, rax) == 0, "syscall rax offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, r15) == 112, "syscall r15 offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, rip) == 120, "syscall RIP offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, cs) == 128, "syscall CS offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, rsp) == 144, "syscall RSP offset");

void syscall_dispatch(syscall_frame_t *frame);

#endif
