#include "elf64.h"

#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1u
#define ET_EXEC 2u
#define EM_X86_64 62u
#define PT_LOAD 1u
#define ELF64_MAX_PROGRAM_HEADERS 64u

typedef struct {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) elf64_header_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed)) elf64_program_header_t;

_Static_assert(sizeof(elf64_header_t) == 64, "ELF64 header layout changed");
_Static_assert(sizeof(elf64_program_header_t) == 56, "ELF64 program header layout changed");

static int add_overflows_u64(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b;
}

static int range_fits_size(uint64_t offset, uint64_t length, size_t size) {
    if (add_overflows_u64(offset, length)) return 0;
    return offset + length <= (uint64_t)size;
}

static int power_of_two_or_zero(uint64_t value) {
    return value == 0 || (value & (value - 1u)) == 0;
}

elf64_status_t elf64_load_image(const uint8_t *image,
                                size_t image_size,
                                elf64_segment_writer_t writer,
                                void *writer_context,
                                uint64_t *entry_out) {
    if (!image || !writer || !entry_out) return ELF64_BAD_ARGUMENT;
    if (image_size < sizeof(elf64_header_t)) return ELF64_TRUNCATED;

    const elf64_header_t *header = (const elf64_header_t *)image;
    if (header->ident[0] != 0x7fu || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F') {
        return ELF64_BAD_MAGIC;
    }
    if (header->ident[EI_CLASS] != ELFCLASS64) return ELF64_UNSUPPORTED_CLASS;
    if (header->ident[EI_DATA] != ELFDATA2LSB) return ELF64_UNSUPPORTED_ENDIAN;
    if (header->ident[EI_VERSION] != EV_CURRENT || header->version != EV_CURRENT) {
        return ELF64_BAD_MAGIC;
    }
    if (header->type != ET_EXEC) return ELF64_UNSUPPORTED_TYPE;
    if (header->machine != EM_X86_64) return ELF64_UNSUPPORTED_MACHINE;
    if (header->ehsize != sizeof(elf64_header_t) ||
        header->phentsize != sizeof(elf64_program_header_t) ||
        header->phnum == 0 || header->phnum > ELF64_MAX_PROGRAM_HEADERS) {
        return ELF64_BAD_PROGRAM_TABLE;
    }

    uint64_t table_bytes = (uint64_t)header->phnum * (uint64_t)header->phentsize;
    if (!range_fits_size(header->phoff, table_bytes, image_size)) {
        return ELF64_BAD_PROGRAM_TABLE;
    }

    uint32_t load_segments = 0;
    for (uint16_t i = 0; i < header->phnum; ++i) {
        uint64_t ph_offset = header->phoff + (uint64_t)i * header->phentsize;
        const elf64_program_header_t *program =
            (const elf64_program_header_t *)(image + ph_offset);
        if (program->type != PT_LOAD) continue;

        if (program->filesz > program->memsz ||
            !range_fits_size(program->offset, program->filesz, image_size) ||
            add_overflows_u64(program->vaddr, program->memsz) ||
            !power_of_two_or_zero(program->align) ||
            (program->align > 1u &&
             (program->vaddr & (program->align - 1u)) !=
             (program->offset & (program->align - 1u)))) {
            return ELF64_BAD_SEGMENT;
        }

        if (!writer(writer_context,
                    program->vaddr,
                    image + program->offset,
                    program->filesz,
                    program->memsz,
                    program->flags)) {
            return ELF64_WRITER_FAILED;
        }
        load_segments++;
    }

    if (load_segments == 0) return ELF64_NO_LOAD_SEGMENTS;
    *entry_out = header->entry;
    return ELF64_OK;
}
