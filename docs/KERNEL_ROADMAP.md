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

---

## K0 — Boot contract hardening

### Goals

- [x] Introduce a boot-adapter layer instead of reading Limine request structures throughout kernel code.
- [ ] Define an internal `josh_boot_info` representation.
- [x] Validate framebuffer geometry and address arithmetic.
- [ ] Validate/sanitise memory-map entries.
- [ ] Preserve raw boot-protocol metadata for debug builds.
- [ ] Add support for Josh Boot Protocol alongside Limine.
- [ ] Keep Limine as a reference boot path until the JoshBootloader path is equally reliable.

### Exit test

The same kernel binary or source tree can boot through both supported adapters and reach the same `JOSHOS_BOOT_OK` milestone.

---

## K1 — Exceptions before features

Implement:

- [ ] GDT owned by the kernel;
- [ ] TSS;
- [ ] IDT;
- [ ] exception stubs;
- [ ] page-fault handler;
- [ ] general-protection-fault handler;
- [ ] double-fault strategy;
- [ ] register dump;
- [ ] stack trace groundwork where feasible;
- [ ] graphical + serial panic path.

### Test cases

Deliberately trigger:

- invalid opcode;
- divide-by-zero;
- page fault;
- general protection fault.

CI should prove the expected panic marker appears instead of hanging.

---

## K2 — Physical memory

Build a physical page-frame allocator from the boot memory map.

Requirements:

- [ ] reserve kernel image;
- [ ] reserve boot structures still in use;
- [ ] reserve framebuffer;
- [ ] reserve ACPI/firmware regions appropriately;
- [ ] page-aligned allocation/free;
- [ ] double-free detection in debug builds;
- [ ] statistics for total/used/free pages;
- [ ] deterministic allocator tests with synthetic memory maps.

Start simple. A bitmap allocator is acceptable if its invariants are easy to prove.

---

## K3 — Virtual memory

- [ ] own page tables after boot;
- [ ] map kernel with explicit permissions;
- [ ] NX where supported;
- [ ] read-only kernel text/rodata after init;
- [ ] framebuffer mapping;
- [ ] MMIO mapping API;
- [ ] temporary mapping mechanism;
- [ ] guard pages around critical stacks;
- [ ] address-space abstraction for future processes.

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

Incremental path:

- [ ] network device abstraction;
- [ ] loopback;
- [ ] first QEMU NIC driver;
- [ ] Ethernet;
- [ ] ARP;
- [ ] IPv4;
- [ ] ICMP;
- [ ] UDP;
- [ ] DHCP;
- [ ] DNS;
- [ ] TCP;
- [ ] sockets or deliberately chosen Josh networking API;
- [ ] later IPv6.

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

Add hardware only to the support matrix after repeated cold boots and documented subsystem results.

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
