#ifndef JOSHOS_SCHEDULER_H
#define JOSHOS_SCHEDULER_H

#include <stdint.h>

#define SCHEDULER_MAX_THREADS 16u

typedef void (*thread_entry_t)(void *argument);

typedef enum {
    THREAD_UNUSED = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_DEAD
} thread_state_t;

typedef struct {
    uint64_t id;
    thread_state_t state;
    uint64_t switches;
    uint64_t address_space_root;
    uint8_t user_mode;
} thread_info_t;

void scheduler_init(void);
int scheduler_create_kernel_thread(thread_entry_t entry, void *argument, uint64_t *thread_id_out);
int scheduler_create_user_thread(thread_entry_t entry,
                                 void *argument,
                                 uint64_t address_space_root,
                                 uint64_t *thread_id_out);
void scheduler_yield(void);
__attribute__((noreturn)) void scheduler_exit_current(void);
uint64_t scheduler_current_thread_id(void);
uint64_t scheduler_current_address_space(void);
uint32_t scheduler_live_thread_count(void);
int scheduler_thread_info(uint64_t thread_id, thread_info_t *info_out);
int scheduler_self_test(void);

#endif
