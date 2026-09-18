# Josh OS product experience roadmap

This roadmap covers the visible journey from the first Josh-branded pixel during boot through first-run, everyday desktop use, failure and recovery.

The product should feel like one system even while different technical layers mature at different speeds.

## One visual language, multiple stages

There may eventually be three visually related screens:

1. **Firmware splash** — JoshBIOS, before the OS loader.
2. **OS boot splash** — kernel/userspace startup.
3. **First-run / login / desktop** — Josh OS product.

They should share typography, spacing, logo treatment and colour roles without pretending they are the same program.

The design-token source should eventually generate constrained assets/constants for firmware, native boot UI and desktop use.

---

## P0 — truthful startup

Before adding polish:

- [ ] every visible progress state corresponds to a real system milestone;
- [ ] logs are accessible with a key/action;
- [ ] startup failures transition to diagnostics/recovery rather than freezing;
- [ ] firmware, bootloader, kernel and userspace versions are available in diagnostics;
- [ ] no fake progress bar.

Suggested real milestones:

- Firmware ready
- Loading Josh OS
- Kernel started
- Devices ready
- System services ready
- Desktop ready

---

## P1 — firmware start screen

Owned by JoshBIOS, not the desktop.

Normal screen:

- Josh mark;
- build/version in a subtle location;
- “Starting Josh OS” or equivalent;
- minimal key hints.

Actions:

- firmware setup;
- one-time boot menu;
- recovery/diagnostics.

The normal path should be short enough that the splash feels like acknowledgement, not a delay.

---

## P2 — bootloader menu

Normally hidden or near-instant.

Reveal when:

- user requests it;
- default boot fails repeatedly;
- an update is pending validation;
- recovery media is detected where policy permits.

Initial entries:

- Josh OS;
- Previous known-good;
- Recovery;
- Diagnostics.

Later:

- development kernel;
- alternate installed OS;
- external/removable entry.

Use simple keyboard navigation first. Mouse support is unnecessary at this layer.

---

## P3 — OS boot splash

Once the native kernel can draw reliably:

- [ ] render from a small implementation-independent boot theme;
- [ ] show real milestone text;
- [ ] reveal logs/serial-equivalent output on command;
- [ ] display recovery prompt after detected failed boot;
- [ ] make headless/serial boot remain fully supported.

The Linux-backed product track should mimic the same milestone vocabulary so users do not experience two unrelated products.

---

## P4 — session model

Decide explicitly:

### Single-owner development mode

Suitable early on:

- local device;
- one owner;
- direct login;
- lock screen later;
- no decorative multi-user UI.

### Full account/session model

Only when backed by real capabilities:

- identities;
- credentials;
- user data separation;
- per-user settings;
- permissions;
- lock/unlock;
- session switching if desired.

Do not build a polished login screen before the security model exists.

---

## P5 — first-run experience

The first-run flow should be short, resumable and idempotent.

Candidate sequence:

1. language and keyboard;
2. display scale and accessibility essentials;
3. network;
4. device name;
5. account setup when real accounts exist;
6. update preference;
7. privacy choices that map to actual features;
8. welcome hand-off to desktop.

Principles:

- one clear decision per screen;
- safe defaults;
- no mandatory online account unless the architecture genuinely requires it;
- skip/defer wherever safe;
- restarting halfway through resumes cleanly;
- all settings remain changeable later.

---

## P6 — desktop fundamentals

The browser prototype already proves useful concepts. Promote them carefully:

- [ ] canonical App model;
- [ ] canonical Window model;
- [ ] focus/stacking;
- [ ] move/resize/minimise/maximise;
- [ ] snapping;
- [ ] workspaces if they solve a real need;
- [ ] launcher / command palette;
- [ ] dock;
- [ ] status/menu area;
- [ ] notifications;
- [ ] Settings;
- [ ] Files;
- [ ] Terminal;
- [ ] Text Editor.

Before adding many apps, make the shared window/application APIs stable enough that apps are not coupled to one compositor implementation.

---

## P7 — accessibility from architecture

Build in:

- full keyboard operation;
- visible focus;
- logical tab/focus order;
- screen-reader/accessibility tree strategy;
- scalable text/UI;
- contrast targets;
- reduced-motion mode;
- input remapping;
- high-contrast option only if it is maintained as a real design mode.

Accessibility should share semantic information with automation/testing rather than being a separate bolt-on tree.

---

## P8 — settings

Settings should reflect real ownership.

Sections can grow as capabilities become real:

- Appearance
- Displays
- Keyboard and mouse
- Network
- Bluetooth
- Sound
- Storage
- Power
- Accounts
- Applications
- Privacy/security
- Updates
- About
- Recovery

A setting that cannot take effect must not appear as a fake toggle.

---

## P9 — updates

User experience requirements:

- show what layer is updating;
- distinguish firmware/bootloader/kernel/base OS/apps;
- download before reboot where possible;
- preserve previous-known-good;
- clear restart requirement;
- progress based on real work;
- interruption-safe design;
- understandable rollback.

The user should never need to know which partition holds the rollback image just to recover.

---

## P10 — failure and recovery UX

Recovery should feel like part of Josh OS rather than a scary hidden technician screen.

Recovery surface should eventually offer:

- Start Josh OS normally
- Start previous version
- Repair startup
- View diagnostics
- Safe/rescue shell
- Reinstall/repair while preserving data where supported
- Firmware recovery link/action when the failure is below the OS

Always show the exact build/version being recovered.

---

## P11 — diagnostics

A user-facing diagnostics app should collect:

- firmware build;
- bootloader build;
- kernel build;
- OS build;
- boot duration;
- last boot failure;
- CPU/RAM;
- display;
- storage;
- network devices;
- relevant logs.

Provide an export bundle with privacy review/redaction rules before sharing.

---

## P12 — performance perception

Optimise based on real milestones:

- time to first firmware frame;
- firmware-to-bootloader;
- bootloader-to-kernel;
- kernel-to-services;
- services-to-desktop;
- desktop-to-interactive.

A visually smooth animation cannot compensate for a 15-second unexplained stall. Measure stages separately.

---

## P13 — shutdown/restart/suspend experience

The shell should expose:

- Lock
- Sign out when multi-user/session semantics exist
- Sleep
- Restart
- Shut down
- Restart to firmware/setup
- Restart to recovery

Only show actions supported by the current platform.

---

## P14 — product-track/native-track convergence

The Linux-backed product and native Josh kernel should share:

- App definitions;
- Window semantics;
- Settings schema;
- notification model;
- commands/actions;
- design tokens;
- accessibility semantics;
- update/recovery vocabulary.

They do **not** need to share implementation code everywhere.

Success means an app concept survives the transition from browser prototype → Wayland/Linux → native Josh userspace without being conceptually reinvented.
