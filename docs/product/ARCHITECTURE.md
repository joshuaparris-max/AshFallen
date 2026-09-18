# Product-track architecture

## Layers and ownership

| Layer | First usable product | Later product | Long-term convergence |
|---|---|---|---|
| Desktop UI | **Josh** | Josh | Josh |
| Window system | Browser prototype → **Josh Wayland compositor** | Josh | Josh |
| System services | Linux services | **Josh where ownership earns its place** | Josh |
| Kernel | Linux | Linux | **Josh kernel research track** |

The rule for replacing a component is simple: own it when doing so buys
conceptual integrity or capability that cannot be cleanly achieved otherwise.

## Shell prototype

`shell/` models the Josh Window System in a browser. Rendering is disposable;
these concepts are not:

- **Window record:** identity, app, title, geometry, state and previous geometry.
- **Focus/stacking:** one coherent source of truth for which window is active.
- **Snap zones:** pointer position maps to left, right or maximise geometry.
- **App registry:** an app declares identity, preferred geometry and how to
  populate its surface without manipulating the window manager directly.

When apps become processes talking to a compositor, the conceptual contract
should stay recognisable even though the implementation changes completely.

## Design tokens

`design/tokens.json` is the source of truth for shared visual language.
`scripts/build-tokens.mjs` currently generates CSS. Future native product code
and native-kernel UI adapters should consume generated values from the same
semantic source.

## Stage 0 live image

The current Linux-backed ISO is intentionally a compatibility vehicle:

    ArchISO
       ↓
    LightDM
       ↓
    Openbox
       ↓
    Chromium kiosk
       ↓
    Josh shell prototype

This is not the production compositor. It exists so the interaction model is
bootable and testable now.

## Non-goals for the first product milestone

- rewriting mature hardware drivers merely for ownership;
- writing a browser engine;
- pretending the Stage 0 Openbox/Chromium host is the final architecture;
- allowing Linux-specific implementation details to become permanent Josh app
  APIs without a good reason.
