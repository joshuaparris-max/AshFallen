# Difficulty, Scope, and Feasibility Calibration

> **Repo focus:** this is the main calibration for the canonical kernel/userspace/product effort. The important additions are explicit toolchain/ABI, SMP, teardown, debug infrastructure and realistic boundaries around USB, ACPI, GPU, Wi-Fi and browser/video support.

## Purpose

This document is the reality check that sits beside the full-stack roadmap.

The difficulty of Josh OS is **not evenly distributed**. Some layers are a long but well-documented engineering grind. Some are effectively independent research-sized projects. Some are blocked mainly by proprietary firmware, documentation or hardware access rather than by programming ability.

This calibration is deliberately candid so the roadmap does not confuse “listed” with “equally achievable”.

Time estimates below are rough order-of-magnitude calibration only, not commitments.

## What “done” means in this document

A label such as **done** means **done at the current milestone scope**, not “finished forever”.

For this project, a subsystem should only be called done when:

1. real implementation code exists;
2. it executes on the intended current target;
3. there is observable evidence that it works;
4. the current scope and limitations are documented;
5. later roadmap work may still replace or expand it.

For the items already labelled done:

- **Desktop shell + apps — milestone done:** the Stage 0 prototype has real movable/resizable/minimisable/maximisable windows, snapping, dock/menu bar, themes and working prototype apps. It is **not** yet the native Josh userspace/compositor.
- **Serial/VGA diagnostics + boot menus — milestone done:** JoshBIOS has direct early VGA text, COM1 serial output, a timed Stage 2 boot menu, diagnostics and reboot. Full recovery, previous-known-good, UEFI and richer firmware diagnostics remain future work.
- **Stage 1 MBR + INT 13h disk load — milestone done:** the 512-byte boot sector is real, ends in the 0xAA55 signature, verifies BIOS EDD support, uses INT 13h extensions (AH=42h) for LBA loading, loads Stage 2 and transfers control. It does not yet understand partitions or filesystems.
- **Framebuffer graphics + font rendering — milestone done:** the canonical Josh kernel receives a framebuffer, validates it, writes pixels directly, draws rectangles/panels/text and uses its own built-in bitmap font. This is not GPU acceleration or a compositor.
- **A20 / GDT / protected mode — milestone done:** JoshBootloader enables A20, installs its own GDT, sets CR0.PE and successfully transfers into 32-bit protected-mode code. This is not yet x86-64 long mode.

So “done” should be read as:

> **We have crossed this milestone with working code and evidence; we have not exhausted the subject.**

---


# Three difficulty bands

## 1. Tractable — long grind, known path

These are difficult but well-trodden:

- FAT32 boot reading;
- ELF64 validation/loading;
- x86-64 long-mode transition;
- boot protocol hand-off;
- GDT/IDT/TSS and exception handling;
- physical page allocation;
- paging and virtual address spaces;
- kernel heap;
- APIC/timer basics;
- context switching and a scheduler;
- syscall ABI;
- simple VFS;
- one straightforward disk driver under QEMU;
- a basic native compositor.

There is substantial public documentation and emulator support for all of these.

QEMU gives a tight feedback loop:

```text
change code
   ↓
build
   ↓
boot
   ↓
observe serial / graphical / debugger state
   ↓
fix
```

A rough calibration for a consistent evenings/weekends effort is that moving from the current Josh kernel to a multitasking kernel running its own protected userspace programs from persistent storage is plausibly a **many-month to multi-year** project rather than an impossible one.

The exact duration will depend much more on scope discipline and debugging time than on line count.

---

## 2. Brutal — each is effectively its own project

These are not “just another driver”.

### USB / xHCI

Requires:

- PCI discovery;
- controller initialisation;
- rings and descriptors;
- DMA;
- device enumeration;
- hubs;
- transfer types;
- hotplug;
- HID;
- mass storage;
- robust error handling.

This can consume months by itself.

### TCP/IP and real network hardware

The protocol stack can sensibly be ported from an existing implementation such as lwIP rather than rewritten for purity.

The hardware problem remains: Josh OS still needs working NIC drivers, DMA, interrupts and buffer ownership.

### ACPI AML

Reading fixed ACPI tables is manageable.

Executing AML is a different scale of problem. A mature interpreter such as uACPI is a much better engineering choice than writing a bespoke interpreter unless AML itself becomes a research goal.

### Suspend / resume

Suspend touches:

- devices;
- interrupts;
- CPUs;
- ACPI;
- memory;
- storage;
- graphics;
- timers;
- firmware state.

It is a whole-system state transition and remains difficult even in mature operating systems.

### Modern accelerated GPU support

A linear framebuffer is realistic.

A high-performance modern Intel/AMD/NVIDIA graphics stack involves huge amounts of hardware-specific code, firmware interaction, memory management, command submission, display engines and user-space graphics APIs.

A solo Josh project should not assume arbitrary modern GPU acceleration is a normal roadmap item.

### Wi-Fi

Modern Wi-Fi drivers combine:

- PCIe/USB transport;
- firmware loading;
- radio/device-specific state;
- regulatory behaviour;
- authentication/encryption integration;
- power management.

For a solo project, porting/supporting a very specific well-documented adapter is a more realistic target than “Josh Wi-Fi support” generically.

---

## 3. Blocked more by access than skill

Some early firmware layers are constrained by proprietary interfaces.

### DRAM training

On many modern x86 systems, DRAM training and silicon initialisation depend on vendor code such as Intel FSP or AMD AGESA-family firmware components.

The hard boundary is often not understanding what RAM training means; it is not having complete public silicon documentation or replaceable open code.

### Embedded controllers

Laptop EC firmware is frequently:

- board-specific;
- vendor-specific;
- poorly documented publicly;
- protected by NDA or unavailable datasheets.

A controlled/open EC target is much more realistic than replacing an arbitrary laptop EC.

### PMIC and board power sequencing

Power-management ICs and board-specific sequencing can also depend on unavailable documentation.

That is why the firmware roadmap should target:

- a known coreboot-friendly machine;
- an open SBC;
- RISC-V/ARM development hardware;
- or eventually a deliberately chosen/custom controller platform.

The phrase “blocked” here does **not** mean permanently impossible. It means the obstacle is access to hardware knowledge or replaceable firmware, not merely writing more code.

---

# Missing architectural programmes that must be explicit

The full-stack roadmap already covers most territory, but these deserve their own named programmes rather than being treated as implementation details.

## Toolchain and userspace ABI

Josh OS eventually needs an answer to:

> How do I compile a normal program for Josh OS?

That implies:

- target triple;
- ABI;
- calling convention;
- object format;
- linker scripts/runtime;
- compiler support or cross-toolchain;
- C runtime startup;
- libc strategy;
- headers;
- package/build integration;
- eventually dynamic linking if desired.

A libc port such as newlib or musl may be preferable to writing a large C library from scratch.

This does **not** block early kernel work, but it becomes a major gate once Josh OS wants a useful portable userspace and application ecosystem.

## SMP

APIC/timer support is not the same thing as multiprocessor support.

SMP requires:

- AP discovery;
- INIT-SIPI-SIPI startup on x86;
- per-CPU state;
- per-CPU stacks;
- interrupt routing;
- atomic primitives;
- spinlocks/other synchronization;
- memory ordering rules;
- scheduler changes;
- allocator changes;
- clear ownership rules for shared kernel structures.

Single-core first is sensible, but data structures should not make SMP impossible to add cleanly.

## Teardown and reverse lifecycle

The project cannot only model boot.

It needs the reverse direction:

```text
applications
   ↓
services stop
   ↓
files flush
   ↓
filesystems unmount
   ↓
devices quiesce
   ↓
ACPI / firmware transition
   ↓
power off / reboot / suspend
```

Shutdown, restart and eventually suspend/resume are architectural paths, not UI buttons.

## Debug infrastructure

Build this early.

Required tools should include:

- structured serial logs;
- graphical panic path;
- register dump;
- exception vector information;
- symbolized addresses;
- stack-unwinding groundwork;
- map/symbol files;
- QEMU GDB stub workflow;
- deterministic debug builds;
- crash markers in CI;
- optional core/crash dump strategy later.

Debugging infrastructure often pays back more engineering time than an early feature.

---

# Smaller but important gaps

## SMM

System Management Mode can execute below/alongside the OS at a privilege level the normal kernel does not control.

For firmware ownership, document:

- whether the platform uses SMM;
- what firmware/SMM code remains vendor-provided;
- what JoshFirmware can replace;
- security boundaries;
- SMI sources;
- communication with the OS, if any.

## IOMMU and MSI/MSI-X

Modern high-performance device support eventually needs:

- DMA isolation;
- IOMMU;
- MSI;
- MSI-X;
- interrupt affinity;
- robust DMA mapping APIs.

These matter for security as well as performance.

## Timekeeping

Separate concepts:

- monotonic clock;
- wall-clock time;
- RTC;
- timer interrupts;
- TSC/APIC/HPET choices;
- timezone/user presentation;
- NTP/network synchronization.

A scheduler timer is not automatically a complete time subsystem.

## Keyboard/input text model

PS/2 scancodes are only the hardware edge.

The full path is more like:

```text
scan code
   ↓
physical key
   ↓
modifier state
   ↓
keyboard layout
   ↓
keysym/action
   ↓
Unicode/text input
   ↓
application
```

Input-method and international text concerns come later.

## Memory protection and exploit mitigation

Eventually plan for:

- NX;
- W^X;
- SMEP;
- SMAP;
- stack guards;
- guard pages;
- user/kernel isolation;
- entropy;
- ASLR/KASLR where worthwhile;
- hardened parser boundaries.

## Installer and partitioning

A real installer eventually needs:

- storage discovery;
- partition table handling;
- filesystem creation;
- install layout;
- boot-entry creation;
- upgrade/reinstall behaviour;
- data-preservation policy;
- recovery installation.

---

# AI-assisted / “vibe-coding” calibration

This ranking is about how well the loop

> describe it → generate/modify code → run it → see it fail → provide the evidence → iterate

works.

It is **not** a claim that AI can replace understanding, testing or hardware documentation.

## Tier 1 — genuinely AI-assistable

Tight QEMU loop, broad public documentation and loud failures.

1. Desktop shell + prototype apps — already substantially done.
2. Serial/VGA diagnostics and boot menus — already started.
3. Stage 1 MBR + BIOS disk loading — already working.
4. Framebuffer graphics + font rendering — already working.
5. A20 / GDT / protected-mode path — already working in JoshBootloader.
6. FAT32 read-only support.
7. ELF64 parse/load.
8. Long-mode switch + Josh Boot Protocol.
9. IDT and exception handlers.
10. Bitmap physical allocator + initial heap.
11. PIT/HPET/APIC timer groundwork.
12. Proper PS/2 keyboard handling: scancode sets, modifiers and event model.

These still require verification. AI is useful because the failures are generally observable and reference implementations/specifications exist.

---

## Tier 2 — AI-assistable, but debugging dominates

Bugs become silent, delayed or non-local.

13. Paging and process address spaces.
14. Context switching and scheduler.
15. SYSCALL/SYSRET ABI and MSR setup.
16. VFS plus a deliberately simple filesystem such as ext2.
17. AHCI or NVMe.
18. ACPI shutdown/reboot using fixed tables where possible.
19. A well-documented virtual NIC such as e1000 in QEMU.

At this tier, a plausible-looking implementation can be subtly wrong.

The correct workflow becomes:

```text
LLM suggestion
   ↓
spec/manual check
   ↓
small patch
   ↓
serial/GDB/QEMU evidence
   ↓
invariant/test validation
```

not “generate a subsystem and trust it”.

---

## Tier 3 — real standalone projects

20. Cross-toolchain + libc integration.
21. SMP bring-up and synchronization model.
22. TCP/IP integration/port.
23. Native compositor + native input integration.
24. ACPI AML via a mature interpreter port.
25. HD Audio.
26. Broad physical-hardware bring-up beyond QEMU.

At this stage, architecture and debugging expertise matter more than code-generation speed.

---

## Tier 4 — the wall for a solo general-purpose OS

27. Broad USB/xHCI compatibility.
28. TLS implementation — **port a mature library; do not design custom cryptography**.
29. Full fonts/shaping/Unicode stack — port mature components such as FreeType/HarfBuzz where appropriate.
30. Major browser-engine port.
31. Modern video decode stack and hardware/SIMD optimization.
32. Modern accelerated GPU stack.
33. Broad Wi-Fi hardware support.

These are not impossible in principle.

They are simply large enough that “Josh OS supports this generally” should not be assumed as a solo milestone.

For many of these, the right engineering decision is to port mature open-source components rather than reproduce decades of specialist work.

---

# Where AI assistance stops being strong

AI assistance becomes much less reliable when:

- failures are not observable;
- the hardware silently wedges;
- documentation is thin or proprietary;
- behaviour depends on board errata;
- an answer can be locally plausible but globally wrong;
- correctness depends on exact ordering hundreds of instructions earlier;
- the problem requires unavailable vendor documentation.

Examples include:

- xHCI race/debug problems;
- AML/platform-specific behaviour;
- firmware/board bring-up;
- obscure PCIe devices;
- GPU command submission;
- laptop embedded controllers.

The response to this is not “never attempt it”.

It is to improve observability, shrink the experiment, use specifications/reference code and choose hardware with good documentation.

---

# Calibration of the overall goal

A complete modern general-purpose operating system comparable in hardware breadth to Windows, macOS or Linux is not a realistic solo completion target.

That does **not** make Josh OS unrealistic.

The useful distinction is between:

## A real Josh operating system

A Josh-owned kernel that can:

- boot;
- handle faults;
- manage memory;
- schedule processes;
- run protected userspace;
- read/write persistent storage;
- launch services;
- display a native desktop;
- accept keyboard/mouse input;
- network on at least one supported device;
- shut down cleanly;
- install/recover on named supported environments.

That is an enormous but coherent project.

## Arbitrary-PC parity

Supporting:

- arbitrary USB hardware;
- arbitrary GPUs;
- arbitrary Wi-Fi;
- arbitrary laptops;
- modern browsers;
- accelerated video;
- suspend/resume everywhere;
- every ACPI quirk;

is a fundamentally different scope.

Projects such as SerenityOS, Haiku and Redox illustrate how much engineering remains even after years of serious multi-contributor development.

The lesson is not “don't build Josh OS”.

It is:

> **Define exactly what “Josh OS is finished” means.**

---

# Recommended completion definitions

## Josh Kernel 0.1 — already alive

Boots, produces observable output and owns its execution after boot hand-off.

## Josh Kernel 0.5 — real kernel

- exceptions;
- allocator;
- paging;
- interrupts/time;
- processes;
- syscalls;
- basic storage;
- first userspace init.

## Josh OS Native 1.0 — real operating system

On its supported VM/hardware target:

- JoshBootloader boots it;
- protected userspace works;
- persistent files work;
- native compositor works;
- keyboard/mouse work;
- networking works on at least one documented adapter;
- installer/recovery work;
- clean restart/shutdown work;
- automated QEMU tests exist.

This is already a legitimate standalone operating system.

## Josh Full Stack 1.0 — deep ownership

On **one explicitly documented machine**:

```text
power button
   ↓
Josh-controlled/open firmware path
   ↓
JoshBIOS
   ↓
JoshBootloader
   ↓
Josh kernel
   ↓
Josh userspace/services
   ↓
Josh compositor
   ↓
Josh desktop
```

Every non-Josh programmable dependency is explicitly documented.

## Josh Universal — not a required finish line

“Runs everything on arbitrary PCs and does everything Windows/Linux/macOS do” should not be the definition of success.

That goal would turn every hardware ecosystem problem into a blocker for declaring the OS real.

---

# Immediate priority after this calibration

The next work should remain:

```text
JoshBootloader
    ↓
FAT32
    ↓
ELF64
    ↓
x86-64 long mode
    ↓
Josh Boot Protocol
    ↓
canonical Josh kernel
```

Then immediately invest in:

1. exception/panic diagnostics;
2. QEMU + GDB workflow;
3. physical memory allocation;
4. paging;
5. interrupts/time;
6. process/syscall architecture;
7. toolchain/userspace ABI design.

That sequence maximizes the amount of the stack that remains observable and therefore learnable.
