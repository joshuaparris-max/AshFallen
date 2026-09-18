# Product-track vision

Josh OS aims to borrow strong ideas without copying any one desktop:

- **macOS** — visual restraint and consistency;
- **Windows** — familiar window management and pragmatic desktop conventions;
- **Linux** — openness, hackability and transparency.

The ambition is not merely "another Linux distro". The Linux kernel is an
initial compatibility layer for the usable product while the independent Josh
kernel develops in parallel.

## Foundational rule

> Josh OS should feel simpler after you understand it, not more complicated.

This applies to both interface and source code.

## Product shape

    Desktop UI
        ↓
    Josh Window System / compositor
        ↓
    Josh system services
        ↓
    Linux kernel initially
        ↓
    hardware

As the native kernel matures, more of this stack can converge onto Josh-owned
infrastructure without forcing the desktop to wait for every driver first.
