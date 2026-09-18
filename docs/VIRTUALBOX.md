# VirtualBox smoke test

This is the manual acceptance test for the Stage 0 Linux-backed product ISO.

## VM settings

Use 2 CPUs, 4096 MB RAM, VMSVGA, 128 MB video memory and no 3D acceleration for
the first run. Mount the generated `josh-os-*.iso` as the optical disk. BIOS is
the first-path test; UEFI is the second-path test.

## Pass criteria

The image passes when all of these are true:

- it reaches a graphical session without asking for a login;
- the Josh OS shell fills the display with no Chromium toolbar;
- About, Files, Terminal, Editor and Settings open;
- windows move, resize, minimise, maximise and snap;
- the live clock updates;
- light/dark switching works;
- resizing the VirtualBox window does not make the shell unusable;
- after closing Chromium, the shell returns automatically within a few seconds;
- Ctrl+Alt+F2 still provides an escape path for diagnostics.

Do not mark this manual VM milestone complete merely because CI produced an ISO.
