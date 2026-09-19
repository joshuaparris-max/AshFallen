.SUFFIXES:
.DELETE_ON_ERROR:

IMAGE := JoshOS-0.1-x86_64
FAULT_IMAGE := JoshOS-fault-test-x86_64
DOUBLE_FAULT_IMAGE := JoshOS-double-fault-test-x86_64
DIVIDE_FAULT_IMAGE := JoshOS-divide-fault-test-x86_64
PAGE_FAULT_IMAGE := JoshOS-page-fault-test-x86_64
GP_FAULT_IMAGE := JoshOS-gp-fault-test-x86_64
RING3_IMAGE := JoshOS-ring3-test-x86_64
LIMINE_VERSION := 12.9.0
LIMINE_SHA256 := 84059c93b4ea03994af6d614654c7095291388850ea7b258d64f9263abde5557
LIMINE_URL := https://github.com/Limine-Bootloader/Limine/releases/download/v$(LIMINE_VERSION)/limine-binary.tar.gz

.PHONY: all kernel host-tests run smoke keyboard-smoke fault-smoke ring3-smoke clean distclean
all: $(IMAGE).iso

limine-binary.tar.gz:
	curl -fL -o $@ $(LIMINE_URL)
	echo "$(LIMINE_SHA256)  $@" | sha256sum -c -

limine-binary/limine: limine-binary.tar.gz
	rm -rf limine-binary
	gzip -dc limine-binary.tar.gz | tar -xf -
	$(MAKE) -C limine-binary

kernel/.deps-obtained:
	sh kernel/get-deps

kernel: kernel/.deps-obtained
	$(MAKE) -C kernel EXTRA_CPPFLAGS="$(EXTRA_CPPFLAGS)"

host-tests:
	$(MAKE) -C kernel host-tests

$(IMAGE).iso: limine-binary/limine kernel limine.conf
	rm -rf iso_root
	mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
	cp kernel/bin/kernel iso_root/boot/kernel
	cp limine.conf iso_root/boot/limine/limine.conf
	cp limine-binary/limine-bios.sys limine-binary/limine-bios-cd.bin iso_root/boot/limine/
	cp limine-binary/limine-uefi-cd.bin iso_root/boot/limine/
	cp limine-binary/BOOTX64.EFI limine-binary/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $@
	./limine-binary/limine bios-install $@
	rm -rf iso_root

run: $(IMAGE).iso
	qemu-system-x86_64 -M q35 -m 512M -cdrom $(IMAGE).iso -nic user,model=e1000

smoke: $(IMAGE).iso
	rm -f boot.log
	-timeout 10s qemu-system-x86_64 -M q35 -smp 2 -m 256M -cdrom $(IMAGE).iso -nic user,model=e1000 -display none -serial stdio -no-reboot > boot.log 2>&1
	grep -q JOSHOS_CPU_FEATURES_OK boot.log
	grep -q JOSHOS_NX_OK boot.log
	grep -q JOSHOS_GDT_TSS_OK boot.log
	grep -q JOSHOS_IDT_OK boot.log
	grep -q JOSHOS_PMM_OK boot.log
	grep -q JOSHOS_PMM_STRESS_OK boot.log
	grep -q JOSHOS_PAGING_TABLES_OK boot.log
	grep -q JOSHOS_PAGING_OWNED_OK boot.log
	grep -q JOSHOS_PAGING_PERMISSIONS_OK boot.log
	grep -q JOSHOS_SCHEDULER_OK boot.log
	grep -q JOSHOS_HEAP_OK boot.log
	grep -q JOSHOS_HEAP_SELF_TEST_OK boot.log
	grep -q JOSHOS_PCI_OK boot.log
	grep -q JOSHOS_E1000_OK boot.log
	grep -q JOSHOS_NET_LOOPBACK_OK boot.log
	grep -q JOSHOS_ACPI_MADT_OK boot.log
	grep -q JOSHOS_APIC_OK boot.log
	grep -q JOSHOS_IOAPIC_OK boot.log
	grep -q JOSHOS_SMP_TOPOLOGY_OK boot.log
	grep -q JOSHOS_PERCPU_STACKS_OK boot.log
	grep -q JOSHOS_SMP_MULTICPU_OK boot.log
	grep -q JOSHOS_TIMER_CONFIG_OK boot.log
	grep -q JOSHOS_KEYBOARD_IRQ_CONFIG_OK boot.log
	grep -q JOSHOS_TIMER_IRQ_OK boot.log
	grep -q JOSHOS_IPI_OK boot.log
	grep -q JOSHOS_INTERRUPT_INPUT_READY boot.log
	grep -q JOSHOS_BOOT_OK boot.log
	@echo "Josh OS boot smoke test passed."

keyboard-smoke: $(IMAGE).iso
	python3 kernel/tests/qemu_keyboard_smoke.py $(IMAGE).iso

fault-smoke: limine-binary/limine kernel/.deps-obtained limine.conf
	$(MAKE) -C kernel clean
	$(MAKE) IMAGE=$(FAULT_IMAGE) EXTRA_CPPFLAGS=-DJOSHOS_FAULT_TEST_UD2 $(FAULT_IMAGE).iso
	rm -f fault-boot.log
	-timeout 10s qemu-system-x86_64 -M q35 -m 256M -cdrom $(FAULT_IMAGE).iso -display none -serial stdio -no-reboot > fault-boot.log 2>&1
	grep -q JOSHOS_GDT_TSS_OK fault-boot.log
	grep -q JOSHOS_IDT_OK fault-boot.log
	grep -q JOSHOS_FAULT_TEST_UD2 fault-boot.log
	grep -q 'VECTOR=0x0000000000000006' fault-boot.log
	grep -q JOSHOS_REGISTER_DUMP fault-boot.log
	grep -q 'RAX=0x1122334455667788' fault-boot.log
	grep -q 'R15=0x8877665544332211' fault-boot.log
	grep -q JOSHOS_PANIC_HALT fault-boot.log
	@echo "Josh OS invalid-opcode register-frame smoke test passed."
	$(MAKE) -C kernel clean
	rm -f fault-boot.log $(FAULT_IMAGE).iso

	$(MAKE) IMAGE=$(DIVIDE_FAULT_IMAGE) EXTRA_CPPFLAGS=-DJOSHOS_FAULT_TEST_DIVIDE $(DIVIDE_FAULT_IMAGE).iso
	rm -f divide-fault-boot.log
	-timeout 10s qemu-system-x86_64 -M q35 -m 256M -cdrom $(DIVIDE_FAULT_IMAGE).iso -display none -serial stdio -no-reboot > divide-fault-boot.log 2>&1
	grep -q JOSHOS_FAULT_TEST_DIVIDE divide-fault-boot.log
	grep -q 'VECTOR=0x0000000000000000' divide-fault-boot.log
	grep -q JOSHOS_REGISTER_DUMP divide-fault-boot.log
	grep -q JOSHOS_PANIC_HALT divide-fault-boot.log
	@echo "Josh OS divide-by-zero smoke test passed."
	$(MAKE) -C kernel clean
	rm -f divide-fault-boot.log $(DIVIDE_FAULT_IMAGE).iso

	$(MAKE) IMAGE=$(PAGE_FAULT_IMAGE) EXTRA_CPPFLAGS=-DJOSHOS_FAULT_TEST_PAGE $(PAGE_FAULT_IMAGE).iso
	rm -f page-fault-boot.log
	-timeout 10s qemu-system-x86_64 -M q35 -m 256M -cdrom $(PAGE_FAULT_IMAGE).iso -display none -serial stdio -no-reboot > page-fault-boot.log 2>&1
	cat page-fault-boot.log
	grep -q JOSHOS_FAULT_TEST_PAGE page-fault-boot.log
	grep -qi 'VECTOR=0x000000000000000E' page-fault-boot.log
	grep -q 'CR2=0x00007FFFFFFFF000' page-fault-boot.log
	grep -q JOSHOS_REGISTER_DUMP page-fault-boot.log
	grep -q JOSHOS_PANIC_HALT page-fault-boot.log
	@echo "Josh OS page-fault smoke test passed."
	$(MAKE) -C kernel clean
	rm -f page-fault-boot.log $(PAGE_FAULT_IMAGE).iso

	$(MAKE) IMAGE=$(GP_FAULT_IMAGE) EXTRA_CPPFLAGS=-DJOSHOS_FAULT_TEST_GP $(GP_FAULT_IMAGE).iso
	rm -f gp-fault-boot.log
	-timeout 10s qemu-system-x86_64 -M q35 -m 256M -cdrom $(GP_FAULT_IMAGE).iso -display none -serial stdio -no-reboot > gp-fault-boot.log 2>&1
	grep -q JOSHOS_FAULT_TEST_GP gp-fault-boot.log
	grep -qi 'VECTOR=0x000000000000000D' gp-fault-boot.log
	grep -q JOSHOS_REGISTER_DUMP gp-fault-boot.log
	grep -q JOSHOS_PANIC_HALT gp-fault-boot.log
	@echo "Josh OS general-protection-fault smoke test passed."
	$(MAKE) -C kernel clean
	rm -f gp-fault-boot.log $(GP_FAULT_IMAGE).iso

	$(MAKE) IMAGE=$(DOUBLE_FAULT_IMAGE) EXTRA_CPPFLAGS=-DJOSHOS_FAULT_TEST_DOUBLE_FAULT $(DOUBLE_FAULT_IMAGE).iso
	rm -f double-fault-boot.log
	-timeout 10s qemu-system-x86_64 -M q35 -m 256M -cdrom $(DOUBLE_FAULT_IMAGE).iso -display none -serial stdio -no-reboot > double-fault-boot.log 2>&1
	grep -q JOSHOS_GDT_TSS_OK double-fault-boot.log
	grep -q JOSHOS_IDT_OK double-fault-boot.log
	grep -q JOSHOS_FAULT_TEST_DOUBLE_FAULT double-fault-boot.log
	grep -q 'VECTOR=0x0000000000000008' double-fault-boot.log
	grep -q 'ERROR=0x0000000000000000' double-fault-boot.log
	grep -q JOSHOS_REGISTER_DUMP double-fault-boot.log
	grep -q JOSHOS_PANIC_HALT double-fault-boot.log
	@echo "Josh OS double-fault smoke test passed."
	$(MAKE) -C kernel clean
	rm -f double-fault-boot.log $(DOUBLE_FAULT_IMAGE).iso

ring3-smoke: limine-binary/limine kernel/.deps-obtained limine.conf
	$(MAKE) -C kernel clean
	$(MAKE) IMAGE=$(RING3_IMAGE) EXTRA_CPPFLAGS=-DJOSHOS_RING3_TEST $(RING3_IMAGE).iso
	rm -f ring3-boot.log
	-timeout 10s qemu-system-x86_64 -M q35 -m 256M -cdrom $(RING3_IMAGE).iso -display none -serial stdio -no-reboot > ring3-boot.log 2>&1
	cat ring3-boot.log
	grep -q JOSHOS_SCHEDULER_OK ring3-boot.log
	grep -q JOSHOS_USER_PAGING_OK ring3-boot.log
	grep -q JOSHOS_RING3_ENTER ring3-boot.log
	grep -q JOSHOS_RING3_SYSCALL_OK ring3-boot.log
	grep -q JOSHOS_USERSPACE_HELLO ring3-boot.log
	grep -q 'JOSHOS_USERSPACE_EXIT_STATUS=0x0000000000000000' ring3-boot.log
	grep -q JOSHOS_USERSPACE_EXIT_OK ring3-boot.log
	@echo "Josh OS Ring 3 syscall smoke test passed."
	$(MAKE) -C kernel clean
	rm -f ring3-boot.log $(RING3_IMAGE).iso

clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root boot.log keyboard-smoke.log keyboard-smoke.monitor fault-boot.log divide-fault-boot.log page-fault-boot.log gp-fault-boot.log double-fault-boot.log ring3-boot.log $(IMAGE).iso $(FAULT_IMAGE).iso $(DIVIDE_FAULT_IMAGE).iso $(PAGE_FAULT_IMAGE).iso $(GP_FAULT_IMAGE).iso $(DOUBLE_FAULT_IMAGE).iso $(RING3_IMAGE).iso

distclean:
	$(MAKE) -C kernel distclean
	rm -rf iso_root boot.log fault-boot.log divide-fault-boot.log page-fault-boot.log gp-fault-boot.log double-fault-boot.log $(IMAGE).iso $(FAULT_IMAGE).iso $(DIVIDE_FAULT_IMAGE).iso $(PAGE_FAULT_IMAGE).iso $(GP_FAULT_IMAGE).iso $(DOUBLE_FAULT_IMAGE).iso limine-binary limine-binary.tar.gz
