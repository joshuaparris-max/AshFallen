#include "user_elf.h"
#include <stddef.h>
#include <stdint.h>

#define EI_NIDENT 16u
#define ELFCLASS64 2u
#define ELFDATA2LSB 1u
#define EV_CURRENT 1u
#define ET_EXEC 2u
#define EM_X86_64 62u
#define PT_LOAD 1u

typedef struct {
    uint8_t ident[EI_NIDENT];
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
} __attribute__((packed)) elf64_phdr_t;

_Static_assert(sizeof(elf64_header_t) == 64, "ELF64 header layout");
_Static_assert(sizeof(elf64_phdr_t) == 56, "ELF64 program header layout");

static int add_overflows(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b;
}

static int range_inside(uint64_t offset, uint64_t length, uint64_t total) {
    return !add_overflows(offset, length) && offset + length <= total;
}

static int power_of_two(uint64_t value) {
    return value != 0 && (value & (value - 1u)) == 0;
}

static uint64_t page_down(uint64_t value) {
    return value & ~UINT64_C(0xfff);
}

static int page_up(uint64_t value, uint64_t *out) {
    if (!out || value > UINT64_MAX - UINT64_C(0xfff)) return 0;
    *out = (value + UINT64_C(0xfff)) & ~UINT64_C(0xfff);
    return 1;
}

static int user_range(uint64_t start, uint64_t length) {
    if (length == 0 || start < USER_ELF_VADDR_MIN ||
        start >= USER_ELF_VADDR_MAX || add_overflows(start, length)) {
        return 0;
    }
    return start + length <= USER_ELF_VADDR_MAX;
}

static const elf64_header_t *basic_header(const void *image,
                                           size_t image_size,
                                           user_elf_status_t *status) {
    if (!image || !status) return 0;
    if (image_size < sizeof(elf64_header_t)) {
        *status = USER_ELF_TRUNCATED;
        return 0;
    }

    const elf64_header_t *header = (const elf64_header_t *)image;
    if (header->ident[0] != 0x7f || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F') {
        *status = USER_ELF_BAD_MAGIC;
        return 0;
    }
    if (header->ident[4] != ELFCLASS64) {
        *status = USER_ELF_UNSUPPORTED_CLASS;
        return 0;
    }
    if (header->ident[5] != ELFDATA2LSB) {
        *status = USER_ELF_UNSUPPORTED_ENDIAN;
        return 0;
    }
    if (header->ident[6] != EV_CURRENT || header->version != EV_CURRENT) {
        *status = USER_ELF_UNSUPPORTED_VERSION;
        return 0;
    }
    if (header->type != ET_EXEC) {
        *status = USER_ELF_UNSUPPORTED_TYPE;
        return 0;
    }
    if (header->machine != EM_X86_64) {
        *status = USER_ELF_UNSUPPORTED_MACHINE;
        return 0;
    }
    if (header->ehsize != sizeof(elf64_header_t) ||
        header->phentsize != sizeof(elf64_phdr_t) ||
        header->phnum == 0 || header->phnum > USER_ELF_MAX_SEGMENTS) {
        *status = USER_ELF_BAD_PROGRAM_HEADERS;
        return 0;
    }

    uint64_t ph_bytes = (uint64_t)header->phnum * header->phentsize;
    if (!range_inside(header->phoff, ph_bytes, image_size)) {
        *status = USER_ELF_TRUNCATED;
        return 0;
    }

    *status = USER_ELF_OK;
    return header;
}

static const elf64_phdr_t *phdr(const void *image,
                                 const elf64_header_t *header,
                                 uint16_t index) {
    const uint8_t *bytes = (const uint8_t *)image;
    return (const elf64_phdr_t *)(const void *)(bytes + header->phoff +
        (uint64_t)index * header->phentsize);
}

static user_elf_status_t validate_segment(const elf64_phdr_t *segment,
                                          size_t image_size) {
    if (segment->filesz > segment->memsz) return USER_ELF_BAD_SEGMENT;
    if (segment->memsz == 0) return USER_ELF_OK;
    if (!range_inside(segment->offset, segment->filesz, image_size)) {
        return USER_ELF_TRUNCATED;
    }
    if (!user_range(segment->vaddr, segment->memsz)) {
        return USER_ELF_OUTSIDE_USER_RANGE;
    }
    if ((segment->flags & (USER_ELF_PF_W | USER_ELF_PF_X)) ==
        (USER_ELF_PF_W | USER_ELF_PF_X)) {
        return USER_ELF_WX_SEGMENT;
    }
    if (segment->align > 1u) {
        if (!power_of_two(segment->align)) return USER_ELF_BAD_SEGMENT;
        if ((segment->vaddr & (segment->align - 1u)) !=
            (segment->offset & (segment->align - 1u))) {
            return USER_ELF_BAD_SEGMENT;
        }
    }
    return USER_ELF_OK;
}

user_elf_status_t user_elf_validate(const void *image,
                                    size_t image_size,
                                    user_elf_plan_t *plan_out) {
    if (!plan_out) return USER_ELF_BAD_ARGUMENT;

    user_elf_status_t status = USER_ELF_OK;
    const elf64_header_t *header = basic_header(image, image_size, &status);
    if (!header) return status;

    uint64_t minimum = UINT64_MAX;
    uint64_t maximum = 0;
    uint16_t load_count = 0;
    int entry_executable = 0;

    for (uint16_t i = 0; i < header->phnum; ++i) {
        const elf64_phdr_t *segment = phdr(image, header, i);
        if (segment->type != PT_LOAD) continue;

        status = validate_segment(segment, image_size);
        if (status != USER_ELF_OK) return status;
        if (segment->memsz == 0) continue;

        uint64_t end = segment->vaddr + segment->memsz;
        if (segment->vaddr < minimum) minimum = segment->vaddr;
        if (end > maximum) maximum = end;
        load_count++;

        if ((segment->flags & USER_ELF_PF_X) != 0 &&
            header->entry >= segment->vaddr &&
            header->entry < end) {
            entry_executable = 1;
        }

        for (uint16_t j = 0; j < i; ++j) {
            const elf64_phdr_t *other = phdr(image, header, j);
            if (other->type != PT_LOAD || other->memsz == 0) continue;
            if (add_overflows(other->vaddr, other->memsz)) {
                return USER_ELF_BAD_SEGMENT;
            }
            uint64_t other_end = other->vaddr + other->memsz;
            uint64_t page_end;
            uint64_t other_page_end;
            if (!page_up(end, &page_end) || !page_up(other_end, &other_page_end)) {
                return USER_ELF_BAD_SEGMENT;
            }

            /*
             * x86 permissions apply to entire 4 KiB pages. Even byte-disjoint
             * PT_LOAD ranges may not share a page, otherwise an RX segment and
             * an RW segment could collapse into an effective W+X mapping.
             */
            if (page_down(segment->vaddr) < other_page_end &&
                page_down(other->vaddr) < page_end) {
                return USER_ELF_SEGMENT_OVERLAP;
            }
        }
    }

    if (load_count == 0) return USER_ELF_BAD_SEGMENT;
    if (!entry_executable) return USER_ELF_ENTRY_NOT_EXECUTABLE;

    plan_out->entry = header->entry;
    plan_out->virtual_min = minimum;
    plan_out->virtual_max = maximum;
    plan_out->segment_count = load_count;
    return USER_ELF_OK;
}

user_elf_status_t user_elf_segment(const void *image,
                                   size_t image_size,
                                   uint16_t load_index,
                                   user_elf_segment_t *segment_out) {
    if (!segment_out) return USER_ELF_BAD_ARGUMENT;

    user_elf_status_t status = USER_ELF_OK;
    const elf64_header_t *header = basic_header(image, image_size, &status);
    if (!header) return status;

    uint16_t current = 0;
    for (uint16_t i = 0; i < header->phnum; ++i) {
        const elf64_phdr_t *segment = phdr(image, header, i);
        if (segment->type != PT_LOAD || segment->memsz == 0) continue;

        status = validate_segment(segment, image_size);
        if (status != USER_ELF_OK) return status;
        if (current++ != load_index) continue;

        segment_out->file_offset = segment->offset;
        segment_out->virtual_address = segment->vaddr;
        segment_out->file_size = segment->filesz;
        segment_out->memory_size = segment->memsz;
        segment_out->alignment = segment->align;
        segment_out->flags = segment->flags;
        return USER_ELF_OK;
    }

    return USER_ELF_BAD_SEGMENT;
}

const char *user_elf_status_string(user_elf_status_t status) {
    switch (status) {
        case USER_ELF_OK: return "ok";
        case USER_ELF_BAD_ARGUMENT: return "bad argument";
        case USER_ELF_TRUNCATED: return "truncated image";
        case USER_ELF_BAD_MAGIC: return "bad ELF magic";
        case USER_ELF_UNSUPPORTED_CLASS: return "not ELF64";
        case USER_ELF_UNSUPPORTED_ENDIAN: return "not little endian";
        case USER_ELF_UNSUPPORTED_VERSION: return "unsupported ELF version";
        case USER_ELF_UNSUPPORTED_TYPE: return "unsupported ELF type";
        case USER_ELF_UNSUPPORTED_MACHINE: return "not x86-64";
        case USER_ELF_BAD_PROGRAM_HEADERS: return "invalid program headers";
        case USER_ELF_BAD_SEGMENT: return "invalid load segment";
        case USER_ELF_SEGMENT_OVERLAP: return "overlapping load segments";
        case USER_ELF_ENTRY_NOT_EXECUTABLE: return "entry outside executable segment";
        case USER_ELF_OUTSIDE_USER_RANGE: return "segment outside user virtual range";
        case USER_ELF_WX_SEGMENT: return "writable executable segment rejected";
        default: return "unknown user ELF error";
    }
}
