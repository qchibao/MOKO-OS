# MOKO OS v0.1 — Developer Bootstrap

MOKO OS is a Linux-powered operating system project with a MOKO-owned user experience and system layer. Debian is used as the initial low-level base; MOKO Shell, MOKO services, apps, identity, AI integration, update flow and future compositor are developed as MOKO components rather than a GNOME/KDE theme.

## Current bootstrap status

This repository already contains:

- a Qt 6 / QML **MOKO Shell Developer Preview** matching the bright glass/ice concept;
- a MOKO-specific shell component structure (top bar, launcher, AI panel, dock, original programmatic glyphs);
- native Qt 6/QML **MOKO Files, MOKO Settings and MOKO Terminal** applications integrated through the shared MOKO application registry;
- an unprivileged **MOKO AI daemon**, MOKO-owned D-Bus API, allowlisted action layer and provider-backed Shell panel for safe local actions;
- native **MOKO Hardware Diagnostics** with evidence-based compatibility states and privacy-safe JSON/text reports;
- fixed BIOS/UEFI boot entries for **Try MOKO OS**, direct **Hardware Diagnostics** and **Safe Graphics Mode**;
- a non-installing Live USB safety audit that runs before the graphical session and rejects unexpected block-device mounts;
- Debian 13 (trixie) live-build scaffolding for an **amd64/x86_64** developer ISO;
- a MOKO-owned wlroots compositor for the normal multi-window desktop, with Cage retained as the Safe Graphics and rollback path;
- a MOKO Control Center using real NetworkManager, BlueZ, PipeWire/WirePlumber,
  backlight, battery and power-profile state instead of placeholder toggles;
- architecture, design language, hardware target, security principles and roadmap documents;
- Codex-oriented task files and `AGENTS.md`.

The ISO is intentionally labeled **Developer Preview**. The shell, launcher and
first native applications, AI system actions and hardware diagnostics have been built on Debian 13 and
validated in the real QEMU graphical session, including three cold boots with
network/input detection and clean shutdown. It remains a non-installing preview;
non-destructive Live USB readiness and physical hardware testing come next.

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

On macOS or another Docker host, use the reproducible validation path:

```bash
./scripts/test-shell-debian.sh
./scripts/build-iso-docker.sh
MOKO_BOOT_RUNS=3 ./scripts/test-iso-docker.sh
MOKO_BOOT_RUNS=3 MOKO_BOOT_FIRMWARE=uefi MOKO_BOOT_TIMEOUT=480 ./scripts/test-iso-docker.sh
MOKO_BOOT_MODE=hardware-diagnostics ./scripts/test-iso-docker.sh
MOKO_BOOT_MODE=safe-graphics ./scripts/test-iso-docker.sh
MOKO_CONTROL_CENTER_TEST=1 ./scripts/test-iso-docker.sh
MOKO_LAUNCH_QUERY=files MOKO_LAUNCH_APP_ID=org.moko.Files \
  MOKO_REQUIRE_APP_READY=1 MOKO_WINDOW_WORKFLOW=1 ./scripts/test-iso-docker.sh
```

The build emits the ISO, SHA-256, build information, package manifest, known
issues and Live USB checklist under `out/`. See `STATUS.md` and the task files
for the current validation evidence and remaining physical hardware work.

## Use with Codex

Open this repository in Codex and tell it:

> Read `AGENTS.md`, `docs/ARCHITECTURE.md`, `docs/DESIGN_SYSTEM.md`, and `STATUS.md`. Continue from the first unfinished task in `tasks/`. Preserve the MOKO architecture: do not replace MOKO Shell with GNOME/KDE and do not reduce the project to a theme.

See `docs/CODEX_HANDOFF.md` for the exact workflow.
