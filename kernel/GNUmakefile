.SUFFIXES:
.DELETE_ON_ERROR:

OUTPUT := kernel
CC := clang
HOSTCC ?= cc

CFLAGS := \
	-target x86_64-unknown-none-elf \
	-std=gnu11 \
	-O2 \
	-g \
	-Wall \
	-Wextra \
	-Werror \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-PIC \
	-ffunction-sections \
	-fdata-sections \
	-m64 \
	-march=x86-64 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-mno-red-zone \
	-mgeneral-regs-only \
	-mcmodel=kernel

ASFLAGS := \
	-target x86_64-unknown-none-elf \
	-g \
	-m64 \
	-march=x86-64 \
	-mno-red-zone \
	-mcmodel=kernel

CPPFLAGS := \
	-nostdinc \
	-I src \
	-I limine-protocol/include \
	-isystem freestanding-c-hdrs/include \
	-MMD \
	-MP
CPPFLAGS += $(EXTRA_CPPFLAGS)

LDFLAGS := \
	-target x86_64-unknown-none-elf \
	-fuse-ld=lld \
	-nostdlib \
	-static \
	-Wl,-m,elf_x86_64 \
	-Wl,-z,max-page-size=0x1000 \
	-Wl,-z,noexecstack \
	-Wl,--gc-sections \
	-Wl,-T,linker-scripts/x86_64.lds \
	-Wl,--build-id=none \

C_SRC := $(shell find src -type f -name '*.c' | LC_ALL=C sort)
ASM_SRC := $(shell find src -type f -name '*.S' | LC_ALL=C sort)
OBJ := $(patsubst %.c,obj/%.o,$(C_SRC)) $(patsubst %.S,obj/%.o,$(ASM_SRC))
DEPS := $(OBJ:.o=.d)
HOST_TEST := obj-host/test_boot_josh

.PHONY: all clean distclean host-tests
all: bin/$(OUTPUT)

-include $(DEPS)

bin/$(OUTPUT): $(OBJ) linker-scripts/x86_64.lds
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

obj/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

obj/%.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) $(CPPFLAGS) -c $< -o $@

$(HOST_TEST): tests/test_boot_josh.c src/boot_josh.c src/boot.h src/boot_internal.h src/josh_boot_protocol.h
	mkdir -p $(dir $@)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -I src tests/test_boot_josh.c src/boot_josh.c -o $@

host-tests: $(HOST_TEST)
	$(HOST_TEST)

clean:
	rm -rf bin obj obj-host

distclean: clean
	rm -rf .deps-obtained freestanding-c-hdrs limine-protocol
