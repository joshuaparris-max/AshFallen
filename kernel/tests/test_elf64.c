#include "elf64.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
} __attribute__((packed)) test_header_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed)) test_program_t;

static int failures;
static int writes;
static uint64_t last_vaddr;
static uint64_t last_filesz;
static uint64_t last_memsz;
static uint32_t last_flags;

static void expect(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static int writer(void *context,
                  uint64_t vaddr,
                  const uint8_t *data,
                  uint64_t filesz,
                  uint64_t memsz,
                  uint32_t flags) {
    (void)context;
    writes++;
    last_vaddr = vaddr;
    last_filesz = filesz;
    last_memsz = memsz;
    last_flags = flags;
    return filesz == 4 && data[0] == 0xaa;
}

static void make_valid(uint8_t image[512]) {
    memset(image, 0, 512);
    test_header_t *h = (test_header_t *)image;
    h->ident[0] = 0x7f;
    h->ident[1] = 'E';
    h->ident[2] = 'L';
    h->ident[3] = 'F';
    h->ident[4] = 2;
    h->ident[5] = 1;
    h->ident[6] = 1;
    h->type = 2;
    h->machine = 62;
    h->version = 1;
    h->entry = 0x400000;
    h->phoff = sizeof(*h);
    h->ehsize = sizeof(*h);
    h->phentsize = sizeof(test_program_t);
    h->phnum = 1;

    test_program_t *p = (test_program_t *)(image + h->phoff);
    p->type = 1;
    p->flags = ELF64_PF_R | ELF64_PF_X;
    p->offset = 0x100;
    p->vaddr = 0x400000;
    p->filesz = 4;
    p->memsz = 8;
    p->align = 1;

    image[0x100] = 0xaa;
    image[0x101] = 0xbb;
    image[0x102] = 0xcc;
    image[0x103] = 0xdd;
}

int main(void) {
    uint8_t image[512];
    uint64_t entry = 0;

    make_valid(image);
    writes = 0;
    expect("valid ELF loads",
           elf64_load_image(image, sizeof(image), writer, 0, &entry) == ELF64_OK);
    expect("entry preserved", entry == 0x400000);
    expect("one load segment", writes == 1);
    expect("segment address", last_vaddr == 0x400000);
    expect("segment file size", last_filesz == 4);
    expect("segment memory size", last_memsz == 8);
    expect("segment flags", last_flags == (ELF64_PF_R | ELF64_PF_X));

    make_valid(image);
    ((test_program_t *)(image + sizeof(test_header_t)))->align = 0x1000;
    expect("misaligned load segment rejected",
           elf64_load_image(image, sizeof(image), writer, 0, &entry) == ELF64_BAD_SEGMENT);

    make_valid(image);
    ((test_header_t *)image)->machine = 3;
    expect("wrong machine rejected",
           elf64_load_image(image, sizeof(image), writer, 0, &entry) ==
           ELF64_UNSUPPORTED_MACHINE);

    make_valid(image);
    ((test_program_t *)(image + sizeof(test_header_t)))->filesz = 9;
    ((test_program_t *)(image + sizeof(test_header_t)))->memsz = 8;
    expect("filesz larger than memsz rejected",
           elf64_load_image(image, sizeof(image), writer, 0, &entry) == ELF64_BAD_SEGMENT);

    make_valid(image);
    ((test_program_t *)(image + sizeof(test_header_t)))->offset = 510;
    ((test_program_t *)(image + sizeof(test_header_t)))->filesz = 4;
    expect("truncated segment rejected",
           elf64_load_image(image, sizeof(image), writer, 0, &entry) == ELF64_BAD_SEGMENT);

    make_valid(image);
    ((test_header_t *)image)->phoff = 500;
    expect("truncated program table rejected",
           elf64_load_image(image, sizeof(image), writer, 0, &entry) ==
           ELF64_BAD_PROGRAM_TABLE);

    if (failures) return 1;
    puts("ELF64 loader tests passed");
    return 0;
}
