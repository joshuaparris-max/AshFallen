# Bootable Josh OS product image

Josh OS has a Stage 0 live-image pipeline. It deliberately uses ArchISO's current
`releng` profile for the low-level BIOS/UEFI boot plumbing, then layers the
Josh OS shell on top.

This is **not** the future Josh compositor and it is distinct from the
independent native-kernel ISO already built by this repository. The product ISO
boots Linux, autologs into a small X11/Openbox compatibility session, and
launches the browser shell in Chromium kiosk mode. That makes the interaction
model testable in VirtualBox now, while ADR-0002 still governs the real product
compositor: Wayland, not X11.

## What boots

1. ArchISO handles BIOS/UEFI boot and the live root filesystem.
2. LightDM autologs in as the disposable `josh` live user.
3. A minimal Openbox host session starts.
4. Chromium opens `file:///opt/josh-os/shell/index.html` full-screen.
5. VirtualBox guest utilities are included for better VM integration.

The live user has passwordless sudo because this image is a development image,
not a security boundary or production installer.

## Build on Arch Linux

```sh
sudo pacman -S archiso
sudo bash ./scripts/build-iso.sh
```

The result is written to `out/josh-os-*.iso`.

## Build in GitHub Actions

The **Build Josh OS Product ISO** workflow builds inside a privileged Arch Linux
container and uploads `JoshOS-Stage0-Live-x86_64` as a workflow artifact. It
also publishes a `SHA256SUMS` file beside the ISO.

## Developer Mode

The product image includes an opt-in real terminal and the `josh-dev` command.
Press **Ctrl+Alt+T** or **Super+Enter**, then use `josh-dev enable` to begin.
Developer Mode can install the build/QEMU toolchain plus Codex CLI, Claude Code
and Antigravity CLI, clone the three Josh repositories, run an agent in an
isolated Git worktree, verify the result and build a candidate product ISO.

It does not replace the running OS or automatically merge/publish AI changes.
On a live image, use persistent storage for the Developer Mode workspace unless
you deliberately opt into an ephemeral session. See
[Developer Mode](../docs/developer-mode.md).

## VirtualBox

Create a VM with:

- Type: Linux / Arch Linux (64-bit), or Other Linux (64-bit)
- RAM: 4096 MB
- CPUs: 2
- Graphics controller: VMSVGA
- Video memory: 128 MB
- 3D acceleration: off for the first boot
- Optical drive: attach `josh-os-*.iso`
- Secure Boot: off

BIOS and UEFI are both inherited from ArchISO's releng profile. Start with BIOS
for the simplest first test; UEFI can be enabled afterwards.

Expected result: after the normal boot sequence, Josh OS fills the display and
the browser chrome is hidden. Chromium is automatically relaunched if it exits.

## Honest boundary

A successful ISO build proves the image is structurally buildable. It does not
prove every VirtualBox host/graphics combination boots correctly. The roadmap
keeps a separate manual VirtualBox boot check for that reason.
