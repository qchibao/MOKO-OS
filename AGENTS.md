# AGENTS.md — MOKO OS engineering rules

## Mission
Build MOKO OS as a genuine MOKO system layer on top of the Linux kernel and a minimal Debian base. Do not turn this repository into an Ubuntu/Debian desktop reskin.

## Non-negotiable architecture rules
1. GNOME/KDE may be referenced for interoperability research but must not become the MOKO desktop implementation.
2. User-visible shell surfaces belong to `shell/` and use the MOKO design language.
3. The temporary Cage compositor is **bootstrap infrastructure only**. Replace it incrementally with `compositor/moko-compositor` when that milestone is reached.
4. System APIs must be separated from UI. Prefer D-Bus interfaces for power, networking, settings, search, AI actions and hardware status.
5. Do not embed secrets or API keys in source, images, ISO configuration or logs.
6. Do not implement destructive disk installation by default. The installer remains disabled until partitioning, rollback and recovery tests exist.
7. Every change that affects boot, login, networking, storage, power or permissions needs a test/rollback note.

## Technology baseline
- Base: Debian 13 / trixie minimal, amd64 first.
- Display: Wayland.
- Shell/apps: Qt 6 + QML.
- Core services: Rust preferred for new privileged daemons; C++ acceptable where Qt integration is primary.
- IPC: D-Bus.
- Audio: PipeWire/WirePlumber.
- Network: NetworkManager.
- Bluetooth: BlueZ.
- Init/service management: systemd.
- Packaging: Debian packages initially; MOKO package/update service comes later.

## Coding rules
- Keep modules small and independently testable.
- No silent fallback to GNOME/KDE components.
- Avoid global state in QML; introduce models/controllers as features become real.
- Any temporary implementation must be labeled `BOOTSTRAP` or `DEV_ONLY` in code/comments.
- Prefer readable code over clever abstractions in v0.1.
- Update `STATUS.md` whenever a task moves state.
- Add a short validation section to each completed task file.

## Visual rules
- Bright, calm, premium palette: white, ice blue, cobalt accents, small controlled secondary gradients.
- No default red cyberpunk theme.
- Use the user's MOKO wordmark as brand reference; do not invent a replacement emblem.
- App icons should use a coherent MOKO geometry language rather than copying macOS icons.
- Glass effects should remain readable and not make controls disappear into the wallpaper.

## Before modifying the architecture
Explain the proposed change in the task notes and preserve compatibility with the stated roadmap unless the user explicitly approves the architecture change.
