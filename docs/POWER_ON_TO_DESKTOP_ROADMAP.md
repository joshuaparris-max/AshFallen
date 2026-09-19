# Josh OS: power-on to desktop roadmap

This document is the whole-stack roadmap for the machine lifecycle from the instant the power button is pressed to a usable Josh OS desktop.

It is intentionally broader than the kernel roadmap. The goal is to make every layer explicit so Josh OS does not become a collection of impressive pieces with gaps between them.

## Canonical stack and ownership

| Layer | Canonical home | Responsibility |
|---|---|---|
| Board-specific hardware bring-up | JoshFirmware work in `Parris-Tech-Services/JoshBIOS` | CPU/chipset/DRAM/platform initialisation through a supported firmware foundation |
| Firmware UX and policy | JoshBIOS work in `Parris-Tech-Services/JoshBIOS` | splash/setup, hardware inventory, boot order, recovery entry, firmware settings |
| Kernel loading | JoshBootloader work in `Parris-Tech-Services/JoshBIOS` | discover bootable Josh systems, select an entry, load ELF64, construct boot information, enter x86-64 kernel |
| Native kernel | `kernel/` in this repository | memory, interrupts, processes, devices, filesystems, networking, security primitives |
| System services/userspace | this repository | init/service model, session/device/file/settings services, app APIs |
| Desktop/product | this repository | compositor, shell, launcher, settings, apps, first-run and recovery UX |
| Stage-0 compatibility extraction | `Parris-Tech-Services/JoshOS-Stage0` | fast product prototyping and ArchISO compatibility image |

The small kernel in the JoshBIOS repository is only a boot-stack payload. **The canonical native Josh kernel is `JoshOS/kernel`.**

## One boot story

The target long-term native path is:

```text
Power button
   ↓
power rails / reset / CPU reset vector
   ↓
JoshFirmware-supported platform bring-up
   ↓
CPU + chipset + DRAM + essential buses
   ↓
firmware security / measurements / platform tables
   ↓
JoshBIOS firmware UX and boot policy
   ↓
JoshBootloader
   ↓
Josh Boot Protocol
   ↓
Josh kernel
   ↓
memory + interrupts + scheduler + devices
   ↓
Josh init / services
   ↓
Josh compositor + desktop shell
   ↓
session / first-run / login
   ↓
usable Josh OS desktop
```

The practical product path can continue using Linux underneath while the native path matures. Both tracks should share product concepts and visual language.

---

# Phase 0 — Define the contracts before adding more layers

Before replacing more infrastructure, define the seams.

## 0.1 Naming and responsibility

Use these meanings consistently:

- **JoshFirmware** — low-level platform initialisation and firmware foundation.
- **JoshBIOS** — the Josh-branded firmware environment, setup UI and boot policy.
- **JoshBootloader** — kernel/system selection and kernel hand-off.
- **Josh Boot Protocol** — versioned data contract between bootloader and kernel.
- **Josh kernel** — the independent x86-64 kernel in this repository.
- **Josh OS** — the complete product above the kernel.

Avoid using “BIOS” to mean all five layers.

## 0.2 Versioned Josh Boot Protocol

Define a stable hand-off structure containing at least:

- protocol magic and version;
- bootloader identity/version;
- physical memory map;
- framebuffer address, dimensions, pitch and pixel masks;
- ACPI RSDP pointer when available;
- SMBIOS pointer when available;
- EFI system table pointer when booted through UEFI;
- boot device identity;
- kernel command line;
- loaded modules/initrd list;
- random seed/entropy material;
- firmware security state;
- measured-boot information when available;
- previous-boot status / recovery reason;
- architecture and CPU feature flags needed at entry.

The kernel must validate version, sizes and required fields before trusting the structure.

## 0.3 Failure model

Every layer needs a defined failure destination:

- firmware failure → firmware diagnostic screen / recovery;
- boot selection failure → boot menu;
- kernel load failure → bootloader error and alternate entry;
- early kernel panic → serial + framebuffer panic screen;
- userspace failure → rescue target;
- desktop/session failure → safe shell or recovery desktop.

A blank screen is never an acceptable final failure state.

---

# Phase 1 — Power button to firmware-ready platform

This belongs primarily to JoshFirmware/JoshBIOS.

## Hardware bring-up

For each explicitly supported board:

- CPU reset entry and early execution environment;
- CPU microcode policy;
- chipset/SoC initialisation;
- DRAM discovery/training;
- cache/MTRR/PAT setup as required;
- timers needed for firmware;
- LPC/SPI/eSPI where applicable;
- PCI/PCIe enumeration sufficient for boot;
- storage controller visibility;
- USB keyboard support where practical;
- display/console path;
- RTC;
- ACPI table generation/hand-off;
- SMBIOS information;
- firmware variable/settings storage.

Do not pursue “generic PC firmware”. Support should be board-by-board with a compatibility matrix and recovery path.

## Security foundation

Plan for:

- reproducible firmware builds;
- signed release artefacts;
- rollback protection policy;
- TPM detection and optional measured boot;
- Secure Boot-compatible path for UEFI systems;
- separation of development keys from release keys;
- recovery image verification;
- firmware update interruption safety.

Security features should be staged, but the data structures and update model should not make them impossible later.

---

# Phase 2 — JoshBIOS start screen and setup

Yes: Josh OS should have a **firmware start screen**.

It should appear only after enough hardware is initialised to render reliably.

## Normal start screen

Target behaviour:

- Josh logo / firmware identity;
- small build/version text;
- concise status such as “Starting Josh OS…”;
- hidden-by-default diagnostic detail;
- visible key hints only when useful:
  - Setup
  - Boot menu
  - Recovery / diagnostics

The normal path should be fast and calm. Avoid a long animated splash that hides genuine stalls.

## Diagnostic mode

A key or previous failed boot can enable:

- firmware build ID;
- CPU and memory summary;
- detected boot devices;
- firmware stage timings;
- last failure reason;
- serial log status;
- board identifier.

## Firmware setup

Initial sections:

1. **System**
   - board/CPU/memory information
   - firmware version
   - date/time

2. **Boot**
   - boot order
   - one-time boot
   - timeout
   - default Josh OS entry

3. **Security**
   - Secure Boot state
   - TPM state
   - firmware verification state

4. **Devices**
   - storage/network/basic hardware inventory
   - enable/disable only where safe and meaningful

5. **Recovery**
   - previous-good firmware
   - reset settings
   - diagnostics
   - recovery media

Keep settings deliberately small. Firmware setup should not become a second operating system.

---

# Phase 3 — Boot menu and JoshBootloader

Yes: Josh OS should have a **boot menu**, but it is conceptually separate from firmware setup.

## Normal behaviour

- boot the default entry immediately or after a very short configurable timeout;
- reveal the menu when a key is held/pressed;
- automatically reveal it after failed boots;
- remember the selected default without hiding alternate/recovery entries.

## Entries

The bootloader should eventually support:

- Josh OS current;
- Josh OS previous-known-good;
- Josh OS recovery;
- Josh diagnostics;
- alternate Josh kernel development build;
- chainload another OS where supported;
- removable-media boot entry when policy allows.

## Bootloader responsibilities

- locate boot filesystem/partition robustly;
- read FAT32 first, then a deliberately chosen broader filesystem story;
- parse ELF64 program headers;
- validate architecture and image bounds;
- load kernel and optional initrd/modules;
- obtain firmware memory map;
- obtain framebuffer details;
- collect ACPI/SMBIOS/UEFI pointers;
- gather entropy;
- build the Josh Boot Protocol structure;
- transition into required x86-64 state;
- jump to the canonical Josh kernel entry;
- never silently continue after validation failure.

The bootloader should remain small. Filesystems, networking, desktop UI and general hardware drivers belong elsewhere unless required specifically for boot/recovery.

---

# Phase 4 — Kernel foundations

See `docs/KERNEL_ROADMAP.md` for the detailed roadmap.

The ordering should be:

1. exceptions and panic reporting;
2. physical memory management;
3. virtual memory and kernel heap;
4. interrupt controller and timer;
5. event-driven input;
6. device discovery and driver model;
7. processes/threads/scheduler;
8. syscall ABI and userspace;
9. VFS/storage;
10. networking;
11. power management;
12. broader hardware support.

Every milestone should remain bootable in QEMU and produce observable diagnostics.

---

# Phase 5 — Init, services and userspace

A kernel that can run programs is not yet an OS product.

Create explicit models for:

- init/service supervision;
- logging;
- device events;
- sessions;
- settings/configuration;
- files and mounts;
- network state;
- time;
- notifications;
- permissions/capabilities;
- package/application metadata;
- update state.

## Boot targets

At minimum:

- normal graphical target;
- text/rescue target;
- recovery target;
- diagnostics target.

A broken compositor should not make the machine unrecoverable.

---

# Phase 6 — Desktop startup and session

There should also be an **OS boot splash**, distinct from the firmware splash.

## OS splash

After the kernel has enough graphics support:

- use the same design language as the firmware splash;
- display a small number of meaningful progress states rather than fake percentages;
- provide a key to reveal boot logs;
- transition cleanly into the session.

Possible states:

- Starting kernel
- Starting devices
- Starting system services
- Starting desktop

These must reflect real milestones emitted by the system.

## Session / login

Decide deliberately whether the initial product:

- boots directly into a single-owner local session; or
- presents an account/login screen.

Do not build a decorative login screen before the account, credential and permissions model exists.

---

# Phase 7 — First boot / start experience

A **first-run start screen** belongs in Josh OS, not the firmware.

First boot should be short and resumable:

1. language/keyboard;
2. display/accessibility essentials;
3. network if needed;
4. account/device name if the account model exists;
5. privacy/update choices that genuinely exist;
6. concise “Welcome to Josh OS” hand-off.

Never ask questions whose answers the system cannot yet honour.

---

# Phase 8 — Recovery, rollback and updates

Recovery is part of the architecture, not an afterthought.

## Required recovery paths

- firmware previous-known-good;
- bootloader previous-known-good;
- kernel previous-known-good;
- OS snapshot/update rollback where the storage model permits;
- rescue shell;
- diagnostics;
- reinstall/repair media.

## Boot health

Record:

- boot attempt number;
- whether kernel hand-off succeeded;
- whether userspace reached healthy state;
- whether desktop/session reached healthy state.

Only mark a new build “good” after it reaches an explicit health checkpoint.

Repeated failure should automatically expose recovery choices.

## Updates

Eventually support atomic or transactional updates for:

- firmware;
- bootloader;
- kernel;
- base OS;
- applications.

The precise mechanism may differ per layer, but interrupted updates must not strand the machine.

---

# Phase 9 — Suspend, resume, reboot and shutdown

“Power button to desktop” has a reverse path too.

Design and test:

- clean shutdown;
- reboot;
- firmware reboot;
- soft power-off;
- suspend;
- resume;
- lid events on laptops;
- wake sources;
- crash/panic reboot policy.

Power-state bugs can corrupt storage, so this deserves first-class test coverage.

---

# Phase 10 — Observability

Every layer should produce useful evidence.

## Firmware

- stage timing;
- board ID;
- hardware init failures;
- serial log.

## Bootloader

- selected entry;
- image hashes;
- filesystem/device;
- memory-map summary;
- protocol version;
- hand-off address.

## Kernel

- early serial console;
- structured log levels;
- panic reason and register dump;
- exception vector;
- boot protocol dump in debug builds.

## Userspace

- service state;
- boot journal;
- crash reports;
- recovery-safe log viewer.

Debug output should be detailed without making the normal boot visually noisy.

---

# Phase 11 — Test matrix

Automate progressively:

## Virtual

- QEMU legacy BIOS;
- QEMU UEFI/OVMF;
- multiple RAM sizes;
- multiple framebuffer sizes;
- missing/invalid boot entry;
- corrupt kernel;
- recovery entry;
- warm reboot;
- cold boot simulation where practical.

## Physical

Maintain an explicit matrix for each tested machine:

- firmware mode;
- CPU;
- RAM;
- GPU;
- storage;
- keyboard/mouse;
- Ethernet/Wi-Fi;
- audio;
- suspend/resume;
- installer;
- update/rollback.

“Booted once” is not the same as supported.

---

# Near-term integration milestones

## Milestone A — define the seam

- [ ] Write Josh Boot Protocol v0 structure.
- [ ] Add protocol-version checking to the JoshOS kernel.
- [ ] Document required vs optional fields.
- [ ] Add protocol conformance fixtures/tests.

## Milestone B — make JoshBootloader load the real kernel

- [ ] ELF64 loader.
- [ ] x86-64 long-mode hand-off.
- [ ] framebuffer hand-off.
- [ ] memory-map hand-off.
- [ ] ACPI pointer hand-off.
- [ ] serial diagnostics.
- [ ] boot JoshOS kernel in QEMU without Limine.

Limine remains a supported/reference boot path until JoshBootloader reaches equivalent reliability.

## Milestone C — boot UX

- [ ] JoshBIOS firmware splash.
- [ ] setup/boot/recovery key handling.
- [ ] JoshBootloader hidden-by-default boot menu.
- [ ] previous-known-good entry.
- [ ] Josh OS real progress milestones.

## Milestone D — recoverability

- [ ] boot-health state.
- [ ] automatic recovery menu after repeated failures.
- [ ] kernel rescue target.
- [ ] documented firmware recovery procedure.
- [ ] update rollback story.

## Milestone E — first supported physical development machine

- [ ] choose exact board/system;
- [ ] external firmware recovery method;
- [ ] support matrix;
- [ ] reproduce cold boot repeatedly;
- [ ] validate reboot/shutdown;
- [ ] validate Ventoy/USB boot;
- [ ] only then consider firmware flashing experiments.

---

# Definition of “whole stack works”

A Josh OS stack milestone is complete when:

1. the machine can boot through the intended path repeatedly;
2. failures are understandable rather than blank;
3. there is a recovery path;
4. automated evidence covers the virtual path;
5. physical support claims name exact tested hardware;
6. the desktop remains coherent with the same App/Window/Setting/Capability concepts used by the product track;
7. no layer claims responsibility that actually belongs to another layer.

The goal is not merely to reach a desktop. It is to make the entire journey from power-on to desktop **coherent, observable, recoverable and increasingly Josh-owned**.
