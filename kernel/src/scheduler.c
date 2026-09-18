#include "scheduler.h"
#include "gdt.h"
#include "serial.h"
#include <stdint.h>

static process_t processes[PROCESS_MAX];
static int current_index = -1;

extern void scheduler_enter_user(syscall_frame_t *frame, uint64_t cr3_phys);
__attribute__((noreturn)) extern void scheduler_return_to_kernel(void);

_Static_assert(__builtin_offsetof(syscall_frame_t, rax) == 0, "syscall frame rax offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, rdx) == 24, "syscall frame rdx offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, r15) == 112, "syscall frame r15 offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, rip) == 120, "syscall frame rip offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, cs) == 128, "syscall frame cs offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, rflags) == 136, "syscall frame rflags offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, rsp) == 144, "syscall frame rsp offset");
_Static_assert(__builtin_offsetof(syscall_frame_t, ss) == 152, "syscall frame ss offset");

static void frame_copy(syscall_frame_t *destination, const syscall_frame_t *source) {
    destination->rax = source->rax;
    destination->rbx = source->rbx;
    destination->rcx = source->rcx;
    destination->rdx = source->rdx;
    destination->rsi = source->rsi;
    destination->rdi = source->rdi;
    destination->rbp = source->rbp;
    destination->r8 = source->r8;
    destination->r9 = source->r9;
    destination->r10 = source->r10;
    destination->r11 = source->r11;
    destination->r12 = source->r12;
    destination->r13 = source->r13;
    destination->r14 = source->r14;
    destination->r15 = source->r15;
    destination->rip = source->rip;
    destination->cs = source->cs;
    destination->rflags = source->rflags;
    destination->rsp = source->rsp;
    destination->ss = source->ss;
}

static void activate_process(int next_index, syscall_frame_t *live_frame) {
    process_t *next = &processes[next_index];
    next->state = PROCESS_RUNNING;
    current_index = next_index;
    gdt_set_rsp0(next->kernel_stack_top);
    frame_copy(live_frame, &next->context);
    __asm__ volatile ("mov %0, %%cr3" :: "r"(next->cr3_phys) : "memory");
}

static int find_next_runnable(int after_index) {
    for (uint32_t offset = 1; offset <= PROCESS_MAX; ++offset) {
        int candidate = (after_index + (int)offset) % (int)PROCESS_MAX;
        if (processes[candidate].state == PROCESS_RUNNABLE) return candidate;
    }
    return -1;
}

process_t *scheduler_current_process(void) {
    if (current_index < 0 || current_index >= (int)PROCESS_MAX) return 0;
    if (processes[current_index].state != PROCESS_RUNNING) return 0;
    return &processes[current_index];
}

void scheduler_yield(syscall_frame_t *frame) {
    process_t *current = scheduler_current_process();
    if (!current || !frame) return;

    frame->rax = 0;
    frame_copy(&current->context, frame);
    current->state = PROCESS_RUNNABLE;

    int next = find_next_runnable(current_index);
    if (next < 0) {
        current->state = PROCESS_RUNNING;
        return;
    }

    serial_write("JOSHOS_SCHED_SWITCH\n");
    activate_process(next, frame);
}

void scheduler_exit(syscall_frame_t *frame, int64_t status) {
    process_t *current = scheduler_current_process();
    if (!current || !frame) return;

    serial_write("JOSHOS_PROCESS_EXIT PID=");
    serial_write_hex64(current->pid);
    serial_write(" STATUS=");
    serial_write_hex64((uint64_t)status);
    serial_write("\n");

    current->state = PROCESS_EXITED;
    int old_index = current_index;
    int next = find_next_runnable(old_index);
    if (next >= 0) {
        serial_write("JOSHOS_SCHED_SWITCH\n");
        activate_process(next, frame);
        return;
    }

    current_index = -1;
    serial_write("JOSHOS_USERSPACE_OK\n");
    scheduler_return_to_kernel();
}

int scheduler_run_userspace(const boot_context_t *boot) {
    if (!boot) return 0;

    for (uint32_t i = 0; i < PROCESS_MAX; ++i) {
        if (!process_create_init(&processes[i], i + 1u, i, boot)) {
            serial_write("JOSHOS_ERROR_PROCESS_CREATE\n");
            return 0;
        }
    }

    serial_write("JOSHOS_PROCESSES_READY\n");
    current_index = 0;
    processes[0].state = PROCESS_RUNNING;
    gdt_set_rsp0(processes[0].kernel_stack_top);
    serial_write("JOSHOS_SCHEDULER_START\n");

    scheduler_enter_user(&processes[0].context, processes[0].cr3_phys);

    if (processes[0].state != PROCESS_EXITED || processes[1].state != PROCESS_EXITED) {
        serial_write("JOSHOS_ERROR_USERSPACE_RETURN_STATE\n");
        return 0;
    }
    return 1;
}
