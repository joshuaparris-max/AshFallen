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
- [ ] map kernel with explicit permissions;
- [ ] NX where supported;
- [ ] read-only kernel text/rodata after init;
- [ ] framebuffer mapping;
- [ ] MMIO mapping API;
- [ ] temporary mapping mechanism;
- [ ] guard pages around critical stacks;
- [ ] address-space abstraction for future processes.

The first K3 slice is now real: the kernel allocates its own x86-64 page-table hierarchy from the PMM, maps the direct physical-memory window plus the higher-half kernel, switches CR3, moves to a Josh-owned transition stack and only then emits `JOSHOS_PAGING_OWNED_OK`. This is verified on both the Limine and JoshBootloader QEMU paths. Permissions are intentionally still permissive; W^X/NX and section-level read-only mappings remain outstanding.

Document the intended higher-half layout before it spreads through code.

---

## K4 — Kernel allocation

- [ ] small-object allocator;
- [ ] page-backed heap growth;
- [ ] allocation failure semantics;
- [ ] zeroed allocation helper;
- [ ] debug poisoning/canaries where useful;
- [ ] leak/accounting hooks for tests.

Avoid making the allocator API more clever than current needs.

---

## K5 — Interrupts and time

- [ ] PIC transition/disable strategy;
- [ ] local APIC detection;
- [ ] IOAPIC where applicable;
- [ ] timer source selection;
- [ ] monotonic time;
- [ ] sleep/deadline primitive;
- [ ] interrupt-safe event queue;
- [ ] replace keyboard polling with interrupt-driven input.

A stable clock is foundational for scheduling, input, networking and logs.

---

## K6 — CPU and SMP

After single-core correctness:

- [ ] CPUID capability inventory;
- [ ] required-feature validation;
- [ ] per-CPU state;
- [ ] application processor startup;
- [ ] per-CPU stacks;
- [ ] interrupt routing;
- [ ] spinlock primitives;
- [ ] cross-CPU signalling;
- [ ] SMP-safe allocator and scheduler work.

Do not make early subsystems accidentally SMP-dependent before this milestone.

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
