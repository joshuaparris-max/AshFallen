# Josh Full-Stack Programming Plan

> **This repository's role:** this is currently the canonical Josh OS integration/kernel repository. It primarily owns phases 2–23 plus the shared whole-stack contracts and convergence with JoshBIOS and JoshOS-Stage0.

## Goal

The long-term goal is intentionally broader than “write an operating system”.

> **Program every programmable layer we reasonably can, from a physical power-button press to an interactive Josh OS desktop, while explicitly documenting the hardware/silicon layers we do not own.**

The target is not to pretend that software can replace electronics. PSU voltage generation, VRMs, analog reset supervisors, DDR signalling and other physical circuitry are hardware. On commercial machines, some CPU microcode, DRAM-training code and embedded-controller firmware may also remain vendor-controlled.

The project should therefore distinguish:

1. **Josh-owned software** — code we write and test.
2. **Open foundations we deliberately build on** — for example coreboot while board bring-up matures.
3. **Documented silicon/vendor dependencies** — unavoidable code/firmware that is named rather than hidden.
4. **Electronics** — physical behaviour that is not software at all.

The ambition is to push the Josh-owned boundary downward over time without compromising recoverability or pretending unsupported hardware is supported.

---

## Whole-stack ownership map

| Phase | Layer to build | Canonical home | First concrete milestone | Done enough to move on |
|---:|---|---|---|---|
| 0 | Whole-stack contracts | JoshBIOS + canonical Josh OS | Freeze names, responsibilities and Josh Boot Protocol v0 | Every layer knows exactly what it receives and hands to the next |
| 1 | JoshBootloader → real Josh kernel | JoshBIOS / `boot/` | FAT32 + ELF64 + x86-64 long mode | JoshBootloader boots the canonical Josh kernel without Limine in QEMU |
| 2 | Exceptions and fault handling | Josh kernel | GDT, TSS, IDT, page fault, GP fault, double fault | Deliberate faults produce useful serial + graphical panic output |
| 3 | Physical memory | Josh kernel | Page-frame allocator based on firmware memory map | Allocate/free thousands of pages and detect invalid frees |
| 4 | Virtual memory | Josh kernel | Josh-owned page tables | Kernel no longer relies indefinitely on bootloader-created mappings |
| 5 | Kernel heap | Josh kernel | Small-object allocator | Kernel modules can allocate dynamic structures reliably |
| 6 | Interrupts and time | Josh kernel | APIC/timer + interrupt-driven keyboard | No keyboard polling; stable monotonic clock exists |
| 7 | CPU/SMP | Josh kernel | CPUID + multiple-core startup | QEMU with several CPUs boots and executes work on them |
| 8 | Device model | Josh kernel | PCI/PCIe enumeration + driver matching | Josh OS lists QEMU devices using its own discovery code |
| 9 | Input | Josh kernel | PS/2 mouse → unified input events | Real pointer moves a cursor; keyboard/mouse share a clean event API |
| 10 | Processes | Josh kernel | Context switching + scheduler | Two independent tasks demonstrably execute concurrently |
| 11 | User/kernel boundary | Josh kernel | Ring 3 + syscall ABI | First userspace program prints through a syscall |
| 12 | Init/services | Josh userspace | Josh init + logging/service supervisor | Kernel launches Josh init; init starts multiple real services |
| 13 | Storage | Kernel + userspace | QEMU block driver + partitions + filesystem | Create/read/edit a file and retain it after reboot |
| 14 | Files/security model | Userspace + kernel | Handles + capabilities/permissions | Separate apps cannot access resources they were not granted |
| 15 | Native graphics architecture | Kernel + userspace | Surface/buffer abstraction | Separate processes can submit surfaces to the desktop |
| 16 | Native compositor | Josh userspace | Move Josh Window System out of the browser prototype | Real Josh processes get movable/resizable/snappable windows |
| 17 | Native desktop | Josh userspace | Dock, launcher, settings, notifications | Stage 0 experience works without Chromium underneath |
| 18 | Networking | Kernel + service | NIC → Ethernet → ARP → IPv4 → DHCP → DNS → TCP | Josh OS obtains an IP and fetches data from another machine |
| 19 | USB | Josh kernel | xHCI + enumeration + HID | USB keyboard and mouse work on modern hardware |
| 20 | Audio | Kernel + service | Virtual audio device first | Application plays PCM audio through a Josh audio service |
| 21 | Power management | Kernel + services | ACPI parsing + shutdown/reboot | Proper ACPI shutdown/reboot; suspend/resume follows later |
| 22 | Installer | Josh product | Disk installation | Boot live USB → install → remove USB → Josh OS boots itself |
| 23 | Updates/recovery | Whole stack | Previous-known-good boot | Deliberately break current build and automatically recover |
| 24 | UEFI JoshBootloader | JoshBIOS | `.efi` loader + GOP + memory map + `ExitBootServices()` | Josh's own UEFI loader boots Josh kernel without Limine |
| 25 | Secure/measured boot | JoshBIOS + loader | Image signatures + TPM measurements | Tampered kernel is rejected and valid build boots |
| 26 | JoshFirmware in emulation | JoshBIOS / `firmware/` | coreboot/QEMU payload | QEMU starts in our firmware environment and reaches JoshBootloader |
| 27 | CPU reset / early firmware | JoshFirmware | Earliest serial marker possible | From virtual CPU reset we can trace execution through our firmware stack |
| 28 | Firmware hardware discovery | JoshFirmware | PCI/storage/USB/display inventory | Firmware independently discovers everything needed to boot |
| 29 | ACPI/SMBIOS generation | JoshFirmware | Josh-created platform tables | Josh kernel successfully consumes tables produced by JoshFirmware |
| 30 | Firmware POST/diagnostics | JoshBIOS | Real stage diagnostics | Fail simulated hardware init and get an intelligible diagnostic screen |
| 31 | Firmware setup UI | JoshBIOS | System / Boot / Security / Recovery | Settings persist and alter subsequent boots |
| 32 | Firmware updater | JoshBIOS | Signed A/B or recoverable update path | Interrupted/bad update cannot brick the VM/target |
| 33 | First physical firmware target | JoshFirmware | One exact coreboot-supported machine | Repeatable cold boot → JoshBootloader → Josh kernel |
| 34 | DRAM/platform ownership | JoshFirmware | Board-specific RAM/platform init | Cold boots repeatedly with correct RAM and no vendor BIOS |
| 35 | Embedded controller | Future JoshEC area/repository | Open/custom EC firmware | Josh code handles power button, battery/charger/events on chosen hardware |
| 36 | Power-button policy | JoshEC / firmware | Button → controlled power/start event | Trace a physical power-button press into our own firmware policy |
| 37 | Custom hardware — optional endgame | Separate hardware project | Open motherboard/controller | Almost every programmable element from button to desktop is ours |

---

# Programme order

The numbering above describes the whole ownership map. It does **not** mean we should attack the physically lowest layer first.

The efficient path is to make the real OS coherent first, then descend underneath it.

## Track A — complete the real OS path first

The immediate engineering chain is:

```text
Existing BIOS / UEFI
        ↓
   JoshBootloader
        ↓
    FAT32 reader
        ↓
    ELF64 loader
        ↓
  x86-64 long mode
        ↓
 Josh Boot Protocol
        ↓
     Josh kernel
        ↓
exceptions + memory + interrupts
        ↓
processes + syscalls
        ↓
    Josh userspace
        ↓
   Josh compositor
        ↓
    Josh desktop
```

This is the highest-value sequence because both ends already exist:

- JoshBootloader already has a working Stage 1/Stage 2 legacy path and QEMU proof.
- The canonical Josh x86-64 kernel already boots through Limine and renders its own graphical shell.

The first major integration goal is therefore:

> **JoshBootloader boots the canonical Josh kernel without Limine.**

That requires FAT32/GPT as needed, ELF64 validation/loading, long mode, memory-map/framebuffer/ACPI hand-off and the versioned Josh Boot Protocol.

---

## Track B — then descend into firmware

Once the Josh-owned bootloader/kernel/userspace chain is coherent:

```text
JoshFirmware
     ↓
CPU / chipset
     ↓
DRAM
     ↓
PCI / storage / display
     ↓
ACPI / SMBIOS
     ↓
JoshBIOS
     ↓
JoshBootloader
     ↓
Josh kernel
```

Start this in emulation and on explicit supported firmware foundations.

Do **not** attempt “generic PC firmware”. Firmware is motherboard-specific.

The progression should be:

1. QEMU/coreboot development target.
2. Earliest possible serial output.
3. Platform discovery and tables.
4. Josh-facing firmware UX.
5. Recovery/update mechanisms.
6. One exact recoverable physical development machine.
7. Only then deeper board-specific ownership such as DRAM/platform init.

---

## Track C — embedded controller and the physical power-button path

If the goal eventually includes Josh code before the host CPU starts executing normal firmware, create a dedicated embedded-controller programme.

Possible structure:

```text
JoshEC/
├── src/
│   ├── power.c
│   ├── buttons.c
│   ├── battery.c
│   ├── thermal.c
│   └── host_interface.c
├── boards/
└── docs/
```

A controlled development target could use an open MCU such as an STM32- or RP2040-class controller before any attempt to replace a proprietary laptop EC.

The conceptual flow is:

```text
physical button
      ↓
debounce
      ↓
power policy
      ↓
power/reset control signals
      ↓
host platform starts
      ↓
JoshFirmware
```

This is where “program from the button onward” becomes an embedded-systems project as well as an operating-system project.

### Important boundary

Software can control power-state signals.

Software does **not** create the electrical rails themselves.

Generating and regulating +12 V, +5 V, +3.3 V, CPU core voltages, DDR signalling and similar behaviour is power electronics / board design. Owning those layers would mean a custom hardware programme, not another C module.

---

# Phase details

## Phase 0 — contracts

Before more implementation divergence:

- finalise JoshFirmware / JoshBIOS / JoshBootloader / Josh Boot Protocol / Josh kernel naming;
- define required versus optional boot fields;
- version every cross-layer structure;
- define error/failure destinations;
- define build IDs carried across firmware → loader → kernel → userspace;
- build conformance fixtures.

**Proof:** the same kernel-side boot-context tests can validate both Limine-derived and JoshBootloader-derived information.

## Phase 1 — JoshBootloader boots the real kernel

Implement:

- block-read abstraction;
- partition discovery;
- FAT32;
- ELF64 parser with malformed-image tests;
- PT_LOAD loading and BSS zeroing;
- x86-64 page-table bootstrap;
- long-mode transition;
- known stack/register contract;
- Josh Boot Protocol v0 builder;
- memory map;
- framebuffer;
- ACPI/SMBIOS pointers;
- serial diagnostics.

**Proof:** QEMU reaches the canonical kernel's normal boot-success marker without Limine.

## Phases 2–11 — turn the demonstrator into a kernel

In order:

1. exceptions and panic path;
2. physical allocator;
3. kernel-owned paging;
4. heap;
5. APIC/timer/interrupts;
6. CPU feature model and later SMP;
7. device discovery;
8. event-driven input;
9. scheduler/processes;
10. Ring 3 transition;
11. syscall ABI.

Do not add userspace until memory protection and failure reporting are reliable enough to make bugs understandable.

## Phases 12–17 — become an operating system

Build:

- init;
- service supervision;
- logs;
- sessions;
- storage/filesystem;
- application handles/capabilities;
- native surfaces;
- compositor;
- native desktop.

The Stage 0 browser shell is a behavioural prototype. Preserve its useful App/Window/Setting/Notification concepts while replacing DOM/Chromium implementation details.

**Proof:** the familiar Josh desktop runs using real Josh processes and surfaces, with no Chromium underneath.

## Phases 18–21 — become useful on hardware

Add:

- networking;
- USB/xHCI;
- audio;
- ACPI/power management.

Prefer QEMU-friendly first drivers, then build a named physical-hardware support matrix.

“No driver” is better than claiming a generic driver that only worked once.

## Phases 22–25 — become installable, recoverable and trustworthy

Implement:

- installer;
- update model;
- previous-known-good;
- rescue/recovery environment;
- Josh UEFI loader;
- Secure/verified boot;
- TPM measured boot where appropriate.

**Proof:** intentionally corrupt a new update and demonstrate automatic recovery to a known-good system.

## Phases 26–34 — own firmware

Start with QEMU/coreboot and progress cautiously toward a real board.

Firmware programme requirements include:

- CPU reset path;
- early execution environment;
- CPU/chipset bring-up;
- DRAM training/validation;
- PCI/PCIe;
- boot storage;
- USB input for setup;
- display;
- RTC;
- ACPI;
- SMBIOS;
- TPM/security information;
- firmware setup;
- POST/diagnostics;
- update/rollback.

### Physical firmware rule

Never flash experimental firmware merely because the code compiles.

Before a physical target is considered supported:

- exact model/revision documented;
- known-good firmware backed up;
- external recovery programmer tested;
- flash voltage/wiring documented;
- cold/warm boots repeated;
- RAM capacity verified;
- storage/input/display verified;
- ACPI/SMBIOS validated;
- recovery tested.

## Phases 35–37 — own the layers below normal PC firmware

These are separate disciplines:

### JoshEC

Embedded firmware for power-button events, battery/charger state, thermal events and host communication.

### Power policy

Control when the host powers on/off/resets, and expose meaningful state to firmware/OS.

### Custom hardware

Optional long-term work involving PCB/FPGA/power electronics if we want to control programmable hardware boundaries that commercial PCs keep closed.

This is not required for Josh OS to be a legitimate operating system. It is the optional “how far can we take ownership?” endgame.

---

# Ultimate milestone

Do not define the end goal merely as:

> Josh OS boots.

Define it as:

> **Josh Full Stack 1.0:** one documented machine can go from a physical power-button press to an interactive Josh OS desktop through an auditable chain in which every programmable layer is either Josh-written or an explicitly documented dependency.

Target chain:

```text
Power button
     ↓
JoshEC
     ↓
CPU reset
     ↓
JoshFirmware
     ↓
JoshBIOS
     ↓
JoshBootloader
     ↓
Josh Boot Protocol
     ↓
Josh kernel
     ↓
Josh init
     ↓
Josh services
     ↓
Josh compositor
     ↓
Josh desktop
     ↓
Josh applications
```

## Full-stack 1.0 evidence

A credible full-stack claim requires:

- exact hardware target named;
- every software/firmware dependency documented;
- source + reproducible builds for Josh-owned layers;
- automated virtual tests wherever possible;
- repeatable physical cold boots;
- failure diagnostics at each major boundary;
- bootloader/kernel/userspace version traceability;
- clean shutdown/reboot;
- recovery from a deliberately broken software update;
- recovery from a deliberately broken firmware update where the platform permits;
- security state visible rather than implied;
- no fake capability.

---

# Engineering principles

1. **Own a layer only when we can test it.**
2. **Emulation before physical risk.**
3. **One exact supported board beats “generic PC support”.**
4. **Failure paths are features.**
5. **No blank-screen failures when serial/diagnostic output is possible.**
6. **No fake UI for capabilities that do not exist yet.**
7. **Shared concepts survive implementation changes.**
8. **Keep mature reference paths until Josh replacements reach parity.**
9. **Metrics inform engineering; they do not override architectural clarity.**
10. **Every milestone needs visible proof.**

---

# Immediate next milestone

The next major engineering task is deliberately not embedded-controller firmware or DRAM training.

It is:

> **Make JoshBootloader load and enter the real canonical x86-64 Josh kernel.**

That joins two pieces that already work independently and produces the first longer Josh-owned chain:

```text
existing firmware
      ↓
JoshBootloader
      ↓
Josh Boot Protocol
      ↓
canonical Josh kernel
      ↓
Josh graphical shell
```

Once that works reliably, continue upward through exceptions, memory, interrupts, processes and userspace before descending further into firmware ownership.
