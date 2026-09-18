#ifndef JOSHOS_SCHEDULER_H
#define JOSHOS_SCHEDULER_H

#include "boot.h"
#include "process.h"
#include "syscall.h"
#include <stdint.h>

int scheduler_run_userspace(const boot_context_t *boot);
process_t *scheduler_current_process(void);
void scheduler_yield(syscall_frame_t *frame);
void scheduler_exit(syscall_frame_t *frame, int64_t status);

#endif
