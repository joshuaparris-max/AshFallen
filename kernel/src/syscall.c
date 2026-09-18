#include "syscall.h"
#include "gdt.h"
#include "process.h"
#include "scheduler.h"
#include "serial.h"
#include "syscall_abi.h"
#include <stdint.h>

#define SYSCALL_WRITE_MAX 256u

static int ring3_seen;

static __attribute__((noreturn)) void syscall_panic(const char *message) {
    serial_write(message);
    serial_write("\n");
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

static uint64_t error_result(int error_number) {
    return (uint64_t)(int64_t)(-error_number);
}

void syscall_dispatch(syscall_frame_t *frame) {
    if (!frame) syscall_panic("JOSHOS_PANIC_NULL_SYSCALL_FRAME");
    if (frame->cs != GDT_USER_CODE_SELECTOR || frame->ss != GDT_USER_DATA_SELECTOR) {
        syscall_panic("JOSHOS_PANIC_BAD_SYSCALL_PRIVILEGE");
    }

    process_t *process = scheduler_current_process();
    if (!process) syscall_panic("JOSHOS_PANIC_SYSCALL_WITHOUT_PROCESS");

    if (!ring3_seen) {
        ring3_seen = 1;
        serial_write("JOSHOS_RING3_ENTERED\n");
    }

    switch (frame->rax) {
        case JOSH_SYS_ABI_VERSION:
            frame->rax = JOSH_SYSCALL_ABI_VERSION;
            serial_write("JOSHOS_SYSCALL_ABI_OK\n");
            return;

        case JOSH_SYS_WRITE: {
            uint64_t length = frame->rsi;
            if (length > SYSCALL_WRITE_MAX) {
                frame->rax = error_result(JOSH_ERR_EINVAL);
                return;
            }

            char buffer[SYSCALL_WRITE_MAX];
            if (!process_copy_from_user(process, frame->rdi, buffer, (size_t)length)) {
                frame->rax = error_result(JOSH_ERR_EFAULT);
                return;
            }

            for (uint64_t i = 0; i < length; ++i) serial_write_char(buffer[i]);
            frame->rax = length;
            return;
        }

        case JOSH_SYS_YIELD:
            scheduler_yield(frame);
            return;

        case JOSH_SYS_EXIT:
            scheduler_exit(frame, (int64_t)frame->rdi);
            return;

        default:
            frame->rax = error_result(JOSH_ERR_ENOSYS);
            return;
    }
}
