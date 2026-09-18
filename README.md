# Josh OS

Josh OS is an experimental desktop operating-system project for x86-64 PCs.

The long-term aim is a coherent system that combines the openness of Linux, the familiar desktop conventions of Windows, and the visual restraint of macOS—without simply cloning any of them.

> **Josh OS should become clearer as you understand more of it.**

## Two complementary tracks

Josh OS now deliberately has two development tracks:

**Product track:** build a beautiful, practical Josh desktop on top of the Linux kernel initially, so it can benefit from mature drivers and run on real modern hardware.

**Research track:** continue the independent Josh kernel in this repository, progressively implementing memory, interrupts, processes, filesystems, drivers, networking and eventually the same Josh desktop stack natively.

The intention is convergence, not two permanent operating systems. Shared concepts such as apps, windows, settings, files, notifications and capabilities should remain implementation-independent.

See [docs/PRODUCT_STRATEGY.md](docs/PRODUCT_STRATEGY.md) for the full plan.

## What exists today

The native kernel already:

- boots independently on x86-64 through Limine;
- produces a hybrid BIOS/UEFI ISO suitable for QEMU and Ventoy;
- discovers the framebuffer and memory map;
- renders a graphical Josh OS desktop directly;
- accepts basic PS/2 keyboard input;
- exposes a tiny graphical shell;
- emits serial diagnostics;
- is built and actually booted by GitHub Actions in QEMU.

The shell currently supports commands including `help`, `about`, `mem`, `clear`, `echo` and `reboot`.

This is genuinely native code: there is no Linux kernel underneath the current kernel build.

## Build the native kernel

On Ubuntu/Debian:

```sh
sudo apt install clang lld make xorriso curl git qemu-system-x86
make
```

The output is:

```text
JoshOS-0.1-x86_64.iso
```

Run it with:

```sh
make run
```

Or copy the ISO to a Ventoy USB and boot it from Ventoy.

## Current limitations

Josh OS is pre-alpha.

The native desktop is currently direct framebuffer rendering, not yet a real compositor. The window and dock are visual structures rather than independently managed surfaces. Keyboard support is intentionally tiny and PS/2-oriented. There is not yet a USB HID stack, mouse driver, scheduler, userspace, filesystem, networking, audio stack or accelerated GPU driver.

These are explicit roadmap items rather than hidden dependencies.

## Documentation

- [Roadmap](ROADMAP.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Product and architecture strategy](docs/PRODUCT_STRATEGY.md)
- [Desktop prototype vs native kernel](docs/DESKTOP_PROTOTYPE_COMPARISON.md)
