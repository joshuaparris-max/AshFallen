# ADR-0003: One token file for every surface

**Status:** accepted  
**Date:** 2026-09-18

## Context

The shell prototype is web-based. The real compositor will be native. The
independent kernel has its own renderer. Separate codebases describing the same
visual language will drift unless the shared design decisions have one source
of truth.

## Decision

`design/tokens.json` is the canonical source for colour, spacing, typography,
radius, motion and desktop chrome dimensions.

The web prototype consumes generated CSS. Future native/product code should
consume generated constants/resources from the same semantic token source.

## Consequences

Visual changes become deliberate, reviewable changes. Platform adapters may
encode the tokens differently, but should not invent independent values without
an explicit architectural reason.
