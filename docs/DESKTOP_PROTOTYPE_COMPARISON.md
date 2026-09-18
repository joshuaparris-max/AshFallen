# Desktop Prototype and Native Kernel Comparison

This document records the architectural lesson from comparing two early Josh OS implementations:

1. the browser-based desktop prototype supplied during the initial design exploration; and
2. the native x86-64 Josh kernel currently in this repository.

Neither replaces the other. They answer different questions.

## What the desktop prototype proves

The prototype explores what Josh OS should feel like as a desktop product.

Its strengths include:

- movable, resizable, minimisable and maximisable windows;
- focus and stacking behaviour;
- window snapping;
- dock and menu-bar interactions;
- multiple prototype applications;
- settings, themes and wallpaper behaviour;
- notifications and persisted UI state;
- a coherent design-token approach;
- a clearer Window/App abstraction than the current kernel UI.

Its architecture can be summarised as:

    Application
        ↓
    Window record
        ↓
    focus / geometry / state
        ↓
    window manager
        ↓
    rendered desktop

That is significantly further ahead as a *desktop interaction model* than the kernel renderer.

However, browser behaviour is provided by an existing host operating system and browser. Mouse input, fonts, process isolation, storage, rendering acceleration and networking are not Josh OS implementations merely because the prototype can use them.

## What the native kernel proves

The current repository explores the opposite end of the stack.

Its strengths include:

- a real x86-64 freestanding kernel;
- BIOS/UEFI hybrid ISO generation;
- Limine boot handoff;
- direct framebuffer rendering;
- direct PS/2 keyboard input;
- memory-map discovery;
- serial diagnostics;
- a tiny graphical shell;
- automated QEMU boot verification;
- a downloadable ISO build artifact.

Its path is:

    firmware
        ↓
    Limine
        ↓
    Josh kernel
        ↓
    framebuffer / hardware-facing code
        ↓
    Josh graphical shell

There is no Linux kernel or browser underneath that execution path.

The limitation is equally important: the current apparent "window" is only directly rendered geometry. It is not yet a compositor-managed window with independent surfaces, mouse interaction, movement, resizing or process ownership.

## Side-by-side status

| Capability | Desktop prototype | Native kernel |
| --- | --- | --- |
| Boots a physical/virtual PC independently | No | Yes |
| Produces a bootable ISO | No | Yes |
| Own kernel | No | Yes |
| Real framebuffer access | Host/browser abstraction | Yes |
| Movable/resizable windows | Yes | Not yet |
| Window snapping | Yes | Not yet |
| Mouse interaction | Host/browser | Not yet |
| Dock/menu interactions | Yes | Visual only |
| Multiple app prototypes | Yes | Shell only |
| Real filesystem | No | No |
| Theme/settings exploration | Yes | Not yet |
| Design-token system | Yes | Not yet |
| Automated native boot proof | No | Yes |

## Architectural conclusion

The prototype is currently the better specification for **how Josh OS should look and behave**.

The native build is currently the stronger proof for **how Josh OS can eventually own the machine underneath that experience**.

The right response is not to discard either implementation. Josh OS should transfer the prototype's strongest concepts into a production desktop architecture while continuing the independent kernel as a parallel research track.

## Concepts worth carrying forward

The following prototype ideas should be treated as design input for the real desktop:

### Window state

A window should explicitly own:

- identity;
- application identity;
- title;
- x/y position;
- width/height;
- normal/minimised/maximised/snapped state;
- previous geometry for restoration;
- focus/stacking state;
- capabilities such as resizable or closable.

### App registration

Applications should be registered through a stable app model rather than hard-wired directly into the shell.

A future application manifest should describe identity, name, icon, launch entry point, required capabilities and preferred initial window geometry.

### Desktop state

The desktop should own system-level concepts such as:

- focused window;
- window stack;
- workspaces;
- dock/pinned applications;
- notifications;
- theme;
- wallpaper;
- session state.

### Design tokens

Visual decisions should come from one source of truth and be compiled into the implementation, rather than being scattered as magic numbers through compositor or application code.

## What should *not* be copied blindly

Browser-specific implementation details are prototypes, not architecture.

Do not make DOM nodes, CSS layout, localStorage or browser events part of Josh OS' conceptual model.

Likewise, do not promote kernel-specific implementation details such as PS/2 polling or raw framebuffer rectangles into the application API.

Both ends should implement shared Josh OS concepts through appropriate adapters.
