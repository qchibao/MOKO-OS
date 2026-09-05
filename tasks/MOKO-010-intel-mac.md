# MOKO-010 - Live USB readiness
State: DONE (validated in QEMU)

Prepare the non-installing amd64 Developer Preview for QEMU and physical PC or
Intel Mac Live USB validation. MOKO-005 and MOKO-009 are validated prerequisites.

## Architecture and safety
- GRUB/UEFI and ISOLINUX/BIOS expose fixed `desktop`,
  `hardware-diagnostics` and `safe-graphics` profiles.
- `moko-cage-session` applies safe rendering before Cage starts; profile data is
  never evaluated as a shell command.
- The diagnostics profile opens the real native app directly, then falls back
  to MOKO Shell if the app closes.
- A pre-greetd disk audit rejects unexpected block-device mounts. The image has
  no installer, partitioning UI, formatting workflow or automatic `udisks2`
  mount service.
- The release includes required network, Bluetooth, audio, common Intel/AMD
  graphics, input, webcam and firmware inspection tools.
- Rollback is reverting the MOKO-010 implementation commit and using the
  validated MOKO-009 ISO recorded in `STATUS.md`.

## Release output
- Artifact: `out/MOKO-OS-v0.1-dev-amd64.hybrid.iso`
- SHA-256: `21345808f10a208c9a1d43f16a4c2feab1668104dee4c7226aca382bc2ca6a4c`
- Build timestamp (UTC): `2026-09-05T02:10:29Z`
- Source commit: `f879a7171058f8681fa39585403342db24d5e106`
- Source state: clean
- Package manifest: `out/MOKO-OS-v0.1-dev-amd64.packages.txt`
- Known issues: `out/MOKO-OS-v0.1-dev-amd64.known-issues.txt`
- Live USB checklist: `out/MOKO-OS-v0.1-dev-amd64.live-usb-checklist.md`

## Validation
- Two clean builds from the same commit are byte-for-byte identical. Both have
  SHA-256 `21345808f10a208c9a1d43f16a4c2feab1668104dee4c7226aca382bc2ca6a4c`.
- Debian 13 compile, unit and render tests pass: Shell `2/2`, AI `3/3`, native
  apps `8/8`.
- BIOS desktop passes `3/3` cold boots with input, network, MOKO Shell and clean
  shutdown. The interaction run confirms the AI daemon opens MOKO Files and the
  Launcher searches for and opens MOKO Settings as UID 1000. Artifacts:
  `out/moko-iso-smoke-20260905T023402Z-bios-desktop-boot-*`.
- UEFI desktop passes with MOKO Terminal launched from the registry and its PTY
  shell reported ready as UID 1000. Artifacts:
  `out/moko-iso-smoke-20260905T024646Z-uefi-desktop-boot-1.*`.
- Safe Graphics Mode passes with software rendering, working input and zero
  greetd restarts. Artifacts:
  `out/moko-iso-smoke-20260905T025142Z-bios-safe-graphics-boot-1.*`.
- Hardware Diagnostics direct boot passes, reports QEMU as `PARTIAL`, exports
  JSON/text as UID 1000 and detects no writable test disk. Artifacts:
  `out/moko-iso-smoke-20260905T025445Z-bios-hardware-diagnostics-boot-1.*`.
- Static ISO gates confirm BIOS/UEFI profiles, required hardware packages,
  disabled installer/udisks2 paths, release files and absence of unstable APT
  and wget cache files.
- Final screenshots were inspected for the BIOS/UEFI desktop, Safe Graphics and
  Hardware Diagnostics surfaces.

## Physical Intel Mac follow-up

Physical PC and Intel Mac compatibility is not certified by the QEMU result.
The following Live USB-only validation remains required on each target model.

Process:
1. identify exact Mac model and T2 status;
2. full backup;
3. Live USB only, no disk writes;
4. test display/input/Wi-Fi/audio/Bluetooth/suspend;
5. export hardware report and journal;
6. decide whether external SSD testing is safe.
