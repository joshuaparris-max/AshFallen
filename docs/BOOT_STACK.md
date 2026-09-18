# Josh OS boot stack

Josh OS treats boot as a chain of explicit contracts rather than one mysterious blob.

## Current native Josh OS path

```text
Power button
    ↓
PC firmware (BIOS or UEFI)
    ↓
Limine 12.9 boot manager
    ↓
Limine protocol adapter (kernel/src/boot_limine.c)
    ↓
Josh-owned boot_context_t
    ↓
Josh kernel
    ↓
framebuffer desktop + shell
```

The Limine menu is intentionally visible for three seconds and branded **Josh OS Boot Manager**. There is only one native entry today because recovery/safe-mode entries should not be shown until they do something real.

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

AshFallen itself is unchanged at the boot boundary: **Limine remains the only integrated native-kernel boot path**. The JoshBIOS UEFI scaffold does not yet load this kernel, construct the full Josh Boot Protocol or call a Josh-specific kernel entry adapter.

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

The JoshBIOS repository is defining `JoshBootInfo`. Before replacing Limine for the native path it needs to provide, at minimum:

- x86-64 ELF loading;
- memory map;
- framebuffer description;
- ACPI/SMBIOS pointers;
- boot-device identity;
- command line / boot mode;
- loaded modules;
- version/feature fields.

Once that exists, AshFallen can add a `boot_josh.c` adapter beside `boot_limine.c` and run the same kernel through either loader while the transition is tested.
