#include "process.h"
#include "paging.h"
#include "pmm.h"
#include "scheduler.h"
#include "serial.h"
#include "user_elf.h"
#include <stddef.h>
#include <stdint.h>

#define PROCESS_USER_STACK_PAGES 4u
#define PROCESS_USER_STACK_TOP UINT64_C(0x00007fffffffe000)
#define PROCESS_USER_STACK_BASE \
    (PROCESS_USER_STACK_TOP - PROCESS_USER_STACK_PAGES * PMM_PAGE_SIZE)

typedef struct {
    uint64_t pid;
    process_state_t state;
    uint64_t exit_status;
    paging_address_space_t address_space;
    uint64_t entry;
    uint64_t user_stack_top;
    uint64_t owned_frames[PROCESS_MAX_OWNED_FRAMES];
    uint16_t owned_frame_count;
} process_t;

extern __attribute__((noreturn))
void user_enter(uint64_t root_phys, uint64_t entry, uint64_t stack_top);

static process_t processes[PROCESS_MAX_PROCESSES];

static uint64_t align_down(uint64_t value) {
    return value & ~(PMM_PAGE_SIZE - 1u);
}

static int align_up(uint64_t value, uint64_t *out) {
    if (!out || value > UINT64_MAX - (PMM_PAGE_SIZE - 1u)) return 0;
    *out = (value + PMM_PAGE_SIZE - 1u) & ~(PMM_PAGE_SIZE - 1u);
    return 1;
}

static void zero_page(uint64_t physical) {
    uint64_t *page = (uint64_t *)paging_direct_pointer(physical);
    if (!page) return;
    for (uint32_t i = 0; i < PMM_PAGE_SIZE / sizeof(uint64_t); ++i) {
        page[i] = 0;
    }
}

static process_t *find_process(uint64_t pid) {
    if (pid == 0) return 0;
    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES; ++i) {
        if (processes[i].state != PROCESS_UNUSED && processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return 0;
}

static process_t *allocate_slot(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES; ++i) {
        if (processes[i].state == PROCESS_UNUSED) return &processes[i];
    }
    return 0;
}

static int remember_frame(process_t *process, uint64_t physical) {
    if (!process || physical == UINT64_MAX ||
        process->owned_frame_count >= PROCESS_MAX_OWNED_FRAMES) {
        return 0;
    }
    process->owned_frames[process->owned_frame_count++] = physical;
    return 1;
}

static void release_process_memory(process_t *process) {
    if (!process) return;

    for (uint16_t i = 0; i < process->owned_frame_count; ++i) {
        (void)pmm_free_frame(process->owned_frames[i]);
    }
    process->owned_frame_count = 0;

    if (process->address_space.root_phys != 0) {
        (void)paging_destroy_user_address_space(&process->address_space);
    }
}

static process_status_t map_zeroed_page(process_t *process,
                                        uint64_t virtual_address,
                                        int writable,
                                        int executable,
                                        uint64_t *physical_out) {
    if (!process) return PROCESS_BAD_ARGUMENT;
    if (process->owned_frame_count >= PROCESS_MAX_OWNED_FRAMES) {
        return PROCESS_IMAGE_TOO_LARGE;
    }

    uint64_t physical = pmm_alloc_frame();
    if (physical == UINT64_MAX) return PROCESS_NO_MEMORY;
    if (!remember_frame(process, physical)) {
        (void)pmm_free_frame(physical);
        return PROCESS_IMAGE_TOO_LARGE;
    }

    if (!paging_direct_pointer(physical)) return PROCESS_PAGING_ERROR;
    zero_page(physical);

    paging_status_t status = paging_map_user_page(
        &process->address_space, virtual_address, physical,
        writable, executable);
    if (status != PAGING_OK) return PROCESS_PAGING_ERROR;

    if (physical_out) *physical_out = physical;
    return PROCESS_OK;
}

static process_status_t load_segment(process_t *process,
                                     const uint8_t *image,
                                     const user_elf_segment_t *segment) {
    if (!process || !image || !segment || segment->memory_size == 0) {
        return PROCESS_BAD_ARGUMENT;
    }

    uint64_t segment_end = segment->virtual_address + segment->memory_size;
    uint64_t file_end = segment->virtual_address + segment->file_size;
    uint64_t page_end;
    if (!align_up(segment_end, &page_end)) return PROCESS_BAD_ELF;

    uint64_t page_start = align_down(segment->virtual_address);
    int writable = (segment->flags & USER_ELF_PF_W) != 0;
    int executable = (segment->flags & USER_ELF_PF_X) != 0;

    for (uint64_t virtual_page = page_start;
         virtual_page < page_end;
         virtual_page += PMM_PAGE_SIZE) {
        uint64_t physical;
        process_status_t status = map_zeroed_page(
            process, virtual_page, writable, executable, &physical);
        if (status != PROCESS_OK) return status;

        uint64_t page_data_start =
            virtual_page > segment->virtual_address ?
            virtual_page : segment->virtual_address;
        uint64_t virtual_page_end = virtual_page + PMM_PAGE_SIZE;
        uint64_t page_data_end =
            virtual_page_end < file_end ? virtual_page_end : file_end;

        if (page_data_start < page_data_end) {
            uint8_t *destination =
                (uint8_t *)paging_direct_pointer(physical);
            if (!destination) return PROCESS_PAGING_ERROR;

            uint64_t source_offset =
                segment->file_offset +
                (page_data_start - segment->virtual_address);
            uint64_t destination_offset = page_data_start - virtual_page;
            uint64_t bytes = page_data_end - page_data_start;
            for (uint64_t i = 0; i < bytes; ++i) {
                destination[destination_offset + i] =
                    image[source_offset + i];
            }
        }
    }

    return PROCESS_OK;
}

static process_status_t map_user_stack(process_t *process) {
    for (uint32_t i = 0; i < PROCESS_USER_STACK_PAGES; ++i) {
        uint64_t virtual_address =
            PROCESS_USER_STACK_BASE + (uint64_t)i * PMM_PAGE_SIZE;
        process_status_t status = map_zeroed_page(
            process, virtual_address, 1, 0, 0);
        if (status != PROCESS_OK) return status;
    }
    process->user_stack_top = PROCESS_USER_STACK_TOP;
    return PROCESS_OK;
}

static __attribute__((noreturn)) void process_thread_entry(void *argument) {
    process_t *process = (process_t *)argument;
    if (!process || process->address_space.root_phys == 0 ||
        process->entry == 0 || process->user_stack_top == 0) {
        serial_write("JOSHOS_ERROR_PROCESS_ENTRY\n");
        scheduler_exit_current();
    }

    process->state = PROCESS_RUNNING;
    user_enter(process->address_space.root_phys,
               process->entry,
               process->user_stack_top);
}

void process_init(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES; ++i) {
        processes[i].pid = 0;
        processes[i].state = PROCESS_UNUSED;
        processes[i].exit_status = 0;
        processes[i].address_space.root_phys = 0;
        processes[i].entry = 0;
        processes[i].user_stack_top = 0;
        processes[i].owned_frame_count = 0;
    }
}

process_status_t process_spawn_elf(const void *image,
                                   size_t image_size,
                                   uint64_t *pid_out) {
    if (!image || image_size == 0) return PROCESS_BAD_ARGUMENT;

    user_elf_plan_t plan;
    if (user_elf_validate(image, image_size, &plan) != USER_ELF_OK) {
        return PROCESS_BAD_ELF;
    }

    process_t *process = allocate_slot();
    if (!process) return PROCESS_TABLE_FULL;

    process->pid = 0;
    process->state = PROCESS_READY;
    process->exit_status = 0;
    process->address_space.root_phys = 0;
    process->entry = plan.entry;
    process->user_stack_top = 0;
    process->owned_frame_count = 0;

    if (paging_create_user_address_space(&process->address_space) != PAGING_OK) {
        process->state = PROCESS_UNUSED;
        return PROCESS_PAGING_ERROR;
    }

    const uint8_t *bytes = (const uint8_t *)image;
    for (uint16_t i = 0; i < plan.segment_count; ++i) {
        user_elf_segment_t segment;
        if (user_elf_segment(image, image_size, i, &segment) != USER_ELF_OK) {
            release_process_memory(process);
            process->state = PROCESS_UNUSED;
            return PROCESS_BAD_ELF;
        }

        process_status_t status = load_segment(process, bytes, &segment);
        if (status != PROCESS_OK) {
            release_process_memory(process);
            process->state = PROCESS_UNUSED;
            return status;
        }
    }

    process_status_t stack_status = map_user_stack(process);
    if (stack_status != PROCESS_OK) {
        release_process_memory(process);
        process->state = PROCESS_UNUSED;
        return stack_status;
    }

    uint64_t thread_id = 0;
    if (!scheduler_create_user_thread(
            process_thread_entry, process,
            process->address_space.root_phys, &thread_id)) {
        release_process_memory(process);
        process->state = PROCESS_UNUSED;
        return PROCESS_SCHEDULER_ERROR;
    }

    process->pid = thread_id;
    if (pid_out) *pid_out = thread_id;
    return PROCESS_OK;
}

int process_mark_exit_current(uint64_t status) {
    process_t *process = find_process(scheduler_current_thread_id());
    if (!process) return 0;
    process->state = PROCESS_EXITED;
    process->exit_status = status;
    return 1;
}

process_status_t process_get_info(uint64_t pid, process_info_t *info_out) {
    if (!info_out) return PROCESS_BAD_ARGUMENT;
    process_t *process = find_process(pid);
    if (!process) return PROCESS_NOT_FOUND;

    info_out->pid = process->pid;
    info_out->state = process->state;
    info_out->exit_status = process->exit_status;
    info_out->entry = process->entry;
    info_out->user_stack_top = process->user_stack_top;
    info_out->owned_frame_count = process->owned_frame_count;
    return PROCESS_OK;
}

process_status_t process_reap(uint64_t pid) {
    process_t *process = find_process(pid);
    if (!process) return PROCESS_NOT_FOUND;
    if (process->state != PROCESS_EXITED) return PROCESS_NOT_EXITED;

    release_process_memory(process);
    process->pid = 0;
    process->state = PROCESS_UNUSED;
    process->exit_status = 0;
    process->entry = 0;
    process->user_stack_top = 0;
    return PROCESS_OK;
}

uint32_t process_live_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < PROCESS_MAX_PROCESSES; ++i) {
        if (processes[i].state == PROCESS_READY ||
            processes[i].state == PROCESS_RUNNING) {
            count++;
        }
    }
    return count;
}

const char *process_status_string(process_status_t status) {
    switch (status) {
        case PROCESS_OK: return "ok";
        case PROCESS_BAD_ARGUMENT: return "bad argument";
        case PROCESS_TABLE_FULL: return "process table full";
        case PROCESS_BAD_ELF: return "invalid userspace ELF";
        case PROCESS_IMAGE_TOO_LARGE: return "userspace image too large";
        case PROCESS_NO_MEMORY: return "out of physical memory";
        case PROCESS_PAGING_ERROR: return "process paging error";
        case PROCESS_SCHEDULER_ERROR: return "scheduler rejected process";
        case PROCESS_NOT_FOUND: return "process not found";
        case PROCESS_NOT_EXITED: return "process still live";
        default: return "unknown process error";
    }
}
