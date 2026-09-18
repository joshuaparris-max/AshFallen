# Josh OS Architecture

## Architectural intent

Josh OS is one system being developed from both ends:

- **top-down**, by specifying the desktop, window system, application model and visual language;
- **bottom-up**, through an independent x86-64 kernel that already boots and renders directly to a framebuffer.

A practical Linux-backed product track will sit between those ends while native kernel capability grows.

See [PRODUCT_STRATEGY.md](PRODUCT_STRATEGY.md) for the convergence plan, [BOOT_STACK.md](BOOT_STACK.md) for the power-button-to-desktop contract, and [DESKTOP_PROTOTYPE_COMPARISON.md](DESKTOP_PROTOTYPE_COMPARISON.md) for the lessons taken from the early desktop prototype.

## Native v0.1 boot path

    BIOS / UEFI
        ↓
    Limine 12.9
        ↓
    boot_limine.c adapter
        ↓
    Josh boot_context_t
        ↓
    Josh kernel
        ↓
    framebuffer renderer
        ↓
    Josh graphical desktop + shell
        ↓
    PS/2 keyboard polling

Limine is currently the boot manager/loader. After handoff, the Josh kernel owns execution. There is no Linux kernel underneath this native path.

Crucially, Limine-specific structures now stop at `boot_limine.c`. Graphics, desktop and shell code consume Josh-owned structures. That is the seam a future JoshBootloader adapter will use.

## Current native source modules

- `main.c` — kernel orchestration after a validated Josh boot context exists.
- `boot_limine.c` / `boot.h` — boot-protocol adapter and Josh-owned boot context.
- `gfx.c` — bootloader-independent framebuffer drawing primitives and colour conversion.
- `font.c` — deliberately tiny built-in 5×7 bitmap font.
- `desktop.c` — visual composition of the v0.1 desktop.
- `shell.c` — command state and terminal rendering.
- `keyboard.c` — minimal PS/2 Set-1 keyboard input.
- `serial.c` — COM1 diagnostics and CI boot marker.
- `memory.c` — freestanding memory primitives required by the compiler.

## Future production desktop architecture

The first daily-usable Josh OS should initially use mature Linux hardware support while keeping Josh concepts above it:

    Josh apps
       ↓
    Josh application API
       ↓
    Josh desktop shell
       ↓
    Josh compositor/window system
       ↓
    Josh system services
       ↓
    Linux kernel/drivers

This is a staging architecture, not an abandonment of the Josh kernel.

## Shared conceptual layer

The crucial architectural boundary is a set of Josh OS concepts that can survive implementation changes.

Candidate core models:

### App

Identity, metadata, launch contract, capabilities and preferred presentation.

### Window

Identity, owner app/process, title, geometry, state, focus/stacking state and supported operations.

### Surface

Drawable content owned by a window or system component. Browser DOM, Wayland buffers and future native Josh buffers are implementations, not the concept itself.

### Command

A named user/system action that can be invoked from menus, launchers, keyboard shortcuts or automation.

### Notification

A structured event intended for user attention, independent of how a particular shell renders it.

### Setting

Typed configuration with ownership, scope, default and persistence rules.

### Capability

Explicit authority granted to an application or service rather than incidental access to global system state.

## Design-system architecture

Josh OS should have a shared, implementation-neutral design source rather than independent CSS/C constants.

It should define semantic tokens such as:

- surface/background roles;
- text roles;
- accent and status roles;
- spacing scale;
- radius scale;
- typography scale;
- elevation;
- animation duration/easing;
- window constraints;
- desktop geometry.

Generators/adapters may then produce CSS for prototypes, compositor/native constants and application resources.

## Current native boundaries

The v0.1 graphical desktop is **not** yet a compositor. Its apparent window and dock are rendered directly into the boot framebuffer.

Keyboard input is polled rather than interrupt-driven.

There is currently no:

- process isolation;
- userspace;
- filesystem;
- USB stack;
- mouse driver;
- networking;
- audio;
- GPU acceleration.

Those are explicit limitations, not hidden dependencies.

## Architectural principles

1. **Concepts map to reality.** A module exists because the system has that concept, not because a framework convention demanded a layer.
2. **Separate concepts from adapters.** Window is a Josh concept; DOM, Wayland and native framebuffer/compositor implementations are adapters.
3. **Dependencies point inward.** Hardware details stay near hardware-facing modules. Application code should not know about I/O ports or Linux-specific plumbing.
4. **Boot remains observable.** Serial diagnostics should make failures understandable before a GUI is available.
5. **The system remains demonstrably working.** Major work should land as vertical slices with useful automated evidence.
6. **Beauty follows coherence.** Visual polish, interaction design and source architecture should reinforce one another.
7. **Truth over theatre.** A prototype feature is not called an OS capability until the relevant underlying layer genuinely exists.
