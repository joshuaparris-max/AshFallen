# Developer Mode

Developer Mode lets the Linux-backed Josh OS product host the tools that build
the next Josh OS. It is deliberately **not** an agent with unrestricted root
access rewriting the system that is currently running.

## What is implemented

`josh-dev` provides a guarded local workflow:

1. opt in to Developer Mode and select a workspace;
2. explicitly install the local build/QEMU toolchain when needed;
3. install current Codex CLI, Claude Code and Antigravity CLI from their official
   vendor installers;
4. clone/fetch the canonical Josh repositories;
5. create a dedicated Git worktree and `ai/...` branch for an agent task;
6. run the agent inside that worktree;
7. run repository checks after the agent exits;
8. explicitly publish a verified branch; and
9. for the AshFallen or Stage0 product repo, build a verified worktree into a
   candidate ISO without replacing the running image.

The product image includes the small runtime pieces needed to enter Developer
Mode. The heavier compiler, ArchISO and QEMU toolchain is installed only when
you explicitly run `josh-dev install-toolchain`, so normal Josh OS images are
not bloated just to carry development tools.

## Repository ownership

Developer Mode uses these repositories and does not merge their roles:

- `ashfallen` — `joshuaparris-max/AshFallen`: canonical Josh OS product and
  native x86-64 kernel.
- `joshbios` — `Parris-Tech-Services/JoshBIOS`: firmware, BIOS and
  JoshBootloader.
- `stage0` — `Parris-Tech-Services/JoshOS-Stage0`: rapid Linux/browser desktop
  prototype.

The repository manifest is `/etc/josh-os/developer-repos.conf` in the image.

## First use

Open a real terminal with **Ctrl+Alt+T** or **Super+Enter**. The terminal drawn
inside the browser shell is only a UI prototype and cannot execute Linux
commands.

On an installed/persistent system:

```sh
josh-dev enable
josh-dev install-toolchain
josh-dev install-agents all
josh-dev sync
josh-dev status
```

On the current live ISO, the root filesystem is temporary. Developer Mode will
refuse to pretend otherwise. Mount persistent storage and point the workspace
at it:

```sh
josh-dev enable --workspace /path/on/persistent/storage
```

For a deliberately throw-away VM session only:

```sh
josh-dev enable --ephemeral
```

## Ask an agent to improve a repository

For example:

```sh
josh-dev improve ashfallen --agent codex -- \
  "Improve Chromium crash recovery and add a regression test"
```

The command fetches the base clone, creates a separate worktree/branch, runs the
agent there, then runs verification. The worktree is retained whether the agent
or verification succeeds or fails so the result can be inspected.

Codex automation uses `workspace-write`, not full host access. Claude Code is
launched with its normal interactive permission flow. Antigravity is invoked in
print mode without its dangerous permission-bypass option.

Inspect task state with:

```sh
josh-dev list
josh-dev verify <task-id>
```

A verified branch can be pushed explicitly:

```sh
josh-dev publish <task-id>
```

Publishing does **not** merge the branch.

## Build the next product image

After an `ashfallen` or `stage0` product task is verified:

```sh
josh-dev stage <task-id>
```

This runs that repository's existing ArchISO builder and copies the resulting
ISO plus `SHA256SUMS` into:

```text
<workspace>/candidates/<task-id>/
```

This is a candidate only. The running OS is not overwritten and no boot entry
is changed.

## Current boundary

This is the self-development **host and candidate-build loop**, not a complete
self-update system. The canonical product track already has persistent-home
plumbing, but it still needs an installed-system update format, A/B or
previous-known-good boot targets, boot-attempt tracking, promotion and rollback
before Josh OS should install an AI-produced candidate automatically.

Likewise, the native AshFallen kernel cannot yet host Codex/Claude/Antigravity
itself; it still lacks the mature userspace, networking/TLS and runtime stack
those tools depend on. The Linux product track is the development host for that
native stack for now.
