# ADR-0002: Wayland, not X11

**Status:** accepted  
**Date:** 2026-09-18

## Context

A new Linux-backed desktop needs a display protocol. X11 has decades of
compatibility and tooling. Wayland is the direction modern Linux desktops have
committed to.

## Decision

Wayland. Josh OS intends to ship a Wayland compositor and use XWayland only for
legacy applications.

## Rationale

In Wayland the compositor is also the window manager, which fits Josh OS'
preference for fewer, stronger concepts. Screen capture, global hotkeys and
accessibility are explicit protocol/portal problems rather than universal
client privileges.

## Consequences

Some older applications need XWayland. Stage 0 may use X11/Openbox as a
temporary compatibility host for the browser prototype; that does not change
the production compositor decision.
