# Josh OS boot stack

Josh OS treats boot as a chain of explicit contracts rather than one mysterious blob.

## Current native Josh OS paths

The canonical kernel has two verified QEMU boot paths:

```text
reference path:
BIOS / UEFI
    ↓
Limine 12.9
    ↓
kernel/src/boot_limine.c
    ↓
Josh-owned boot_context_t
    ↓
Josh kernel

legacy-BIOS Josh-owned path:
SeaBIOS
    ↓
Josh Stage 1
    ↓
JoshBootloader Stage 2
    ↓
MBR → FAT32 → /BOOT/JOSH/KERNEL.ELF
    ↓
ELF64 load → x86-64 long mode
    ↓
Josh Boot Protocol v0
    ↓
kernel/src/boot_josh.c
    ↓
the same Josh kernel
```

Both reach the kernel's `JOSHOS_BOOT_OK` marker in QEMU. Limine remains the independent reference path while JoshBootloader gains UEFI parity and physical-hardware evidence.

## Why the new boot adapter matters

Previously, `main.c` and `gfx.c` knew about Limine structures directly. That made the bootloader part of the kernel's internal architecture.

Now only `boot_limine.c` knows about Limine. The rest of the kernel consumes a Josh-owned `boot_context_t`.

That creates the seam needed for the JoshBIOS project:

```text
future:
JoshFirmware
    ↓
JoshBootloader
    ↓
JoshBootInfo adapter
    ↓
the same boot_context_t
    ↓
the same Josh kernel
```

The goal is **not** to fork the kernel for each bootloader. Boot protocols are adapters into one kernel contract.

## Current JoshBootloader UEFI status

On 18 September 2026, the JoshBIOS repository gained a tested x86-64 UEFI entry scaffold. Its `BOOTX64.EFI` is built into a FAT removable-media image and exercised under QEMU/OVMF; the smoke test requires the `JOSHUEFI_ENTRY_OK` serial marker.

That UEFI path still does **not** load AshFallen. The integrated JoshBootloader path is currently legacy BIOS only; UEFI still needs GOP, UEFI memory-map capture, `ExitBootServices`, filesystem/kernel loading and the Josh Boot Protocol hand-off.

## From the power button to the desktop

A mature path needs all of these layers:

1. **Reset and hardware bring-up** — CPU/chipset/memory initialisation.
2. **Firmware** — board-specific hardware setup, ACPI/SMBIOS, device enumeration, recovery.
3. **Firmware start screen** — a short Josh-branded splash plus setup/recovery key hints.
4. **Boot manager** — select Josh OS, another OS, recovery or firmware setup.
5. **Bootloader** — load and validate the kernel/modules; construct the boot-information handoff.
6. **Kernel early boot** — CPU tables, interrupts, memory manager, timers and panic/serial path.
7. **Kernel services** — scheduler, processes, syscalls, drivers, storage and filesystems.
8. **Init/userspace** — start services in a deterministic dependency order.
9. **Session/login** — identity, encryption/unlock, accessibility and session recovery.
10. **Compositor/shell** — windows, input, dock, launcher, notifications and settings.
11. **Applications** — the user finally reaches a useful system.

## Near-term convergence contract

The experimental Josh Boot Protocol v0 path now provides enough information to boot the canonical kernel through legacy BIOS in QEMU. Before JoshBootloader can replace Limine as a comparably reliable general path, it still needs to harden and broaden:

- ACPI/SMBIOS integration assertions;
- boot-device identity beyond the current BIOS drive number;
- command line / boot mode;
- loaded modules/initrd;
- typed failure/recovery paths;
- UEFI parity;
- physical-hardware validation.

JoshOS already has `boot_josh.c` beside `boot_limine.c`, and CI proves the same kernel reaches `JOSHOS_BOOT_OK` through either the Limine reference path or the legacy-BIOS JoshBootloader path.
