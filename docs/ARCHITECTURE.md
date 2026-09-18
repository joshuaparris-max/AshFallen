# Josh OS Architecture

## What it is

Josh OS is an independent x86-64 operating-system project. Limine is currently used only as the bootloader: after handoff, the Josh OS kernel owns execution. There is no Linux kernel underneath the current system.

## v0.1 boot path

    BIOS / UEFI
        ↓
    Limine 12.9
        ↓
    Josh kernel ELF64
        ↓
    Framebuffer + memory-map discovery
        ↓
    Josh graphical desktop renderer
        ↓
    Josh graphical shell
        ↓
    PS/2 keyboard polling

## Source modules

- main.c — kernel entry and boot-protocol requests.
- gfx.c — framebuffer drawing primitives and colour conversion.
- font.c — deliberately tiny built-in 5×7 bitmap font.
- desktop.c — visual composition of the v0.1 desktop.
- shell.c — command state and terminal rendering.
- keyboard.c — minimal PS/2 Set-1 keyboard input.
- serial.c — COM1 diagnostics and CI boot marker.
- memory.c — freestanding memory primitives required by the compiler.

## Current boundaries

The graphical desktop in 0.1 is not yet a compositor. Its window and dock are rendered directly into the boot framebuffer. Keyboard input is polled, not interrupt-driven. There is no process isolation, filesystem, USB stack, networking, audio or GPU acceleration yet.

Those are limitations, not hidden dependencies. Each will become an explicit subsystem as the project grows.

## Architectural principles

1. **Concepts should map to reality.** A module exists because the system has that concept, not because a framework convention demanded another layer.
2. **Dependencies point inward.** Hardware details stay near hardware-facing modules; UI code should not manipulate I/O ports.
3. **Boot remains observable.** Serial diagnostics should make failures understandable before a GUI is available.
4. **The system remains bootable.** Major work should land as vertical slices that still produce a runnable ISO.
5. **Beauty follows coherence.** Visual polish matters, but the source and runtime model should tell the same story.
