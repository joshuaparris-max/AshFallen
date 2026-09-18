# Josh OS Roadmap

Josh OS is being built through two complementary tracks: a **product track** that reaches a polished usable desktop sooner, and a **native-kernel track** that progressively owns more of the stack.

Every milestone should leave its implementation understandable and demonstrably working.

## Deep roadmaps

- [Power button → desktop](docs/POWER_ON_TO_DESKTOP_ROADMAP.md) — whole-stack ownership, firmware/start screen, boot menu, recovery, updates and physical-hardware support.
- [Native kernel](docs/KERNEL_ROADMAP.md) — exceptions, memory, interrupts, processes, storage, networking, USB, graphics, power and security.
- [Josh Boot Protocol](docs/BOOT_ABI.md) — future JoshBootloader ↔ canonical kernel contract.
- [Current boot stack](docs/BOOT_STACK.md) — today’s Limine adapter and branded Josh OS Boot Manager.
- [Product/start/recovery experience](docs/product/OS_EXPERIENCE_ROADMAP.md) — firmware/OS splash continuity, first-run, session, accessibility, updates and recovery.
- [Product track](docs/product/ROADMAP.md) — Linux-backed usable desktop and convergence.

The current **Josh OS Boot Manager** is the branded Limine menu and remains a useful reference path. The long-term pre-kernel menu belongs in JoshBootloader once it can boot this canonical kernel reliably.

## First physical validation target

The first selected machine is **DadLAN Laptop #10, a Compaq 610**. It is a development target, not yet a supported Josh OS machine. Machine-specific inventory, firmware safety gates and the evidence ladder are maintained in [JoshBIOS's Compaq 610 target document](https://github.com/Parris-Tech-Services/JoshBIOS/blob/main/docs/HARDWARE_TARGET_COMPAQ_610.md). Native-kernel work should use this concrete machine to expose real assumptions while keeping drivers and platform quirks behind generic subsystem boundaries.

## Native kernel 0.1 — It lives

- [x] x86-64 kernel entry point
- [x] BIOS/UEFI hybrid ISO build
- [x] Limine boot handoff
- [x] graphical framebuffer desktop
- [x] tiny bitmap font renderer
- [x] PS/2 keyboard input
- [x] graphical command shell
- [x] serial boot marker for automated testing
- [x] GitHub Actions ISO build + QEMU smoke test
- [ ] publish downloadable ISO from tagged releases
- [ ] boot-test the native ISO on the first selected physical target — DadLAN Laptop #10 / Compaq 610 — from removable media, then record repeatability and limitations

## Product track 0.1 — Define the desktop

Carry forward the best concepts already explored in the browser prototype:

- [ ] canonical App model
- [ ] canonical Window model
- [ ] focus and stacking model
- [ ] move/resize/minimise/maximise
- [ ] left/right/fullscreen snapping
- [ ] dock and launcher
- [ ] menu/status bar
- [ ] notification model
- [ ] settings and persisted preferences
- [ ] light/dark themes
- [ ] shared design-token specification
- [ ] accessibility rules for keyboard navigation, focus and contrast

The browser implementation is a design/prototyping environment, not the production OS architecture.

## Product track 0.2 — Native Linux-backed desktop

Build the first production Josh desktop on mature Linux infrastructure.

Likely responsibilities:

- Wayland compositor/window manager;
- Josh desktop shell;
- launcher and dock;
- settings service;
- notification service;
- session management;
- app registry/manifest format;
- file manager;
- terminal;
- text editor;
- theme/token compiler;
- package/update strategy.

The exact compositor toolkit and distro/base should be chosen through a small technical spike rather than prematurely frozen.

## Native kernel 0.2 — It manages itself

- interrupt descriptor table and exception reporting
- programmable timer
- physical page allocator
- virtual memory manager
- kernel heap
- event-driven keyboard input
- mouse input
- first real surface/window primitives

## Native kernel 0.3 — It runs programs

- processes and threads
- scheduler
- userspace privilege separation
- system-call ABI
- executable loader
- init process
- shell moved out of the kernel
- capability/security model groundwork

## Product track 0.3 — A daily-usable Josh OS image

- installable/live ISO
- polished first-boot flow
- network, audio and power controls
- Bluetooth and removable-device UX
- application installation/update path
- crash reporting/log viewer
- recovery mode
- accessibility pass
- hardware test matrix
- automated desktop interaction tests

At this stage Linux supplies mature hardware support while Josh OS owns the user experience and increasingly owns its services.

## Native kernel 0.4 — It has a home

- block-device abstraction
- PCI enumeration
- AHCI/NVMe groundwork
- filesystem strategy
- virtual filesystem layer
- native file APIs
- file manager talking through stable Josh APIs rather than kernel internals

## Native kernel 0.5 — It talks

- network-device abstraction
- first Ethernet driver
- ARP and IPv4
- ICMP
- UDP and TCP
- DHCP
- DNS
- native network client
- later USB and Wi-Fi work

## Convergence milestone

The important long-term test is not "can the Josh kernel imitate Linux?"

It is:

> Can the same Josh desktop concepts and application APIs run on both the mature Linux-backed system and the independent Josh kernel?

To make that possible:

- keep Window/App/File/Notification/Setting concepts implementation-neutral;
- avoid leaking DOM/browser concepts into Josh APIs;
- avoid leaking Linux/Wayland-specific concepts into Josh application APIs where unnecessary;
- avoid leaking raw kernel implementation details upward;
- maintain conformance tests for shared APIs.

## Long-term Josh OS

A mature Josh OS stack should look approximately like:

    Josh applications
           ↓
    Josh application APIs
           ↓
    Josh desktop + compositor
           ↓
    Josh system services
           ↓
    Josh userspace/runtime
           ↓
    Josh kernel
           ↓
    hardware

Getting there is an incremental research direction, not a promise to recreate decades of hardware support before Josh OS becomes useful.

## Engineering rule

Principles outrank metrics.

Correctness, conceptual integrity, understandable architecture and verified behaviour matter more than optimising CRAP, complexity, coverage or any other individual score. Metrics are evidence and useful ratchets; they are not the definition of beautiful software.
