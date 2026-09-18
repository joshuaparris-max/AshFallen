# Josh OS Product and Architecture Strategy

## North star

Josh OS should become a coherent desktop operating system whose interface, application model, services and low-level architecture feel like parts of one idea.

The aspiration is not to imitate one existing platform. Josh OS should combine:

- Linux's openness, transparency and hardware ecosystem;
- Windows' familiar desktop conventions and pragmatic usability;
- macOS' restraint, consistency and visual cohesion.

The governing principle remains:

> Josh OS should become clearer as you understand more of it.

## The key decision: use two tracks

Josh OS should not force a false choice between a usable desktop soon and a fully independent kernel eventually.

Development therefore has two complementary tracks.

### Product track — usable Josh OS

The product track aims to make Josh OS genuinely useful on ordinary hardware much sooner.

Initial stack:

    Josh applications
           ↓
    Josh desktop shell
           ↓
    Josh compositor / window system
           ↓
    Josh system services
           ↓
    Linux kernel + mature drivers
           ↓
    hardware

Linux is an implementation layer here, not the product identity. The user-facing system should still be Josh OS: its desktop, settings, launcher, dock, window behaviour, application model, visual language and system conventions.

This path gives Josh OS practical access to decades of driver work: USB, Wi-Fi, Bluetooth, graphics, audio, storage, power management, printers and modern laptop hardware.

### Research track — independent Josh kernel

The existing freestanding kernel remains a first-class Josh OS project.

Current stack:

    Josh framebuffer desktop
           ↓
    Josh kernel
           ↓
    Limine bootloader
           ↓
    firmware / hardware

This track develops the underlying operating-system fundamentals independently: memory management, interrupts, scheduling, userspace, filesystems, drivers, networking and eventually a native compositor.

The kernel is not a throwaway demo. It is the long-term route toward owning the whole stack.

## Convergence path

The tracks should converge gradually rather than through a rewrite.

### Josh OS 1.x

    Josh UI
    Josh apps
    Josh desktop/compositor
    Josh services
    Linux kernel

Goal: a polished, installable and useful desktop operating system.

### Josh OS 2.x

    Josh UI
    Josh apps
    Josh desktop/compositor
    More Josh-native services
    Stable Josh application APIs
    Linux kernel

Goal: reduce dependence on conventional Linux userspace while keeping mature hardware support.

### Josh OS 3.x research target

    Josh UI
    Josh apps
    Josh desktop/compositor
    Josh services
    Josh userspace
    Josh kernel

Goal: run the same Josh application and desktop concepts on the independent Josh kernel.

The version numbers describe direction, not promises or dates.

## Preserve one conceptual model

The web/prototype desktop, Linux-backed product desktop and native-kernel desktop should not become three unrelated implementations.

Shared concepts should be specified independently of rendering technology:

- App
- Window
- Surface
- Workspace
- Focus
- Command
- Notification
- File
- Setting
- Theme
- Capability
- Process

For example, a Window should have the same conceptual state whether it is represented by a browser prototype, a Wayland surface or a native Josh compositor surface.

## Single-source design system

The desktop prototype demonstrated the value of defining design tokens once.

Josh OS should maintain an implementation-neutral design specification containing at least:

- colour roles;
- typography;
- spacing;
- corner radii;
- shadows/elevation;
- animation timing;
- window dimensions and constraints;
- dock and menu-bar geometry;
- light/dark behaviour;
- accessibility contrast targets.

Platform-specific generators can translate that source into CSS, compositor constants or native UI resources.

Visual values should not slowly diverge across implementations.

## What success means

Josh OS succeeds when all of these are true:

1. **Beautiful to use.** The desktop is calm, coherent, responsive and understandable.
2. **Beautiful to understand.** The source reflects real system concepts instead of framework accidents.
3. **Useful on real hardware.** The product track works on modern PCs without waiting to reinvent every driver.
4. **Independent in spirit and increasingly in implementation.** Josh-owned layers grow over time.
5. **Observable and testable.** Boot, builds and important behaviours have automated proof.
6. **No fake progress.** Prototype interactions are labelled as prototypes; kernel capabilities are only claimed once they really exist.

## Decision hierarchy

When trade-offs appear, use this order:

1. correctness;
2. conceptual integrity;
3. user experience;
4. simplicity;
5. portability;
6. performance where measured;
7. metrics.

Metrics inform decisions; they do not define good engineering.
