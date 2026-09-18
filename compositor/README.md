# Josh Window System — the real product compositor

Stage 1. Nothing is written yet, deliberately: this directory holds the plan,
not placeholder code. Fake scaffolding in a repo is a lie about progress.

## The choice: Smithay vs wlroots

| | **Smithay** (Rust) | **wlroots** (C) |
|---|---|---|
| Language | Rust | C |
| Model | A library of parts you assemble | A more complete framework |
| Control | Total — you write the compositor loop | More is done for you |
| Users | COSMIC, Niri | Sway, Hyprland, river |
| Learning cost | Higher | Lower |

**Leaning Smithay.** A compositor is the one place where a memory-safety bug
takes the whole session with it, and the Josh window model is unusual enough
that "assemble from parts" beats "adapt a framework".

This is not locked in. Prototype a hello-world in both before committing.

## Build order

Each step should end with something visibly working.

1. **Boot to a black screen.** Compositor starts under a TTY, initialises DRM,
   and opens a session via libseat.
2. **One window.** Accept a Wayland client, allocate a surface, composite it.
3. **Input.** libinput: pointer, keyboard, focus follows click.
4. **The window model.** Port `snapZoneFor()`, focus and window state from
   `shell/shell.js`.
5. **Decorations.** Server-side, drawn from `design/tokens.json`.
6. **XWayland.** For applications not yet native to Wayland.
7. **Shell surfaces.** Panel, dock and notifications as layer-shell clients.

## Things that will hurt

- DRM/KMS and libseat debugging without an existing window system.
- Multi-monitor, hotplug and scaling.
- Suspend/resume and VT switching.

## Prerequisite

Settle the Stage 0 interaction model before it costs a thousand lines of Rust
to change fundamental window behaviour.
