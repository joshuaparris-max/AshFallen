#ifndef JOSHOS_USER_ELF_H
#define JOSHOS_USER_ELF_H

#include <stddef.h>
#include <stdint.h>

#define USER_ELF_MAX_SEGMENTS 32u
#define USER_ELF_VADDR_MIN UINT64_C(0x0000400000000000)
#define USER_ELF_VADDR_MAX UINT64_C(0x0000800000000000)

#define USER_ELF_PF_X 1u
#define USER_ELF_PF_W 2u
#define USER_ELF_PF_R 4u

typedef enum {
    USER_ELF_OK = 0,
    USER_ELF_BAD_ARGUMENT,
    USER_ELF_TRUNCATED,
    USER_ELF_BAD_MAGIC,
    USER_ELF_UNSUPPORTED_CLASS,
    USER_ELF_UNSUPPORTED_ENDIAN,
    USER_ELF_UNSUPPORTED_VERSION,
    USER_ELF_UNSUPPORTED_TYPE,
    USER_ELF_UNSUPPORTED_MACHINE,
    USER_ELF_BAD_PROGRAM_HEADERS,
    USER_ELF_BAD_SEGMENT,
    USER_ELF_SEGMENT_OVERLAP,
    USER_ELF_ENTRY_NOT_EXECUTABLE,
    USER_ELF_OUTSIDE_USER_RANGE,
    USER_ELF_WX_SEGMENT
} user_elf_status_t;

typedef struct {
    uint64_t file_offset;
    uint64_t virtual_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
    uint32_t flags;
} user_elf_segment_t;

typedef struct {
    uint64_t entry;
    uint64_t virtual_min;
    uint64_t virtual_max;
    uint16_t segment_count;
} user_elf_plan_t;

user_elf_status_t user_elf_validate(const void *image,
                                    size_t image_size,
                                    user_elf_plan_t *plan_out);
user_elf_status_t user_elf_segment(const void *image,
                                   size_t image_size,
                                   uint16_t load_index,
                                   user_elf_segment_t *segment_out);
const char *user_elf_status_string(user_elf_status_t status);

#endif
