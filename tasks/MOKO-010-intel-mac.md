# MOKO-010 - Live USB readiness
State: IN PROGRESS

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
- `MOKO-OS-v0.1-dev-amd64.hybrid.iso`
- SHA-256 and reproducible commit-derived build timestamp
- git commit/source state and package manifest
- known issues and `docs/LIVE_USB_CHECKLIST.md`

## Validation
Pending clean ISO build, BIOS/UEFI profile tests, application/AI regression,
3/3 cold boots, clean shutdown and reproducibility comparison.

## Physical Intel Mac follow-up

Process:
1. identify exact Mac model and T2 status;
2. full backup;
3. Live USB only, no disk writes;
4. test display/input/Wi-Fi/audio/Bluetooth/suspend;
5. export hardware report and journal;
6. decide whether external SSD testing is safe.
