#include "syscall.h"
#include "cpu.h"
#include "paging.h"
#include "scheduler.h"
#include "serial.h"
#include <stdint.h>

#define MAX_SERIAL_WRITE UINT64_C(512)
#define CR3_ADDRESS_MASK UINT64_C(0x000ffffffffff000)

static __attribute__((noreturn)) void syscall_exit(uint64_t status) {
    serial_write("JOSHOS_USERSPACE_EXIT_STATUS=");
    serial_write_hex64(status);
    serial_write("\nJOSHOS_USERSPACE_EXIT_OK\n");
    __asm__ volatile ("cli" ::: "memory");
    for (;;) __asm__ volatile ("hlt");
}

void syscall_dispatch(syscall_frame_t *frame) {
    if (!frame || (frame->cs & 3u) != 3u || (frame->ss & 3u) != 3u) {
        serial_write("JOSHOS_ERROR_SYSCALL_NOT_RING3\n");
        __asm__ volatile ("cli" ::: "memory");
        for (;;) __asm__ volatile ("hlt");
    }

    static int ring3_proved;
    if (!ring3_proved) {
        serial_write("JOSHOS_RING3_SYSCALL_OK\n");
        ring3_proved = 1;
    }

    switch (frame->rax) {
        case JOSH_SYS_WRITE: {
            uint64_t root = cpu_read_cr3() & CR3_ADDRESS_MASK;
            if (frame->rsi == 0 || frame->rsi > MAX_SERIAL_WRITE ||
                !paging_user_range_accessible(root, frame->rdi, frame->rsi, 0)) {
                frame->rax = UINT64_MAX;
                return;
            }
            serial_write_n((const char *)(uintptr_t)frame->rdi, frame->rsi);
            frame->rax = frame->rsi;
            return;
        }

        case JOSH_SYS_EXIT:
            syscall_exit(frame->rdi);

        case JOSH_SYS_GETPID:
            frame->rax = scheduler_current_thread_id();
            return;

        default:
            frame->rax = UINT64_MAX;
            return;
    }
}
