#include "scheduler.h"
#include "serial.h"
#include <stddef.h>
#include <stdint.h>

#define KERNEL_THREAD_STACK_SIZE 16384u
#define SCHEDULER_BOOTSTRAP_ID UINT64_C(1)

typedef struct thread {
    uint64_t saved_rsp;
    uint64_t id;
    thread_state_t state;
    uint64_t switches;
    thread_entry_t entry;
    void *argument;
    uint8_t stack[KERNEL_THREAD_STACK_SIZE] __attribute__((aligned(16)));
} thread_t;

extern void context_switch(uint64_t *old_rsp, uint64_t new_rsp);

static thread_t threads[SCHEDULER_MAX_THREADS];
static thread_t bootstrap_thread;
static thread_t *current;
static uint64_t next_id = SCHEDULER_BOOTSTRAP_ID + 1u;
static uint32_t scan_cursor;

static __attribute__((noreturn)) void thread_trampoline(void) {
    thread_t *thread = current;
    if (!thread || !thread->entry) {
        serial_write("JOSHOS_ERROR_THREAD_TRAMPOLINE\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    thread->entry(thread->argument);
    thread->state = THREAD_DEAD;

    for (;;) {
        scheduler_yield();
        __asm__ volatile ("hlt");
    }
}

static uint64_t prepare_initial_stack(thread_t *thread) {
    uintptr_t top = (uintptr_t)(thread->stack + sizeof(thread->stack));
    top &= ~(uintptr_t)0x0fu;
    top -= sizeof(uint64_t); /* SysV function-entry RSP is 8 mod 16. */

    uint64_t *stack = (uint64_t *)top;

    /*
     * context_switch restores r15,r14,r13,r12,rbp,rbx and then returns.
     * Build exactly that synthetic callee-saved frame, followed by the
     * trampoline return address.
     */
    *--stack = (uint64_t)(uintptr_t)thread_trampoline; /* ret */
    *--stack = 0; /* rbx */
    *--stack = 0; /* rbp */
    *--stack = 0; /* r12 */
    *--stack = 0; /* r13 */
    *--stack = 0; /* r14 */
    *--stack = 0; /* r15 */
    return (uint64_t)(uintptr_t)stack;
}

static thread_t *find_thread_by_id(uint64_t id) {
    if (id == SCHEDULER_BOOTSTRAP_ID) return &bootstrap_thread;
    for (uint32_t i = 0; i < SCHEDULER_MAX_THREADS; ++i) {
        if (threads[i].state != THREAD_UNUSED && threads[i].id == id) {
            return &threads[i];
        }
    }
    return 0;
}

static thread_t *thread_for_slot(uint32_t slot) {
    if (slot == 0) return &bootstrap_thread;
    if (slot <= SCHEDULER_MAX_THREADS) return &threads[slot - 1u];
    return 0;
}

static thread_t *next_runnable(void) {
    const uint32_t slot_count = SCHEDULER_MAX_THREADS + 1u;

    for (uint32_t offset = 0; offset < slot_count; ++offset) {
        uint32_t slot = (scan_cursor + offset) % slot_count;
        thread_t *candidate = thread_for_slot(slot);
        if (candidate && candidate->state == THREAD_READY) {
            scan_cursor = (slot + 1u) % slot_count;
            return candidate;
        }
    }

    return 0;
}

void scheduler_init(void) {
    for (uint32_t i = 0; i < SCHEDULER_MAX_THREADS; ++i) {
        threads[i].state = THREAD_UNUSED;
        threads[i].id = 0;
        threads[i].saved_rsp = 0;
        threads[i].switches = 0;
        threads[i].entry = 0;
        threads[i].argument = 0;
    }

    bootstrap_thread.saved_rsp = 0;
    bootstrap_thread.id = SCHEDULER_BOOTSTRAP_ID;
    bootstrap_thread.state = THREAD_RUNNING;
    bootstrap_thread.switches = 0;
    bootstrap_thread.entry = 0;
    bootstrap_thread.argument = 0;

    current = &bootstrap_thread;
    next_id = SCHEDULER_BOOTSTRAP_ID + 1u;
    scan_cursor = 1; /* slot 0 is the currently running bootstrap thread */
}

int scheduler_create_kernel_thread(thread_entry_t entry, void *argument, uint64_t *thread_id_out) {
    if (!entry) return 0;

    for (uint32_t i = 0; i < SCHEDULER_MAX_THREADS; ++i) {
        if (threads[i].state != THREAD_UNUSED && threads[i].state != THREAD_DEAD) continue;

        thread_t *thread = &threads[i];
        thread->id = next_id++;
        thread->state = THREAD_READY;
        thread->switches = 0;
        thread->entry = entry;
        thread->argument = argument;
        thread->saved_rsp = prepare_initial_stack(thread);

        if (thread_id_out) *thread_id_out = thread->id;
        return 1;
    }

    return 0;
}

void scheduler_yield(void) {
    if (!current) return;

    thread_t *previous = current;
    if (previous->state == THREAD_RUNNING) previous->state = THREAD_READY;

    thread_t *next = next_runnable();
    if (!next) {
        previous->state = THREAD_RUNNING;
        return;
    }

    if (next == previous) {
        previous->state = THREAD_RUNNING;
        return;
    }

    next->state = THREAD_RUNNING;
    next->switches++;
    current = next;
    context_switch(&previous->saved_rsp, next->saved_rsp);
}

uint64_t scheduler_current_thread_id(void) {
    return current ? current->id : 0;
}

uint32_t scheduler_live_thread_count(void) {
    uint32_t count = bootstrap_thread.state != THREAD_DEAD ? 1u : 0u;
    for (uint32_t i = 0; i < SCHEDULER_MAX_THREADS; ++i) {
        if (threads[i].state == THREAD_READY || threads[i].state == THREAD_RUNNING) {
            count++;
        }
    }
    return count;
}

int scheduler_thread_info(uint64_t thread_id, thread_info_t *info_out) {
    if (!info_out) return 0;
    thread_t *thread = find_thread_by_id(thread_id);
    if (!thread) return 0;

    info_out->id = thread->id;
    info_out->state = thread->state;
    info_out->switches = thread->switches;
    return 1;
}

typedef struct {
    uint32_t marker;
    uint32_t iterations;
    uint32_t observed[8];
} scheduler_test_worker_t;

static void scheduler_test_worker(void *argument) {
    scheduler_test_worker_t *worker = (scheduler_test_worker_t *)argument;
    for (uint32_t i = 0; i < worker->iterations; ++i) {
        worker->observed[i] = worker->marker + i;
        scheduler_yield();
    }
}

int scheduler_self_test(void) {
    scheduler_test_worker_t a = { .marker = 0x100u, .iterations = 4u, .observed = {0} };
    scheduler_test_worker_t b = { .marker = 0x200u, .iterations = 4u, .observed = {0} };
    uint64_t a_id = 0;
    uint64_t b_id = 0;

    if (!scheduler_create_kernel_thread(scheduler_test_worker, &a, &a_id)) return 0;
    if (!scheduler_create_kernel_thread(scheduler_test_worker, &b, &b_id)) return 0;

    /*
     * One bootstrap yield must eventually return even though both workers are
     * still runnable. This proves bootstrap participates in round-robin rather
     * than waiting for all workers to die.
     */
    scheduler_yield();
    thread_info_t fairness_a;
    thread_info_t fairness_b;
    if (!scheduler_thread_info(a_id, &fairness_a) ||
        !scheduler_thread_info(b_id, &fairness_b)) return 0;
    if (fairness_a.state == THREAD_DEAD && fairness_b.state == THREAD_DEAD) return 0;
    if (scheduler_current_thread_id() != SCHEDULER_BOOTSTRAP_ID) return 0;

    for (uint32_t guard = 0; guard < 32u; ++guard) {
        thread_info_t a_info;
        thread_info_t b_info;
        if (!scheduler_thread_info(a_id, &a_info) || !scheduler_thread_info(b_id, &b_info)) return 0;

        if (a_info.state == THREAD_DEAD && b_info.state == THREAD_DEAD) break;
        scheduler_yield();
    }

    thread_info_t a_info;
    thread_info_t b_info;
    if (!scheduler_thread_info(a_id, &a_info) || !scheduler_thread_info(b_id, &b_info)) return 0;
    if (a_info.state != THREAD_DEAD || b_info.state != THREAD_DEAD) return 0;
    if (a_info.switches < a.iterations || b_info.switches < b.iterations) return 0;
    if (scheduler_current_thread_id() != SCHEDULER_BOOTSTRAP_ID) return 0;

    for (uint32_t i = 0; i < 4u; ++i) {
        if (a.observed[i] != 0x100u + i) return 0;
        if (b.observed[i] != 0x200u + i) return 0;
    }

    return 1;
}
