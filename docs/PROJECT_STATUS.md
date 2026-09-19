# Josh OS Project Status — Integration Ground Truth

**Audit date:** 2026-09-19  
**Integration owner:** Agent 1  
**Authority:** source code + GitHub branch topology + GitHub Actions evidence. README checkboxes and agent reports are not proof.

> **Green-language rule:** Never write “main is green” without naming the workflow and the run ID. Different workflows can disagree on the same main SHA.

> **Publication note:** the branch-distance audit below is frozen against the named audit SHAs. Agent 1's subsequent documentation-only commits moved JoshBIOS main to `023ecf7` (normal `CI` run `35415855376` green) and canonical JoshOS main to `34cea48`; these documentation commits do not change the audited boot/kernel/product implementation.

## Decisions locked in this round

- Canonical OS repository: `joshuaparris-max/JoshOS` (same repository that was previously named `joshuaparris-max/AshFallen`).
- Stage 0 remains `Parris-Tech-Services/JoshOS-Stage0`; it is a rapid Linux-backed product/UX vehicle, not a second canonical OS.
- Firmware/BIOS/bootloader remains `Parris-Tech-Services/JoshBIOS`.
- Default product browser: **Firefox**.
- Low-spec product reference floor: **Intel Gen5 Ironlake or newer**, approximately first-generation Core i3-era hardware with **2 GiB RAM minimum / 4 GiB preferred**.
- DadLAN Laptop #10 / **Compaq 610 remains a boot-path and firmware target only**, not the product performance/browser reference machine.
- Developer Mode is **optional post-install tooling**, not part of the low-spec base image.

## Repository ownership map

| Repository | Owns | Does not own |
|---|---|---|
| `Parris-Tech-Services/JoshBIOS` | JoshFirmware research, BIOS/UEFI adapters, Stage1/Stage2/JoshBootloader, boot UX, recovery plumbing, Josh Boot Protocol handoff | Canonical OS kernel or desktop |
| `joshuaparris-max/JoshOS` | Canonical Josh OS product integration + independent x86-64 Josh kernel | Board firmware |
| `Parris-Tech-Services/JoshOS-Stage0` | Rapid Linux-backed UX/live-image experimentation | Canonical native kernel or firmware |

## Ground-truth branch audit

### JoshBIOS

Current main audit point: `54778be4127a` (`rename: update canonical kernel references to JoshOS`).

Workflow truth at this audit point:

| Main SHA | Workflow | Run | Status |
|---|---|---:|---|
| `54778be4127a` | `CI` | `35414145264` | **green** |
| `54778be4127a` | `Coreboot QEMU Integration` | `35414145250` | **red** — coreboot and SeaBIOS start, but JoshBootloader is not reached before timeout; `JOSHBOOT_PARTITION_OK` is missing |

Therefore the repo must **not** be described simply as “main is green”.

#### Open PRs

| PR | Branch | Head | Ahead/behind main | Head CI | Applies cleanly? | Integration disposition |
|---|---|---:|---:|---|---|---|
| #1 | `agent/boot-storage-core` | `96e558ee0d64` | +9 / -96 | green (`35341970419`) | No (`mergeable=false`) | **SALVAGE ONLY**; functionality already moved on substantially |
| #3 | `agent/elf64-load-plan` | `07e52159cbff` | +4 / -86 | green (`35342397153`) | No | **SALVAGE ONLY** |
| #4 | `agent/elf64-loader` | `e6d3c3d5fb7b` | +13 / -96 | green (`35341928304`) | No | **SALVAGE ONLY** |

#### Agent branches without open PRs

| Branch | Head | Ahead/behind main | Head CI | Applies cleanly? | Disposition |
|---|---:|---:|---|---|---|
| `agent/boot-storage-rebased` | `026e99a21eb8` | +0 / -80 | green (`35342275697`) | N/A: no unique commits | Superseded |
| `agent/live-fat32-kernel` | `e16e78ed0319` | +3 / -59 | green (`35344604642`) | Not proven; no PR mergeability result | **SALVAGE ONLY** |
| `integration/fat32-boot` | `1047bd8f67c9` | +6 / -69 | red (`35343637180`) | Not proven; no PR mergeability result | **SALVAGE ONLY** |
| `verify/claude-boot-stack` | `7a51f0630342` | +17 / -93 | green (`35344717206`) | Not proven; no PR mergeability result | **SALVAGE ONLY** |

Every unmerged JoshBIOS branch is now more than 20 commits behind current main. None should be merged wholesale.

### Canonical JoshOS

Canonical repo correction: the OS repository formerly called `joshuaparris-max/AshFallen` is now **`joshuaparris-max/JoshOS`**. A separate `joshualparris/AshFallen` Vite/game repository exists and is not the OS.

Current main audit point in this draft: `6d25431af643` (`fix(iso): identify live image as Josh OS`). Multiple direct-to-main product commits arrived during the integration round, so all branch distances below are frozen against this SHA.

Workflow truth:

| Main content / SHA | Workflow | Run | Status |
|---|---|---:|---|
| Native-kernel content at `db3abd5b6643` (unchanged by later docs/shell/ISO commits at this snapshot) | `Build Josh OS Native Kernel` | `35411586180` | **green** — requires scheduler, heap, PCI, E1000, ACPI/APIC/IOAPIC/SMP/timer/keyboard and `JOSHOS_BOOT_OK` |
| Storage-core content at `5d49afaee799` | `Native Storage Core Tests` | `35411583606` | **green** |
| `6d25431af643` | `Build Josh OS Product ISO` | `35415431739` | **red** — product browser pass 1 timed out before browser readiness markers |
| `90fe0f5fac7d` | `Build Josh OS Product ISO` | `35414499841` | **red** — two-boot browser smoke timed out before product browser markers |

Therefore “JoshOS main is green” is ambiguous and prohibited.

Latest product ISO evidence: run `35415431739` completed **red** after the ISO built successfully but `scripts/smoke-product-browser.sh` timed out on pass 1 while serial output was still at the BIOS live-image boot/kernel-load boundary. Treat the Linux product/browser path as **Implemented-unverified**, not merged+green.

#### Open PRs

| PR | Branch | Head | Ahead/behind main | Head CI | Applies cleanly? | Disposition |
|---|---|---:|---:|---|---|---|
| #27 | `agent/userspace-elf-current` | `3c633ec19abd` | +4 / -17 | green (`35411174570`) | No (`mergeable=false`) | **NEAREST MERGE CANDIDATE**; refresh/re-cut on current main |
| #26 | `agent/process-ring3-current` | `a58fdc222b8a` | +17 / -17 | green (`35411134369`) | No (`mergeable=false`) | **NEAREST MERGE CANDIDATE**; refresh/re-cut on current main |
| #25 | `feature/developer-mode-self-build` | `a4c2e00b82ff` | +16 / -20 | red (`35411101443`) | No | Re-cut later as optional package, not base image |
| #24 | `agent/userspace-elf` | `f31f2cfc5826` | +6 / -74 | no head run | No | **SALVAGE ONLY** |
| #22 | `hardware/storage-sources` | `b63ff6579f17` | +1 / -88 | green (`35397533936`, `35397533866`) | No | **SALVAGE ONLY**; cleanest tested storage-source slice |
| #21 | `kernel/native-storage-services-v3` | `f5e641781310` | +27 / -113 | no head run | No | **SALVAGE ONLY**; furthest storage functionality |
| #20 | `agent/process-ring3` | `f0b09296fc27` | +22 / -125 | green (`35397483157`) | Only to stacked base `agent/process-scheduler`, not current main | **SALVAGE ONLY** |
| #19 | `agent/process-scheduler` | `7d1d21a5250a` | +5 / -125 | green (`35397232719`) | No | **SALVAGE ONLY** |
| #17 | `kernel/native-storage-services-v2` | `670f66491f16` | +27 / -150 | no head run | No | **SALVAGE ONLY** |
| #15 | `hardware/storage-device-foundation` | `085f88ab14ed` | +1 / -115 | no head run | No | **SALVAGE ONLY** |
| #13 | `agent/kernel-paging` | `569f7e48863c` | +5 / -199 | red (`35347168179`) | No | **SALVAGE ONLY** |
| #11 | `kernel/process-userspace-v2` | `f9ae7b40f582` | +37 / -218 | no head run | No | **SALVAGE ONLY** |
| #7 | `storage/pci-discovery` | `4434fb8cb96d` | +2 / -200 | no head run | No | **SALVAGE ONLY** |

Important historical correction: #26 and #27 were 12 commits behind and mergeable at an earlier main snapshot. Current main advanced during this integration round; they are now 17 behind and GitHub reports them non-mergeable. They remain the closest green candidates, but they must be refreshed before merge.

#### Agent branches without open PRs

For non-PR branches GitHub does not expose a PR `mergeable` result. “Not proven”
below is deliberate; no clean-merge claim is inferred from a compare graph.

| Branch | Head | Ahead/behind main | Head CI | Applies cleanly? | Disposition |
|---|---:|---:|---|---|---|
| `agent/kernel-foundations` | `176cb7d1dab0` | +15 / -224 | green (`35346431506`) | Not proven | **SALVAGE ONLY** |
| `agent/kernel-heap` | `79057e80127b` | +10 / -199 | none | Not proven | **SALVAGE ONLY** |
| `agent/process-runtime` | `31fec9937494` | +28 / -17 | none | Not proven | Near-main source; re-cut + CI before consideration |
| `antigravity-push` | `58f6deee997c` | +0 / -329 | none | N/A: no unique commits | Superseded |
| `feature/repo-rename-joshos` | `61bc871ee32c` | +5 / -4 | none | Not proven | Docs/integration cleanup only; reconcile |
| `feature/repo-rename-joshos-cleanup` | `308db7030422` | +1 / -3 | none | Not proven | Docs/integration cleanup only; reconcile |
| `kernel/k1-double-fault-test` | `e38ce1d4b819` | +7 / -239 | green (`35342340173`) | Not proven | **SALVAGE ONLY** |
| `kernel/k1-exception-idt` | `afbeb6406a89` | +9 / -244 | none | Not proven | **SALVAGE ONLY** |
| `kernel/k1-exception-idt-v2` | `beeddf87f0bc` | +11 / -243 | green (`35341374080`) | Not proven | **SALVAGE ONLY** |
| `kernel/k1-exception-idt-v3` | `6be2bfae98e4` | +9 / -241 | green (`35341600466`) | Not proven | **SALVAGE ONLY** |
| `kernel/k1-fault-matrix` | `3d2f03b444ad` | +2 / -238 | none | Not proven | **SALVAGE ONLY** |
| `kernel/k1-gdt-tss-ist` | `aefe1f643501` | +6 / -240 | green (`35342038779`) | Not proven | **SALVAGE ONLY** |
| `kernel/k1-register-frame` | `caa03786565f` | +7 / -238 | green (`35344401543`) | Not proven | **SALVAGE ONLY** |
| `kernel/native-storage-services` | `4e2bb5e7818d` | +23 / -179 | none | Not proven | **SALVAGE ONLY** |
| `kernel/process-userspace-slice` | `f0aa2d496344` | +28 / -237 | none | Not proven | **SALVAGE ONLY** |
| `kernel/storage-core-v1` | `852aa6049406` | +16 / -72 | green (`35397760272`, `35397760238`) | Not proven | **SALVAGE ONLY** |
| `kernel/storage-pci-ahci-v1` | `f93f68b860c1` | +0 / -41 | cancelled (`35397965920`) | N/A: no unique commits | Superseded |


### JoshOS-Stage0

PR #4 `feature/stage0-ci-fast-qemu-boot` reached green run `35413006500` at
`224320b`, then was merged as `9c467a1`. Main then passed **Build Josh OS ISO**
run `35413957824` on that merge.

The exact diagnosed failure was real: PR run `35411090421` contained
`archiso login: JOSHOS_NETWORK_READY`, while the harness required
`^JOSHOS_NETWORK_READY`. Commit `224320b` checks the failure token first and
accepts the success token anywhere on the serial line.

PR #3 (real Wayland session ADR) was subsequently green on branch run
`35414127385` and merged as `ac0cee4d3bea`.

| Main SHA | Workflow | Run | Status |
|---|---|---:|---|
| `9c467a1fb504` | `Build Josh OS ISO` | `35413957824` | **green** after PR #4 |
| `ac0cee4d3bea` | `Build Josh OS ISO` | `35415425150` | **green** after PR #3 |

#### Open PRs

| PR | Branch | Head | Ahead/behind main | Head CI | Applies cleanly? | Disposition |
|---|---|---:|---:|---|---|---|
| #6 | `integration/locked-decisions` | `6a6ef725e952` | +1 / -0 | in progress (`35415911731`) | Yes (`mergeable=true`) | Integration docs only; merge after green head run |
| #5 | `feature/repo-rename-joshos` | `10402a8ca173` | +6 / -6 | green (`35414107536`) | No (`mergeable=false`) | Re-cut docs-only rename pieces after ADR/status integration; do not merge wholesale |

#### Agent branches without open PRs

| Branch | Head | Ahead/behind main | Head CI | Applies cleanly? | Disposition |
|---|---:|---:|---|---|---|
| `feature/stage0-adr-real-wayland-session` | `f4064beba5ff` | +0 / -7 | green (`35414127385`) | N/A: already merged | Preserve branch |
| `feature/stage0-ci-fast-qemu-boot` | `224320b77df9` | +0 / -7 | green (`35413006500`) | N/A: already merged | Preserve branch |
| `feature/stage0-ci-network-smoke` | `4df222b517eb` | +2 / -13 | green (`35409121413`) | Not proven; no open PR | Superseded by PR #4; salvage only |
| `step0-baseline-ci` | `8ead7d93cd5b` | +4 / -13 | red (`35409956495`) | Not proven; no open PR | Superseded; preserve, do not merge |

## Duplicate-work resolution

### Stage0 Step 0

**Canonical result:** PR #4 / `feature/stage0-ci-fast-qemu-boot`, now merged.

Preserve but do not merge `feature/stage0-ci-network-smoke` and `step0-baseline-ci`. Any unique assertion can be manually re-applied to current main only if it adds evidence that PR #4 does not already provide.

### Native storage / PCI / AHCI

No existing storage branch is eligible for wholesale merge; every one is more than 20 commits behind current main.

- **Furthest functionality:** `kernel/native-storage-services-v3` (#21), but it is stale and has no current head CI.
- **Cleanest tested source slice:** `hardware/storage-sources` (#22), green when tested but still far behind current main.
- Current main already contains substantive block/cache/VFS/tmpfs/storage-service/PCI code with green storage host tests.

Next storage work must start on a **fresh current-main branch**, first inventory what main already contains, then selectively port only still-missing AHCI/device/runtime pieces from #22/#21 with new tests. Close #7/#15/#17/#21/#22 after their useful commits are recorded in the re-cut PR; preserve the branches.

### Browser acceptance

There is no live browser feature branch to merge. The extensive Chromium harness is already an ancestor of canonical JoshOS main (notably commits `8e864d1`, `0496ab0`, `be5a194`, `f31e0e0`).

Firefox is now the product decision. Salvage the browser-neutral contract:
- persistent profile/home across reboot;
- deterministic media-playback progression;
- PipeWire/Pulse sink-input evidence;
- crash/relaunch/session recovery;
- download creation/content/persistence;
- network/TLS preflight;
- secret-store integration;
- human-assisted YouTube sign-in/audio/reboot retention.

Replace Chromium-specific CDP, policies, cookie paths, singleton locks, remote-debug flags and launcher switches rather than carrying them forward as product requirements.

## Exclusive workstream ownership

GitHub authorship cannot identify which of the thirteen chat agents produced a commit: all pushes use the same account. To avoid inventing identity, this document assigns one **exclusive owner role** per area. One agent may claim a role; two agents must not work the same role in parallel.

| Area | Exclusive owner |
|---|---|
| Ground truth, ADRs, merge queue, this document | **Agent 1 — Integration** |
| JoshFirmware/JoshBIOS/JoshBootloader | **Boot Stack Owner** |
| Stage0 labwc/greetd/installer session | **Stage0 Session Owner** |
| Firefox packaging + browser acceptance adaptation | **Firefox Owner** |
| Native memory/interrupt/APIC/timer/SMP foundations | **Kernel Foundations Owner** |
| Native scheduler/processes/Ring3/syscalls/userspace ELF | **Process/Userspace Owner** |
| Native PCI/block/AHCI/VFS/storage services | **Storage Owner** |
| Native Ethernet/IP/DHCP/DNS/TLS progression | **Native Networking Owner** |
| Linux product network/audio/session services | **Product Services Owner** |
| Future Josh-owned compositor | **Native Compositor Owner** |
| Optional Developer Mode package | **Developer Tools Owner** |
| Compaq 610 + Gen5 physical acceptance | **Hardware Validation Owner** |
| CI quality, CRAP/coverage/static-analysis gates | **Quality Owner** |

## Capability status

Only the three labels below are used.

| Area | Status | Evidence / limitation |
|---|---|---|
| Legacy-BIOS JoshBootloader → canonical JoshOS | **Merged+green** | JoshBIOS normal CI; FAT32/ELF64/long-mode/Josh protocol QEMU path. Current normal CI run is named in the repository audit. Physical hardware not claimed. |
| UEFI JoshBootloader → canonical JoshOS | **Merged+green** | OVMF path previously reached `JOSHOS_BOOT_OK`; current JoshBIOS normal CI continues to build/test UEFI. Physical UEFI hardware unverified. |
| coreboot virtual firmware path | **Implemented-unverified** | Dedicated run `35414145250` is red: coreboot→SeaBIOS starts, but JoshBootloader is not reached before timeout. |
| Compaq 610 physical firmware/coreboot | **Not started** | Read-only/external-programmer preflight tooling exists, but no target capture, recovery proof or physical coreboot boot exists. |
| Native kernel foundations | **Merged+green** | Native run `35411586180` verified scheduler, heap, PCI, E1000, ACPI/APIC/IOAPIC, SMP, timer, IRQ keyboard and `JOSHOS_BOOT_OK`. |
| Native storage core/VFS services | **Merged+green** | Main contains block/cache/VFS/tmpfs/storage-service code; storage host run `35411583606` green. This does not mean AHCI/NVMe device I/O is merged. |
| Native network primitives + e1000 bring-up | **Merged+green** | Native run `35411586180` requires `JOSHOS_E1000_OK` and loopback; host-tested Ethernet/ARP/IPv4/ICMP/UDP/DHCP/DNS/TCP wire primitives exist. A live DHCP/DNS/TCP stack is not claimed. |
| Native live TCP/IP stack | **Not started** | No merged ARP cache/routing state, live UDP endpoints, DHCP state machine, resolver transport or TCP connection state machine. |
| Native AHCI/NVMe disk I/O | **Implemented-unverified** | Work exists only on stale storage branches; must be re-cut. |
| Ring 3/process isolation/syscalls | **Implemented-unverified** | #26 green on its head but currently non-mergeable/behind current main. |
| Userspace ELF validation | **Implemented-unverified** | #27 green on its head but currently non-mergeable/behind current main. |
| Native full userspace/init/services | **Not started** | No merged general process/init/service system. |
| Stage0 live ISO network smoke | **Merged+green** | PR #4 head run `35413006500` green; post-merge main run `35413957824` green; post-ADR main run `35415425150` also green. |
| Real Stage0 Wayland session | **Not started** | ADR selects labwc/greetd/XWayland/Waybar/Fuzzel; implementation not yet landed. |
| Firefox base browser | **Not started** | Decision made; existing image is still Chromium-based until migration lands. |
| Browser persistence/media/audio/download/crash harness | **Implemented-unverified** | Substantive Chromium implementation is merged, but the contract must be adapted to Firefox and the current Product ISO workflow is not green yet. |
| Native Josh compositor | **Not started** | Planning only; framebuffer desktop is not a compositor. |
| Developer Mode | **Implemented-unverified** | PR #25 exists, red CI, and policy changes it to optional post-install. Code inspection shows it downloads Codex/Claude/Antigravity only on explicit `install-agents`; they are not baked into the current ISO. |
| Gen5 low-spec physical acceptance | **Not started** | Reference class decided; exact machine and physical Firefox/VA-API/YouTube evidence still required. |

## Deferred work

- Physical JoshFirmware/coreboot flashing until exact-board identification, three matching external SPI reads and proven OEM restore.
- Broader physical-support matrix until exact-machine tests exist.
- Full native USB, Wi-Fi, audio and accelerated graphics stacks.
- Native browser/runtime on the Josh kernel.
- A/B automatic OS self-update/promotion.
- Optional Developer Mode package until the Firefox/base-image transition settles.
- Final Josh-owned compositor while Stage0 proves product UX on labwc.

## Explicit non-goals

- Compaq 610 as the low-spec browser/product-performance reference.
- Chromium as the default Josh OS browser.
- Three AI coding CLIs in the low-spec base image.
- Treating Stage0 as a second canonical Josh OS.
- Growing JoshBIOS's test kernel into another OS.
- Calling framebuffer scene drawing a compositor.
- Calling one QEMU or one physical boot generic PC support.
- Flashing experimental firmware without external recovery.

## Corrections to existing README/roadmap claims

This section is authoritative until those documents are edited.

| Document/claim | Ground truth | Fix |
|---|---|---|
| Stage0 README/roadmaps point at `joshuaparris-max/AshFallen` as canonical | Repository ID `1211047010` is now named `joshuaparris-max/JoshOS` | Replace old canonical links/names; do not confuse with unrelated `joshualparris/AshFallen` |
| JoshOS README says the native build lacks filesystem/networking wholesale | Main now contains tested block/cache/VFS/tmpfs services, e1000 bring-up and host-tested network wire primitives | Replace the blanket wording with the real boundary: no integrated AHCI/NVMe filesystem persistence and no live DHCP/DNS/TCP stack yet |
| JoshOS README says JoshBootloader UEFI is scaffold-only | Full OVMF loader path was implemented and integration-tested to canonical kernel `JOSHOS_BOOT_OK` | Say UEFI is QEMU/OVMF integration-tested; physical UEFI remains unverified |
| JoshBIOS README says UEFI only emits `JOSHUEFI_ENTRY_OK` and does not load JoshOS | Same stale limitation | Update to current OVMF ELF/GOP/memory-map/ExitBootServices/Josh Boot Protocol handoff evidence |
| JoshBIOS ROADMAP and BOOTLOADER_ROADMAP leave UEFI adapter/memory map/ExitBootServices unchecked or call entry-only | Stale after the UEFI integration work | Mark QEMU/OVMF pieces tested; leave physical UEFI/support unchecked |
| Stage0 roadmap leaves automated QEMU boot smoke unchecked | PR #4 now has a green real-ISO QEMU smoke and is merged; post-merge main run is the final gate | Mark complete only after the named main run is green |
| Stage0 `compositor/README.md` frames Smithay-vs-wlroots as the immediate Stage0 compositor choice | ADR 0002 selects labwc for the Linux-backed Stage0 session; the future Josh-owned native compositor remains a separate decision | Mark the Stage0 compositor plan as superseded for the Linux product path; keep native-compositor research in canonical JoshOS |
| Stage0 ISO/README presents Chromium/Openbox as the forward browser/session choice | It describes current implementation, but ADR 0002 and 0004 supersede it as architecture | Label it legacy compatibility implementation pending labwc + Firefox migration |
| JoshOS product roadmap says compositor toolkit/base should still be spiked before choosing | Stage0 decision is labwc/wlroots for the Linux-backed session; final native Josh compositor is still undecided | Split Stage0 integration choice from future native-compositor choice |
| Earlier integration report said #26/#27 were 12 behind and mergeable | Main advanced during this round; they are now 17 behind and GitHub reports non-mergeable | Keep them as nearest green candidates, but refresh/re-cut before merge |
| Agent report said Developer Mode installs three AI CLIs into the OS image | PR #25 installs them only when `josh-dev install-agents` is explicitly run | Still move Developer Mode helper/config into optional post-install package for low-spec base |
| “JoshBIOS main is green” | Normal CI can be green while Coreboot QEMU Integration is red/in-progress | Always name workflow + run ID |

## Sequenced merge/re-cut plan

1. **Stage0 PR #4 matcher/harness** — merged and post-merge green (`Build Josh OS ISO` run `35413957824`).
2. **Stage0 ADR #3 / real-Wayland decision** — merged as `ac0cee4`; post-merge `Build Josh OS ISO` run `35415425150` is green.
3. **Stage0 ADR integration PR #6** — prepared at `6a6ef72`; ADR numbering is unique (0002 real Wayland, 0003 low-spec Gen5, 0004 Firefox, 0005 design tokens). Head run `35415911731` must be green before merge. No feature code.
4. **Shared status document + README links** — land this document in canonical JoshOS and link it from all three READMEs.
5. **Stage0 repo-rename PR #5** — because it overlaps README/roadmap/build-script text, re-cut it on top of the integration/doc commits rather than racing those files.
6. **JoshOS #26 and #27** — refresh separately from current main. They are the nearest green candidates. #26 and `agent/process-runtime` collide heavily; one Process/Userspace Owner must integrate them serially. #27 is mostly isolated to the userspace ELF parser but should be coordinated with the same owner because `agent/process-runtime` contains a competing copy.
7. **Storage re-cut** — start from current main. Inventory existing block/cache/VFS/tmpfs/PCI first. Salvage missing tested source pieces from #22, then selectively salvage runtime/AHCI work from #21. Do not merge any old storage branch wholesale. Close duplicate storage PRs only after useful commits are accounted for.
8. **Firefox migration** — adapt the existing browser-neutral acceptance contract first, then switch packaging/launcher/profile paths. This work owns browser/session files while active.
9. **Developer Mode optional package** — re-cut #25 only after Firefox migration because both touch ISO packages/session/build scripts. Do not work these in parallel.
10. **JoshBIOS Coreboot QEMU** — fix until its named workflow is green; do not let a green normal CI hide a red firmware workflow.
11. **Physical work** — Compaq USB/firmware evidence only after recovery gates; Gen5 product reference exact machine selection and Firefox/VA-API/audio/YouTube acceptance separately.

### Collision map

- `agent/process-ring3-current` ↔ `agent/process-runtime`: heavy overlap in GDT, interrupts, paging, main, syscall and userspace files. **Serialize.**
- `agent/userspace-elf-current` ↔ `agent/process-runtime`: duplicate user ELF implementation. **Serialize under Process/Userspace Owner.**
- All storage PRs/branches: duplicate PCI/block/AHCI/VFS areas. **One fresh branch only.**
- Stage0 ADR #3 ↔ repo-rename PR #5 ↔ PROJECT_STATUS README link: overlapping docs/README. **Land in the order above.**
- Firefox migration ↔ Developer Mode #25: both alter product ISO/session/build scripts. **Do not run in parallel.**
- Coreboot/Compaq firmware branches: all board-specific firmware work has one Boot Stack Owner; physical flashing is gated by Hardware Validation Owner.

## Rule for all agents

Before starting a task:

1. read this file;
2. fetch current main;
3. check whether your owner role is already active;
4. treat any branch >20 commits behind main as **salvage-only**;
5. never infer implementation from a roadmap checkbox;
6. never say “main is green” without workflow + run ID;
7. update this file through Agent 1 when ground truth materially changes.
