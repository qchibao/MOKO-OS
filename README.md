# MOKO OS v0.1 — Developer Bootstrap

MOKO OS is a Linux-powered operating system project with a MOKO-owned user experience and system layer. Debian is used as the initial low-level base; MOKO Shell, MOKO services, apps, identity, AI integration, update flow and future compositor are developed as MOKO components rather than a GNOME/KDE theme.

## Current bootstrap status

This repository already contains:

- a Qt 6 / QML **MOKO Shell Developer Preview** matching the bright glass/ice concept;
- a MOKO-specific shell component structure (top bar, launcher, AI panel, dock, original programmatic glyphs);
- Debian 13 (trixie) live-build scaffolding for an **amd64/x86_64** developer ISO;
- a temporary Wayland developer session using Cage while `moko-compositor` is still being built;
- architecture, design language, hardware target, security principles and roadmap documents;
- Codex-oriented task files and `AGENTS.md`.

The ISO scaffold is intentionally labeled **Developer Preview**. The current environment used to create this bootstrap does not contain Qt or Debian live-build, so the shell/ISO have not yet been compiled here. The next engineering step is to validate them inside Debian 13 and fix any build/runtime issues before booting physical hardware.

## First target

`MOKO OS v0.1 Developer Preview — x86_64`

Primary hardware target:

- Intel Core / Xeon x86-64
- AMD Ryzen / Threadripper / EPYC x86-64
- Intel Macs without Apple silicon, starting in a VM/Live USB before installation

## Quick start on Debian 13

```bash
sudo ./scripts/bootstrap-debian.sh
./scripts/build-shell.sh
./scripts/run-shell.sh
```

Build the live ISO:

```bash
sudo ./scripts/build-iso.sh
```

Then test it in QEMU:

```bash
./scripts/qemu-test.sh out/MOKO-OS-v0.1-dev-amd64.hybrid.iso
```

## Use with Codex

Open this repository in Codex and tell it:

> Read `AGENTS.md`, `docs/ARCHITECTURE.md`, `docs/DESIGN_SYSTEM.md`, and `STATUS.md`. Continue from the first unfinished task in `tasks/`. Preserve the MOKO architecture: do not replace MOKO Shell with GNOME/KDE and do not reduce the project to a theme.

See `docs/CODEX_HANDOFF.md` for the exact workflow.
