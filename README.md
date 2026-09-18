# Josh OS

Josh OS is an experimental desktop operating system for x86-64 PCs.

The long-term aim is a coherent system that combines the openness of Linux, the familiar desktop conventions of Windows, and the visual restraint of macOS—without simply cloning any of them.

## v0.1 goal

The first milestone is deliberately small and real:

- boot as a standalone x86-64 operating system
- produce a hybrid BIOS/UEFI ISO suitable for QEMU and Ventoy
- initialise a graphical framebuffer
- render the beginnings of the Josh OS desktop
- accept basic keyboard input through a tiny graphical shell
- keep the architecture understandable enough to learn from

Josh OS is **not Linux**. The kernel code in this repository runs directly after the Limine bootloader hands control to it.

## Build

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

## Status

Josh OS is pre-alpha. Hardware support is intentionally tiny. The v0.1 keyboard driver targets the classic PS/2 controller used by QEMU and some physical PCs; modern USB keyboard support is a later milestone.

See [ROADMAP.md](ROADMAP.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
