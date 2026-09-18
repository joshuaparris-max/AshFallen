.SUFFIXES:
.DELETE_ON_ERROR:

IMAGE := JoshOS-0.1-x86_64
LIMINE_VERSION := 12.9.0
LIMINE_SHA256 := 84059c93b4ea03994af6d614654c7095291388850ea7b258d64f9263abde5557
LIMINE_URL := https://github.com/Limine-Bootloader/Limine/releases/download/v$(LIMINE_VERSION)/limine-binary.tar.gz

.PHONY: all kernel host-tests run smoke clean distclean
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
	$(MAKE) -C kernel

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
	qemu-system-x86_64 -M q35 -m 512M -cdrom $(IMAGE).iso

smoke: $(IMAGE).iso
	rm -f boot.log
	-timeout 10s qemu-system-x86_64 -M q35 -m 256M -cdrom $(IMAGE).iso -display none -serial stdio -no-reboot > boot.log 2>&1
	grep -q JOSHOS_BOOT_OK boot.log
	@echo "Josh OS boot smoke test passed."

clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root boot.log $(IMAGE).iso

distclean:
	$(MAKE) -C kernel distclean
	rm -rf iso_root boot.log $(IMAGE).iso limine-binary limine-binary.tar.gz
