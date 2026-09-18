#ifndef JOSHOS_PROCESS_H
#define JOSHOS_PROCESS_H

#include "boot.h"
#include "syscall.h"
#include <stddef.h>
#include <stdint.h>

#define PROCESS_MAX 2u
#define PROCESS_USER_BASE UINT64_C(0x0000000000400000)
#define PROCESS_USER_REGION_PAGES 32u
#define PROCESS_USER_STACK_TOP (PROCESS_USER_BASE + PROCESS_USER_REGION_PAGES * UINT64_C(4096))

typedef enum {
    PROCESS_UNUSED = 0,
    PROCESS_RUNNABLE,
    PROCESS_RUNNING,
    PROCESS_EXITED
} process_state_t;

typedef struct {
    uint32_t pid;
    process_state_t state;
    uint64_t cr3_phys;
    uint64_t kernel_stack_top;
    syscall_frame_t context;
    uint8_t mapped_pages[PROCESS_USER_REGION_PAGES];
    uint8_t writable_pages[PROCESS_USER_REGION_PAGES];
    uint8_t executable_pages[PROCESS_USER_REGION_PAGES];
    void *storage;
} process_t;

int process_create_init(process_t *process,
                        uint32_t pid,
                        uint32_t storage_slot,
                        const boot_context_t *boot);
int process_copy_from_user(const process_t *process,
                           uint64_t user_address,
                           void *destination,
                           size_t length);

#endif
