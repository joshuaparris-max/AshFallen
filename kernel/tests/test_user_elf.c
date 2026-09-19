#include "user_elf.h"
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
} __attribute__((packed)) ehdr_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed)) phdr_t;

static int failures;

static void expect(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void make_valid(uint8_t image[8192]) {
    memset(image, 0, 8192);
    ehdr_t *h = (ehdr_t *)image;
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
    h->entry = USER_ELF_VADDR_MIN + 0x1000;
    h->phoff = sizeof(*h);
    h->ehsize = sizeof(*h);
    h->phentsize = sizeof(phdr_t);
    h->phnum = 2;

    phdr_t *p = (phdr_t *)(image + h->phoff);
    p[0].type = 1;
    p[0].flags = USER_ELF_PF_R | USER_ELF_PF_X;
    p[0].offset = 0x1000;
    p[0].vaddr = USER_ELF_VADDR_MIN + 0x1000;
    p[0].filesz = 32;
    p[0].memsz = 32;
    p[0].align = 0x1000;

    p[1].type = 1;
    p[1].flags = USER_ELF_PF_R | USER_ELF_PF_W;
    p[1].offset = 0x200;
    p[1].vaddr = USER_ELF_VADDR_MIN + 0x2000;
    p[1].filesz = 16;
    p[1].memsz = 128;
    p[1].align = 0x100;
}

int main(void) {
    uint8_t image[8192];
    user_elf_plan_t plan;
    user_elf_segment_t segment;

    make_valid(image);
    expect("valid executable",
           user_elf_validate(image, sizeof(image), &plan) == USER_ELF_OK);
    expect("entry preserved", plan.entry == USER_ELF_VADDR_MIN + 0x1000);
    expect("two load segments", plan.segment_count == 2);
    expect("segment extraction",
           user_elf_segment(image, sizeof(image), 1, &segment) == USER_ELF_OK &&
           segment.virtual_address == USER_ELF_VADDR_MIN + 0x2000 &&
           segment.file_size == 16 && segment.memory_size == 128);

    make_valid(image);
    ((ehdr_t *)image)->ident[0] = 0;
    expect("bad magic", user_elf_validate(image, sizeof(image), &plan) == USER_ELF_BAD_MAGIC);

    make_valid(image);
    ((ehdr_t *)image)->machine = 3;
    expect("wrong machine",
           user_elf_validate(image, sizeof(image), &plan) == USER_ELF_UNSUPPORTED_MACHINE);

    make_valid(image);
    phdr_t *p = (phdr_t *)(image + ((ehdr_t *)image)->phoff);
    p[0].flags |= USER_ELF_PF_W;
    expect("W+X rejected",
           user_elf_validate(image, sizeof(image), &plan) == USER_ELF_WX_SEGMENT);

    make_valid(image);
    p = (phdr_t *)(image + ((ehdr_t *)image)->phoff);
    p[0].vaddr = 0x400000;
    expect("low address rejected",
           user_elf_validate(image, sizeof(image), &plan) == USER_ELF_OUTSIDE_USER_RANGE);

    make_valid(image);
    p = (phdr_t *)(image + ((ehdr_t *)image)->phoff);
    p[1].vaddr = p[0].vaddr + 8;
    p[1].align = 1;
    expect("overlap rejected",
           user_elf_validate(image, sizeof(image), &plan) == USER_ELF_SEGMENT_OVERLAP);

    make_valid(image);
    p = (phdr_t *)(image + ((ehdr_t *)image)->phoff);
    p[0].filesz = 0x100;
    p[0].memsz = 0x100;
    p[0].align = 1;
    p[1].vaddr = p[0].vaddr + 0x800;
    p[1].align = 1;
    expect("shared page rejected",
           user_elf_validate(image, sizeof(image), &plan) == USER_ELF_SEGMENT_OVERLAP);

    make_valid(image);
    ((ehdr_t *)image)->entry = USER_ELF_VADDR_MIN + 0x2000;
    expect("non-executable entry rejected",
           user_elf_validate(image, sizeof(image), &plan) == USER_ELF_ENTRY_NOT_EXECUTABLE);

    make_valid(image);
    p = (phdr_t *)(image + ((ehdr_t *)image)->phoff);
    p[0].filesz = p[0].memsz + 1;
    expect("filesz larger than memsz rejected",
           user_elf_validate(image, sizeof(image), &plan) == USER_ELF_BAD_SEGMENT);

    make_valid(image);
    p = (phdr_t *)(image + ((ehdr_t *)image)->phoff);
    p[0].offset = sizeof(image) - 8;
    p[0].filesz = 32;
    p[0].memsz = 32;
    p[0].align = 1;
    expect("truncated segment rejected",
           user_elf_validate(image, sizeof(image), &plan) == USER_ELF_TRUNCATED);

    if (failures) return 1;
    puts("userspace ELF64 validation tests passed");
    return 0;
}
