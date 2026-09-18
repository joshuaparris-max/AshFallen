# Josh OS

Josh OS is an experimental desktop operating-system project for x86-64 PCs.

The long-term aim is a coherent system that combines the openness of Linux, the familiar desktop conventions of Windows, and the visual restraint of macOS—without simply cloning any of them.

> **Josh OS should become clearer as you understand more of it.**

## Repository map

Josh OS currently spans three related repositories with different jobs:

- **[joshuaparris-max/AshFallen](https://github.com/joshuaparris-max/AshFallen)** — the **canonical Josh OS integration repository**. It contains both the Linux-backed product track and the independent x86-64 Josh kernel track.
- **[Parris-Tech-Services/JoshOS-Stage0](https://github.com/Parris-Tech-Services/JoshOS-Stage0)** — a **Stage 0 product-track extraction/prototype** focused on the browser shell and ArchISO live image. It is useful for iterating on the desktop experience, but it is not the canonical native-kernel repository.
- **[Parris-Tech-Services/JoshBIOS](https://github.com/Parris-Tech-Services/JoshBIOS)** — the **JoshBIOS / firmware / bootloader research stack**. Its `boot/`, `firmware/` and small `kernel/` payload are exploring the power-on-to-kernel handoff independently of the main Josh OS kernel.

The current native Josh OS kernel boots through **Limine**. JoshBIOS does **not** currently boot this kernel. The intended future integration point is a versioned boot ABI: once JoshBootloader can load ELF64/x86-64 kernels and provide the required memory/framebuffer/firmware information, it can become an alternative boot path into the canonical Josh OS kernel.

## Two complementary tracks

Josh OS deliberately has two development tracks.

### Product track — the usable desktop

The product track builds a beautiful, practical Josh desktop on top of the Linux kernel initially so it can benefit from mature drivers and run on modern hardware.

It now includes:

- a browser-based Josh Window System prototype;
- movable/resizable/minimisable/maximisable windows;
- edge snapping;
- dock and menu bar;
- notifications;
- light/dark themes, accents and wallpapers;
- About, Files, Terminal, Text Editor and Settings apps;
- a shared design-token system;
- an ArchISO pipeline that boots the Josh desktop full-screen in Chromium kiosk mode.

GitHub Actions builds this as the **JoshOS-Stage0-Live-x86_64** artifact.

### Native-kernel track — the independent OS

The independent Josh kernel already:

- boots on x86-64 through Limine;
- shows a **Josh OS Boot Manager** for three seconds before auto-boot;
- isolates Limine behind a Josh-owned boot-context adapter;
- produces a hybrid BIOS/UEFI ISO suitable for QEMU and Ventoy;
- discovers the framebuffer and memory map;
- renders directly to the framebuffer;
- accepts basic PS/2 keyboard input;
- exposes a small graphical shell;
- emits serial diagnostics;
- is actually booted by GitHub Actions in QEMU.

GitHub Actions builds this as **JoshOS-0.1-x86_64.iso**.

There is no Linux kernel underneath the native-kernel build.

## Which ISO should I use?

**Want to see the desktop/window experience?** Use the **Stage 0 product ISO**. It boots Linux/ArchISO underneath, autologs into a minimal session and launches the Josh desktop full-screen.

**Want to boot the actual Josh kernel?** Use **JoshOS-0.1-x86_64.iso**. It is much more primitive visually, but the kernel and framebuffer code are ours.

Both can be tested in a VM. The native ISO is also intended for Ventoy testing on suitable x86-64 hardware.

## Build the native-kernel ISO

On Ubuntu/Debian:

```sh
sudo apt install clang lld make xorriso curl git qemu-system-x86
make
```

Output:

```text
JoshOS-0.1-x86_64.iso
```

Run:

```sh
make run
```

## Build the Stage 0 product ISO

On Arch Linux:

```sh
sudo pacman -S archiso
sudo bash ./scripts/build-iso.sh
```

Output is written to:

```text
out/josh-os-*.iso
```

The GitHub Actions workflow **Build Josh OS Product ISO** performs this build automatically and uploads the ISO with its SHA-256 checksum.

## Architecture direction

The tracks are intended to converge, not remain separate forever.

```text
Josh apps
   ↓
Josh application APIs
   ↓
Josh desktop / compositor
   ↓
Josh services
   ↓
Linux kernel initially
   ↓
hardware

          while in parallel

Josh kernel
   ↓
memory / interrupts / processes
   ↓
filesystems / drivers / networking
   ↓
eventual Josh userspace + desktop
```

Shared concepts such as App, Window, Surface, Setting, Notification and Capability should remain implementation-independent so the desktop does not need to be reinvented during convergence.

## Current limitations

Josh OS is pre-alpha.

The native desktop is not yet a compositor and lacks mouse/USB, processes, userspace, filesystem, networking, audio and accelerated graphics.

The Stage 0 product ISO is intentionally a compatibility vehicle: ArchISO + LightDM + Openbox + Chromium host the prototype. It is not the final Wayland architecture.

## Documentation

- [Full-stack programming plan](docs/FULL_STACK_PROGRAMMING_PLAN.md) — 37 phases from power-button policy through firmware, kernel, userspace, desktop and applications.
- [Difficulty, scope and feasibility](docs/DIFFICULTY_SCOPE_AND_FEASIBILITY.md) — tractable vs brutal vs blocked work, missing architectural programmes, and realistic completion definitions.
- [Roadmap](ROADMAP.md)
- [Power button → desktop roadmap](docs/POWER_ON_TO_DESKTOP_ROADMAP.md)
- [Native kernel roadmap](docs/KERNEL_ROADMAP.md)
- [Josh Boot Protocol roadmap](docs/BOOT_ABI.md)
- [OS/start/recovery experience roadmap](docs/product/OS_EXPERIENCE_ROADMAP.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Boot stack: power button to desktop](docs/BOOT_STACK.md)
- [Product and architecture strategy](docs/PRODUCT_STRATEGY.md)
- [Desktop prototype vs native kernel](docs/DESKTOP_PROTOTYPE_COMPARISON.md)
- [Product-track architecture](docs/product/ARCHITECTURE.md)
- [Product-track roadmap](docs/product/ROADMAP.md)
- [Design principles](docs/DESIGN_PRINCIPLES.md)
- [VirtualBox product ISO test](docs/VIRTUALBOX.md)
- [Architecture decisions](docs/decisions/)
