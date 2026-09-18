# Josh OS Roadmap

Josh OS is being built in layers. Every milestone should leave the system bootable and understandable.

## 0.1 — It lives

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
- [ ] test on physical hardware through Ventoy

## 0.2 — It manages itself

- interrupt descriptor table and exception reporting
- programmable timer
- physical page allocator
- virtual memory manager
- kernel heap
- event-driven keyboard input
- mouse input
- basic window manager with movable/resizable windows

## 0.3 — It runs programs

- processes and threads
- scheduler
- userspace privilege separation
- system-call ABI
- executable loader
- init process
- shell moved out of the kernel

## 0.4 — It has a home

- block-device abstraction
- AHCI/NVMe groundwork
- simple native filesystem or a deliberately chosen existing filesystem
- file manager
- text editor
- settings application
- application bundle/manifest format

## 0.5 — It talks

- PCI enumeration
- network-device abstraction
- first Ethernet driver
- ARP, IPv4, ICMP, UDP and TCP
- DHCP and DNS
- small native network client

## 1.0 — A coherent desktop OS

A usable graphical system with its own kernel, userspace, compositor/window manager, filesystem stack, networking, application model and installer. Hardware support will still be narrower than Linux, Windows or macOS; the aim is conceptual integrity and understandability rather than pretending decades of driver work can be skipped.

## Design rule

> Josh OS should become clearer as you understand more of it.

Prefer a small number of strong concepts over layers of incidental abstraction. Metrics are useful evidence, but architectural clarity and correctness outrank metric-chasing.
