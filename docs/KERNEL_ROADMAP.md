# Josh kernel roadmap

This is the detailed roadmap for the canonical native x86-64 Josh kernel in `kernel/`.

The kernel already boots through Limine, receives a framebuffer and memory map, renders a graphical shell, accepts basic PS/2 keyboard input, and emits a CI boot marker. The next work should turn that demonstrator into a real kernel without losing its current observability.

## Principles

1. Keep every major milestone bootable.
2. Prefer explicit subsystem boundaries over framework-like abstraction.
3. Serial diagnostics remain available even when graphics fail.
4. Validate all firmware/bootloader-provided data.
5. Add a subsystem only when the previous layer can be tested.
6. The kernel exposes mechanisms; policy belongs in userspace where practical.
7. Security boundaries are architectural, not a final polishing pass.

## First physical target

The first selected physical development machine is **DadLAN Laptop #10 / Compaq 610**. Current status is **Selected**, not supported. The exact machine inventory, boot-test sequence and firmware recovery gates are owned by [JoshBIOS's Compaq 610 hardware target document](https://github.com/Parris-Tech-Services/JoshBIOS/blob/main/docs/HARDWARE_TARGET_COMPAQ_610.md).

Use this machine to drive concrete early decisions, but keep hardware-specific quirks behind the appropriate platform or driver boundary. Do not fork the Josh kernel for this laptop, and do not infer generic PC support from success on this one target.

---

## K0 — Boot contract hardening

### Goals

- [x] Introduce a boot-adapter layer instead of reading Limine request structures throughout kernel code.
- [x] Define an internal Josh-owned boot-context representation.
- [x] Validate framebuffer geometry and address arithmetic.
- [x] Validate/sanitise memory-map entries.
- [ ] Preserve raw boot-protocol metadata for debug builds.
- [x] Add support for Josh Boot Protocol alongside Limine.
- [ ] Keep Limine as a reference boot path until the JoshBootloader path is equally reliable.

The Josh-owned `boot_context_t` now preserves framebuffer data, a bounded internal memory-map representation, usable-memory accounting and firmware-table pointers without leaking Limine structures into the rest of the kernel. Josh Boot Protocol entries are range-checked for overflow, zero-length entries are discarded, and unknown/reserved memory is kept non-usable by default.

AshFallen CI run `35345674776` passed the host protocol tests plus the Limine boot path at commit `2c65821`. JoshBIOS cross-repo run `35345724402` checked out that exact AshFallen commit and booted it through the JoshBootloader path to `JOSHOS_BOOT_OK`.

### Exit test

The same kernel source tree now boots through both supported adapters and reaches the same `JOSHOS_BOOT_OK` milestone in QEMU. Limine remains the reference path while JoshBootloader gains UEFI and physical-hardware parity.

---

## K1 — Exceptions before features

Implement:

- [x] GDT owned by the kernel;
- [x] TSS;
- [x] IDT;
- [x] exception stubs;
- [x] page-fault handler;
- [x] general-protection-fault handler;
- [x] double-fault strategy;
- [x] register dump;
- [ ] stack trace groundwork where feasible;
- [ ] graphical + serial panic path.

The kernel installs its own 64-bit GDT and TSS before loading the IDT. TSS IST1 points at a dedicated 16 KiB emergency stack and exception vector 8 selects that IST for double-fault entry. CI deliberately makes #GP delivery fail, forcing the CPU into vector 8, and requires the double-fault panic marker and zero error code. Josh-owned x86-64 assembly stubs now normalise all vectors into a canonical exception frame and preserve RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP and R8–R15 before entering C. The serial panic reports those registers alongside vector, error code, RIP, CS and RFLAGS; page faults also report CR2. CI seeds known RAX and R15 values immediately before an invalid opcode and requires the panic dump to recover both values exactly. Stack-trace groundwork and the graphical panic path are still outstanding.

### Test cases

Deliberately trigger:

- [x] invalid opcode;
- [x] double fault (forced #GP delivery failure);
- [x] divide-by-zero;
- [x] page fault;
- [x] general protection fault.

CI now deliberately exercises invalid opcode (#UD), divide error (#DE), page fault (#PF), general protection (#GP), and the dedicated double-fault/IST path. GitHub Actions run `35346395226` verified all five reach their expected serial panic markers; the page-fault test additionally verifies `CR2=0x00007FFFFFFFF000`, and the invalid-opcode test verifies seeded values at opposite ends of the saved general-purpose register frame.

---

## K2 — Physical memory

Build a physical page-frame allocator from the boot memory map.

Requirements:

- [x] reserve kernel image;
- [x] reserve boot structures still in use;
- [x] reserve framebuffer;
- [x] reserve ACPI/firmware regions appropriately;
- [x] page-aligned allocation/free;
- [x] double-free detection in debug builds;
- [x] statistics for total/used/free pages;
- [x] deterministic allocator tests with synthetic memory maps.

The current range-based allocator consumes the sanitised boot memory map, excludes the low 1 MiB bootstrap area, removes kernel/framebuffer ranges explicitly and treats non-usable/ACPI regions as unavailable. JoshBootloader's live boot structures are below the allocator floor; Limine bootloader/firmware regions remain non-usable through the adapter. Host tests exercise reservation boundaries, alignment, invalid frees, double-free detection, accounting and a 4096-frame allocate/free stress cycle.

AshFallen CI run `35395723343` booted through Limine with the allocator and the first Josh-owned paging handoff. JoshBIOS cross-repo run `35395898854` then loaded the same kernel from FAT32 and reached `JOSHOS_PAGING_OWNED_OK` and `JOSHOS_BOOT_OK`.

---

## K3 — Virtual memory

- [x] own page tables after boot;
- [x] map kernel with explicit permissions;
- [x] NX where supported;
- [x] read-only kernel text/rodata after init;
- [x] framebuffer mapping;
- [x] MMIO mapping API;
- [x] temporary mapping mechanism;
- [ ] guard pages around critical stacks;
- [ ] address-space abstraction for future processes.

The kernel now allocates its own x86-64 page-table hierarchy from the PMM, switches CR3, and verifies that it is executing on the Josh-owned root. The higher-half kernel maps text read-only/executable, rodata read-only/NX, and data/BSS read-write/NX. The physical direct map is NX, the framebuffer is mapped read-write/NX with cache-disabling flags, and dedicated MMIO plus one-page temporary mapping APIs exist for devices and firmware-table access.

The paging-transition stack has unmapped guard pages and is checked at runtime, but the broader roadmap item remains open until other critical kernel stacks such as exception/per-thread stacks receive the same treatment. `paging_address_space_t` currently names the kernel root only; that is not yet a sufficient process address-space lifecycle abstraction, so that item also remains open.

GitHub Actions run `35407321192` verified CR3 ownership and the RX/RO/RW/NX/framebuffer/guard invariants in QEMU before continuing to `JOSHOS_BOOT_OK`.

---

## K4 — Kernel allocation

- [x] small-object allocator;
- [x] page-backed heap growth;
- [x] allocation failure semantics;
- [x] zeroed allocation helper;
- [x] debug poisoning/canaries where useful;
- [x] leak/accounting hooks for tests.

The kernel heap is backed by contiguous PMM page runs and provides `kmalloc`, `kzalloc` and `kfree`. It splits and coalesces free blocks, returns null on allocation failure, records failure/allocation/region statistics, poisons allocated/freed payloads, and places a canary immediately after the requested payload. Host tests exercise growth, alignment, zeroing, accounting, coalescing/double-free behaviour and canary corruption; the live kernel also runs `heap_self_test()` after switching to Josh-owned page tables.

GitHub Actions run `35407321192` passed the heap host tests and required both `JOSHOS_HEAP_OK` and `JOSHOS_HEAP_SELF_TEST_OK` during the QEMU boot. The allocator is still single-core infrastructure; SMP-safe locking is tracked separately in K6.

---

## K5 — Interrupts and time

- [x] PIC transition/disable strategy;
- [x] local APIC detection;
- [x] IOAPIC where applicable;
- [x] timer source selection;
- [x] monotonic time;
- [x] sleep/deadline primitive;
- [x] interrupt-safe event queue;
- [x] replace keyboard polling with interrupt-driven input.

ACPI RSDP/RSDT/XSDT/MADT parsing now supplies LAPIC, IOAPIC, CPU and ISA-interrupt-override topology. The kernel masks the legacy PIC, enables/maps xAPIC, masks IOAPIC redirection entries by default, and routes only explicitly configured IRQs. The initial clock source is the PIT at 100 Hz through IOAPIC IRQ0; it provides monotonic ticks/nanoseconds and a halt-based millisecond sleep primitive.

Input uses a single-producer/single-consumer interrupt-safe event ring. The PS/2 driver enables IRQ1 in the 8042, routes it through the IOAPIC, decodes Set-1 make/break/modifier events and pushes them into that queue. `keyboard_poll()` is retained only as a compatibility queue consumer; it no longer polls the hardware ports.

GitHub Actions run `35407321192` booted the real ISO with two vCPUs and required `JOSHOS_ACPI_MADT_OK`, `JOSHOS_APIC_OK`, `JOSHOS_IOAPIC_OK`, `JOSHOS_TIMER_IRQ_OK` and `JOSHOS_INTERRUPT_INPUT_READY`. Its separate keyboard IRQ step then used the QEMU monitor `sendkey a` command and required `JOSHOS_KEYBOARD_IRQ_OK`, proving a real emulated PS/2 interrupt reached the kernel event consumer.

---

## K6 — CPU and SMP

After single-core correctness:

- [x] CPUID capability inventory;
- [x] required-feature validation;
- [x] per-CPU state;
- [ ] application processor startup;
- [x] per-CPU stacks;
- [x] interrupt routing;
- [x] spinlock primitives;
- [ ] cross-CPU signalling;
- [ ] SMP-safe allocator and scheduler work.

CPUID inventory records APIC/x2APIC, MSR, TSC, PAE, NX, invariant TSC, SMEP and SMAP capability; the boot path validates its required CPU feature set and enables EFER.NXE before installing the owned mappings. ACPI CPU entries are converted into a bounded per-CPU topology with a identified BSP, per-CPU state and dedicated 32 KiB kernel-stack allocations. IRQ routing targets the BSP's local APIC ID, and x86 spinlocks include try-lock plus IRQ-save/restore forms with a host contention stress test.

The current IPI test deliberately sends an APIC fixed interrupt back to the BSP and verifies delivery through the normal IDT handler. That proves the IPI mechanism but is **not** cross-CPU signalling, so that checkbox remains open. Likewise, QEMU advertises two CPUs and the kernel enumerates both, but the application processor is not yet started into Josh kernel code; AP startup and SMP-safe PMM/heap/scheduler work therefore remain open.

GitHub Actions run `35407321192` required `JOSHOS_SMP_MULTICPU_OK`, `JOSHOS_PERCPU_STACKS_OK` and `JOSHOS_IPI_OK` on a `-smp 2` QEMU boot.

---

## K7 — Device discovery and driver framework

- [ ] PCI/PCIe enumeration;
- [ ] BAR parsing;
- [ ] MMIO/PIO resource model;
- [ ] device/driver matching;
- [ ] structured device tree/inventory exposed to userspace later;
- [ ] clear ownership/lifetime rules.

First drivers should be chosen for testability and usefulness, not novelty.

---

## K8 — Input

Move beyond legacy keyboard polling:

- [ ] PS/2 interrupt-driven keyboard;
- [ ] PS/2 mouse where useful for early desktop work;
- [ ] input event abstraction;
- [ ] key state/modifiers;
- [ ] repeat;
- [ ] pointer events;
- [ ] userspace-facing input queue later.

USB HID is a later major milestone rather than being hidden inside “mouse support”.

---

## K9 — Processes and threads

- [ ] kernel thread primitive;
- [ ] scheduler;
- [ ] context switching;
- [ ] per-thread kernel stack;
- [ ] process/address-space object;
- [ ] user-mode transition;
- [ ] process lifecycle;
- [ ] wait/exit;
- [ ] basic signals/events only if the Josh process model needs them.

Define the process model before copying POSIX by habit.

---

## K10 — Syscall ABI

- [ ] explicit syscall numbering/versioning;
- [ ] argument validation;
- [ ] user-pointer validation/copy helpers;
- [ ] stable error model;
- [ ] capability/handle model groundwork;
- [ ] generated or shared syscall definitions where practical;
- [ ] ABI tests.

The first ABI should be deliberately small.

---

## K11 — Init and userspace

- [ ] executable loader;
- [ ] first userspace init;
- [ ] service supervisor;
- [ ] logging service;
- [ ] shell moved out of the kernel;
- [ ] session service;
- [ ] recovery/rescue target.

The graphical shell currently in the kernel should become a test/demo path once real userspace exists.

---

## K12 — Storage

Start with a clean block-device boundary.

- [ ] block-device API;
- [ ] PCI storage discovery;
- [ ] one QEMU-friendly storage driver;
- [ ] AHCI;
- [ ] NVMe;
- [ ] partition parsing;
- [ ] filesystem decision;
- [ ] VFS;
- [ ] mounts;
- [ ] caching;
- [ ] write ordering and flush semantics.

Data integrity outranks filesystem feature count.

---

## K13 — Files and capabilities

- [ ] file/object handles;
- [ ] directory iteration;
- [ ] metadata model;
- [ ] permissions/capabilities;
- [ ] namespace/mount model;
- [ ] device nodes only if they fit the Josh model;
- [ ] stable application-facing file service above raw kernel APIs.

Do not expose kernel internals as the permanent Josh app API.

---

## K14 — Networking

The first native networking slice is deliberately driver-independent and host-testable.

- [x] network device abstraction;
- [x] loopback device + live kernel self-test;
- [ ] first QEMU NIC driver;
- [x] Ethernet frame encode/decode primitives;
- [x] ARP packet encode/decode primitives;
- [x] IPv4 packet encode/decode + header checksum primitives;
- [x] ICMP echo validation/reply primitive;
- [x] UDP datagram encode/decode + IPv4 pseudo-header checksum primitive;
- [x] DHCP OFFER/ACK reply parser;
- [x] DNS A-query builder + A-response parser;
- [x] TCP segment parser + IPv4 checksum primitive;
- [ ] ARP cache and neighbour state;
- [ ] IPv4 interface/routing state;
- [ ] live ICMP over a NIC;
- [ ] live UDP endpoints;
- [ ] DHCP client state machine, timers and lease renewal;
- [ ] DNS resolver transport/cache;
- [ ] TCP connection state machine, retransmission, flow control and timers;
- [ ] sockets or deliberately chosen Josh networking API;
- [ ] later IPv6.

The checked protocol items above are **wire-format primitives**, not a complete TCP/IP stack. Host tests exercise valid, truncated, malformed and checksum paths, and kernel CI requires the network-device loopback self-test marker. A real e1000/virtio-net path remains dependent on PCI/device discovery, MMIO/DMA, interrupts and stable timeouts/timers.

Verified native-network runs on 18 Sep 2026 include:

- `35396861756` — freestanding packet code builds and the existing kernel boot remains green;
- `35396967862` — network-device and packet host tests green;
- `35396982548` — live kernel loopback self-test green;
- `35397002256` — normal boot smoke explicitly requires `JOSHOS_NET_LOOPBACK_OK`.

Networking must include packet validation and hostile-input testing from the start.

---

## K15 — USB

Treat USB as its own programme:

- [ ] controller discovery;
- [ ] xHCI first for modern machines;
- [ ] transfer rings;
- [ ] hub enumeration;
- [ ] HID keyboard/mouse;
- [ ] mass storage;
- [ ] hotplug;
- [ ] power/state handling.

---

## K16 — Graphics

The existing framebuffer is a good bootstrap path.

Progression:

- [ ] generic framebuffer surface abstraction;
- [ ] damage tracking;
- [ ] compositor-facing buffer model;
- [ ] input-to-surface routing;
- [ ] display modes;
- [ ] multi-monitor model;
- [ ] GPU acceleration only after the software path is stable.

The product track can mature its compositor model on Linux first; the kernel track should eventually host the same conceptual Window/Surface contracts.

---

## K17 — Audio

- [ ] audio device abstraction;
- [ ] one virtual/test driver;
- [ ] stream/ring-buffer model;
- [ ] mixer/service in userspace where practical;
- [ ] real hardware expansion later.

---

## K18 — Power management

- [ ] ACPI discovery/parsing;
- [ ] clean power-off;
- [ ] reboot paths;
- [ ] CPU idle;
- [ ] suspend/resume design;
- [ ] laptop lid/battery model later;
- [ ] wake events.

Suspend/resume should be treated as a system-wide checkpoint/restore problem, not one ACPI call.

---

## K19 — Security hardening

- [ ] W^X memory policy;
- [ ] NX;
- [ ] SMEP/SMAP where available;
- [ ] user/kernel isolation;
- [ ] randomised identifiers/ASLR strategy where meaningful;
- [ ] entropy subsystem;
- [ ] capability-based authority or another explicit permission model;
- [ ] signed/verified executable policy if adopted;
- [ ] fuzzable parsers for filesystem/network/protocol inputs.

---

## K20 — Reliability and testability

### Unit/host tests

Extract pure logic for host testing:

- allocators;
- memory-map normalisation;
- ELF parsing;
- filesystem parsers;
- packet parsing;
- boot protocol validation.

### VM integration

Test:

- multiple RAM sizes;
- multiple CPU counts;
- BIOS and UEFI;
- expected panic paths;
- reboot;
- recovery kernel;
- invalid boot data;
- storage corruption simulations where practical.

### Physical

Start with the selected Compaq 610 target. Record each subsystem result rather than using a single pass/fail label. Add it to the support matrix only after repeated cold boots and documented subsystem results; add further hardware only after the first target has a repeatable baseline.

---

## Kernel “1.0” bar

The native kernel should not be called 1.0 merely because it has many drivers.

A credible 1.0 native base should have:

- protected userspace;
- stable syscall ABI;
- scheduler;
- memory safety boundaries at process level;
- storage/filesystem with recovery strategy;
- networking;
- USB keyboard/mouse/storage;
- usable graphics path;
- audio path;
- clean shutdown/reboot;
- update/recovery integration;
- repeatable VM tests;
- at least one explicitly supported physical development system.

The Josh OS product may reach its own 1.x milestones on Linux well before the native kernel reaches this point.
