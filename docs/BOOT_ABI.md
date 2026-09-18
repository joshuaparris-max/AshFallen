# Josh Boot Protocol roadmap

This document defines the direction for the future contract between JoshBootloader and the canonical Josh kernel.

It is a roadmap and design boundary, not yet a frozen binary ABI.

## Why this exists

Today the native kernel boots through Limine and reads Limine protocol structures directly.

JoshBootloader is being developed separately. The correct integration is not to copy the kernel into the bootloader repository. It is to define a small, versioned contract that lets either Limine-adapter code or JoshBootloader provide the same internal boot information.

## Design goals

- versioned;
- architecture-explicit;
- pointer-size-explicit;
- self-describing enough to reject incompatible hand-offs;
- stable across bootloader implementation changes;
- minimal;
- no filesystem or firmware implementation details leaking upward unnecessarily;
- optional fields identified by capability/flags;
- easy to log and validate.

## Proposed top-level shape

Illustrative only:

```c
struct josh_boot_info {
    uint64_t magic;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint32_t total_size;
    uint32_t flags;

    struct josh_memory_map memory;
    struct josh_framebuffer framebuffer;

    uint64_t rsdp_phys;
    uint64_t smbios_phys;
    uint64_t efi_system_table_phys;

    struct josh_boot_device boot_device;
    struct josh_string command_line;
    struct josh_module_list modules;

    uint8_t entropy[64];

    struct josh_boot_health boot_health;
    struct josh_security_state security;
};
```

The final ABI should use fixed-width integer fields and explicit lengths/physical addresses.

## Required data for first JoshBootloader → Josh kernel boot

### Required

- ABI magic/version;
- memory map;
- framebuffer;
- kernel image bounds;
- command line, even if empty;
- bootloader identity/version;
- entropy seed if the boot environment can provide one.

### Strongly preferred

- ACPI RSDP;
- SMBIOS;
- boot device identity;
- previous boot-health status.

### Optional initially

- EFI system table;
- modules/initrd;
- TPM/measured boot;
- Secure Boot state;
- firmware log pointer.

## Memory map contract

Each entry should specify:

- physical base;
- length;
- type;
- attributes/flags if needed.

Define types for at least:

- usable;
- reserved;
- ACPI reclaimable;
- ACPI NVS;
- bootloader reclaimable;
- kernel image;
- framebuffer/MMIO where useful.

The kernel must normalise overlaps and never assume the map is sorted.

## Framebuffer contract

Include:

- physical address;
- width;
- height;
- pitch;
- bits per pixel;
- red/green/blue mask sizes and shifts;
- memory model/format.

This matches the information the current graphics code actually needs.

## Boot health

The bootloader and OS should eventually exchange:

- selected entry ID;
- previous attempt count;
- previous failure stage;
- whether current entry is pending validation;
- previous-known-good entry ID.

The OS marks a boot healthy only after reaching a defined userspace checkpoint.

## Security state

Expose facts, not policy conclusions:

- UEFI Secure Boot enabled/disabled/unknown;
- TPM present/absent;
- measured boot performed/not performed;
- kernel image verification result;
- firmware verification status if known.

The kernel/userspace can then make policy decisions.

## Versioning

### Major

Increment when binary interpretation changes incompatibly.

### Minor

Increment for append-only optional fields/capabilities.

Rules:

- structures include sizes;
- new fields append where practical;
- consumers ignore unknown optional extensions;
- required capability bits cause explicit rejection if unsupported;
- never silently reinterpret a field.

## Transition plan

### Step 1

Create `kernel/src/boot/` abstraction around the current Limine inputs.

### Step 2

Convert Limine framebuffer/memory-map data into internal Josh structures.

### Step 3

Add host tests for validation and malformed structures.

### Step 4

Freeze Josh Boot Protocol v0 enough for an experimental JoshBootloader path.

### Step 5

Teach JoshBootloader to load the canonical ELF64 kernel and construct the structure.

### Step 6

Boot the same kernel in QEMU through:

- Limine BIOS;
- Limine UEFI;
- JoshBootloader legacy BIOS path;
- later JoshBootloader UEFI path.

### Step 7

Only after repeated success consider v1 stability guarantees.

## Non-goals

The boot protocol should not define:

- desktop/window APIs;
- filesystem APIs;
- process APIs;
- networking;
- general device driver interfaces.

It ends once the kernel has enough trustworthy platform information to take ownership.
