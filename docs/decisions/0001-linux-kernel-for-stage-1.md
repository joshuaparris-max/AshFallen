# ADR-0001: Linux kernel underneath for the first usable product

**Status:** accepted  
**Date:** 2026-09-18

## Context

The long-term vision includes an independent Josh kernel, and that kernel now
already boots in QEMU. The question is whether a usable desktop should wait for
that kernel to acquire decades of driver, graphics, browser, networking and
power-management work.

Comparable independent operating systems show that hardware enablement and
application ecosystems dominate the schedule.

## Decision

The first daily-usable Josh OS product uses the Linux kernel.

Everything the user experiences — compositor, shell, settings, applications,
visual language, installer and ISO — should increasingly be Josh OS.

The independent Josh kernel remains a parallel research track rather than being
discarded.

## Consequences

**Good:** a usable desktop can exist early and inherit mature hardware support.

**Bad:** some observers will describe the product as "just Linux underneath".
That is an implementation fact, not a reason to hide the dependency.

**Preserved:** the shared Josh application/window concepts are designed so they
can later run over the independent kernel as it matures.
