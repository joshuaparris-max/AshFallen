# Josh OS

Josh OS is an experimental desktop operating-system project for x86-64 PCs.

The long-term aim is a coherent system that combines the openness of Linux, the familiar desktop conventions of Windows, and the visual restraint of macOS—without simply cloning any of them.

> **Josh OS should become clearer as you understand more of it.**

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

- [Roadmap](ROADMAP.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Product and architecture strategy](docs/PRODUCT_STRATEGY.md)
- [Desktop prototype vs native kernel](docs/DESKTOP_PROTOTYPE_COMPARISON.md)
- [Product-track architecture](docs/product/ARCHITECTURE.md)
- [Product-track roadmap](docs/product/ROADMAP.md)
- [Design principles](docs/DESIGN_PRINCIPLES.md)
- [VirtualBox product ISO test](docs/VIRTUALBOX.md)
- [Architecture decisions](docs/decisions/)
