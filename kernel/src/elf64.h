#ifndef JOSHOS_ELF64_H
#define JOSHOS_ELF64_H

#include <stddef.h>
#include <stdint.h>

#define ELF64_PF_X 0x1u
#define ELF64_PF_W 0x2u
#define ELF64_PF_R 0x4u

typedef enum {
    ELF64_OK = 0,
    ELF64_BAD_ARGUMENT,
    ELF64_TRUNCATED,
    ELF64_BAD_MAGIC,
    ELF64_UNSUPPORTED_CLASS,
    ELF64_UNSUPPORTED_ENDIAN,
    ELF64_UNSUPPORTED_TYPE,
    ELF64_UNSUPPORTED_MACHINE,
    ELF64_BAD_PROGRAM_TABLE,
    ELF64_BAD_SEGMENT,
    ELF64_NO_LOAD_SEGMENTS,
    ELF64_WRITER_FAILED
} elf64_status_t;

typedef int (*elf64_segment_writer_t)(void *context,
                                      uint64_t virtual_address,
                                      const uint8_t *file_data,
                                      uint64_t file_size,
                                      uint64_t memory_size,
                                      uint32_t flags);

elf64_status_t elf64_load_image(const uint8_t *image,
                                size_t image_size,
                                elf64_segment_writer_t writer,
                                void *writer_context,
                                uint64_t *entry_out);

#endif
