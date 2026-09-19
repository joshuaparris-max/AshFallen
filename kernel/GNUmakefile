.SUFFIXES:
.DELETE_ON_ERROR:

OUTPUT := kernel
CC := clang
HOSTCC ?= cc
HOST_CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror

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
HOST_TEST_BOOT := obj-host/test_boot_josh
HOST_TEST_PMM := obj-host/test_pmm
HOST_TEST_CPU := obj-host/test_cpu
HOST_TEST_HEAP := obj-host/test_heap
HOST_TEST_NET := obj-host/test_net_proto
HOST_TEST_NETDEV := obj-host/test_net
HOST_TEST_SPINLOCK := obj-host/test_spinlock
HOST_TEST_ACPI := obj-host/test_acpi
HOST_TEST_APIC_ROUTE := obj-host/test_apic_route
HOST_TEST_USER_ELF := obj-host/test_user_elf
HOST_TEST_PCI := obj-host/test_pci
HOST_TEST_INPUT := obj-host/test_input
HOST_TEST_TIMER := obj-host/test_timer
HOST_TEST_KEYBOARD := obj-host/test_keyboard_decode
HOST_TEST_SMP := obj-host/test_smp_topology
HOST_TEST_STORAGE := obj-host/test_storage
HOST_TEST_VFS := obj-host/test_vfs
HOST_TESTS := $(HOST_TEST_BOOT) $(HOST_TEST_PMM) $(HOST_TEST_CPU) $(HOST_TEST_HEAP) $(HOST_TEST_NET) $(HOST_TEST_NETDEV) $(HOST_TEST_SPINLOCK) $(HOST_TEST_ACPI) $(HOST_TEST_APIC_ROUTE) $(HOST_TEST_PCI) $(HOST_TEST_INPUT) $(HOST_TEST_TIMER) $(HOST_TEST_KEYBOARD) $(HOST_TEST_SMP) $(HOST_TEST_STORAGE) $(HOST_TEST_VFS) $(HOST_TEST_USER_ELF)

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

$(HOST_TEST_BOOT): tests/test_boot_josh.c src/boot_josh.c src/boot.h src/boot_internal.h src/josh_boot_protocol.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_boot_josh.c src/boot_josh.c -o $@

$(HOST_TEST_PMM): tests/test_pmm.c src/pmm.c src/pmm.h src/boot.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_pmm.c src/pmm.c -o $@

$(HOST_TEST_CPU): tests/test_cpu.c src/cpu.c src/cpu.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_cpu.c src/cpu.c -o $@

$(HOST_TEST_HEAP): tests/test_heap.c src/heap.c src/heap.h src/boot.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_heap.c src/heap.c -o $@

$(HOST_TEST_NET): tests/test_net_proto.c src/net_proto.c src/net_proto.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_net_proto.c src/net_proto.c -o $@

$(HOST_TEST_NETDEV): tests/test_net.c src/net.c src/net.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_net.c src/net.c -o $@

$(HOST_TEST_SPINLOCK): tests/test_spinlock.c src/spinlock.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -pthread -I src tests/test_spinlock.c -o $@

$(HOST_TEST_ACPI): tests/test_acpi.c src/acpi.c src/acpi.h src/boot.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_acpi.c src/acpi.c -o $@

$(HOST_TEST_APIC_ROUTE): tests/test_apic_route.c src/apic_route.c src/apic.h src/acpi.h src/cpu.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_apic_route.c src/apic_route.c -o $@

$(HOST_TEST_PCI): tests/test_pci.c src/pci.c src/pci.h src/io.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_pci.c src/pci.c -o $@

$(HOST_TEST_INPUT): tests/test_input.c src/input.c src/input.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_input.c src/input.c -o $@

$(HOST_TEST_TIMER): tests/test_timer.c src/timer_math.c src/timer.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_timer.c src/timer_math.c -o $@

$(HOST_TEST_KEYBOARD): tests/test_keyboard_decode.c src/keyboard_decode.c src/keyboard_decode.h src/input.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_keyboard_decode.c src/keyboard_decode.c -o $@

$(HOST_TEST_SMP): tests/test_smp_topology.c src/smp_topology.c src/smp.h src/acpi.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_smp_topology.c src/smp_topology.c -o $@

$(HOST_TEST_STORAGE): tests/test_storage.c src/block.c src/block.h src/block_cache.c src/block_cache.h src/partition.c src/partition.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_storage.c src/block.c src/block_cache.c src/partition.c -o $@

$(HOST_TEST_VFS): tests/test_vfs.c src/vfs.c src/vfs.h src/tmpfs.c src/tmpfs.h src/storage_services.c src/storage_services.h
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_CFLAGS) -I src tests/test_vfs.c src/vfs.c src/tmpfs.c src/storage_services.c -o $@

$(HOST_TEST_USER_ELF): tests/test_user_elf.c src/user_elf.c src/user_elf.h
	mkdir -p $(dir $@)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -I src tests/test_user_elf.c src/user_elf.c -o $@

host-tests: $(HOST_TESTS)
	$(HOST_TEST_BOOT)
	$(HOST_TEST_PMM)
	$(HOST_TEST_CPU)
	$(HOST_TEST_HEAP)
	$(HOST_TEST_NET)
	$(HOST_TEST_NETDEV)
	$(HOST_TEST_SPINLOCK)
	$(HOST_TEST_ACPI)
	$(HOST_TEST_APIC_ROUTE)
	$(HOST_TEST_USER_ELF)
	$(HOST_TEST_PCI)
	$(HOST_TEST_INPUT)
	$(HOST_TEST_TIMER)
	$(HOST_TEST_KEYBOARD)
	$(HOST_TEST_SMP)
	$(HOST_TEST_STORAGE)
	$(HOST_TEST_VFS)

clean:
	rm -rf bin obj obj-host

distclean: clean
	rm -rf .deps-obtained freestanding-c-hdrs limine-protocol
