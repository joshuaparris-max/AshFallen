#ifndef JOSHOS_PROCESS_H
#define JOSHOS_PROCESS_H

#include "paging.h"
#include <stddef.h>
#include <stdint.h>

#define PROCESS_MAX_PROCESSES 8u
#define PROCESS_MAX_OWNED_FRAMES 256u

typedef enum {
    PROCESS_UNUSED = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_EXITED
} process_state_t;

typedef enum {
    PROCESS_OK = 0,
    PROCESS_BAD_ARGUMENT,
    PROCESS_TABLE_FULL,
    PROCESS_BAD_ELF,
    PROCESS_IMAGE_TOO_LARGE,
    PROCESS_NO_MEMORY,
    PROCESS_PAGING_ERROR,
    PROCESS_SCHEDULER_ERROR,
    PROCESS_NOT_FOUND,
    PROCESS_NOT_EXITED
} process_status_t;

typedef struct {
    uint64_t pid;
    process_state_t state;
    uint64_t exit_status;
    uint64_t entry;
    uint64_t user_stack_top;
    uint16_t owned_frame_count;
} process_info_t;

void process_init(void);
process_status_t process_spawn_elf(const void *image,
                                   size_t image_size,
                                   uint64_t *pid_out);
int process_mark_exit_current(uint64_t status);
process_status_t process_get_info(uint64_t pid, process_info_t *info_out);
process_status_t process_reap(uint64_t pid);
uint32_t process_live_count(void);
const char *process_status_string(process_status_t status);

#endif
