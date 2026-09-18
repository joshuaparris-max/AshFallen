# Product-track roadmap

Each stage should produce something testable rather than a pile of scaffolding.

## Stage 0 — shell prototype and live ISO

- [x] movable/resizable windows
- [x] focus, minimise and maximise
- [x] edge snapping with preview
- [x] dock, menu bar and clock
- [x] design token pipeline
- [x] light/dark themes, accent colours and wallpapers
- [x] notifications
- [x] About, Files, Terminal, Editor and Settings prototypes
- [x] ArchISO build pipeline
- [ ] keyboard-driven window management
- [ ] global launcher / command palette
- [ ] tiling beyond halves
- [ ] manual VirtualBox smoke test of the generated ISO

## Stage 1 — Josh OS 1.x: real desktop, Linux underneath

- [x] choose a Linux-backed compatibility path for the first usable product
- [ ] spike both Smithay and wlroots before choosing
- [ ] implement a Wayland compositor
- [ ] port the shared window model
- [ ] real panel, dock, launcher and notification surfaces
- [ ] Settings backed by real system services
- [ ] installer
- [ ] application packaging/update story
- [ ] accessibility and keyboard-navigation pass

## Stage 2 — own more userland where it helps

- [ ] Josh file-service/API model
- [ ] Josh session/device concepts
- [ ] Josh application manifest/capability model
- [ ] stable app-facing APIs insulated from Linux implementation details

## Stage 3 — converge with the native Josh kernel

The independent kernel already boots and produces its own ISO. The long-term
goal is to make Josh applications, window concepts and system APIs portable
enough to run there as the kernel gains processes, filesystems, drivers and
services.

The kernel track is real engineering, but the usable desktop does not wait for
it to reinvent every modern PC driver.
