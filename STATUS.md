# MOKO OS v0.1 status

Updated: 2026-09-04

| Task | State | Notes |
|---|---|---|
| MOKO-001 Repository bootstrap | DONE | Structure, engineering rules and docs created. |
| MOKO-002 Shell Developer Preview | DONE (uncompiled) | Initial Qt/QML shell implementation created; needs Debian 13 build validation. |
| MOKO-003 Debian build environment | READY | Bootstrap script prepared; needs validation. |
| MOKO-004 Live Wayland developer session | READY | Cage/greetd bootstrap config prepared; needs boot validation. |
| MOKO-005 Live ISO validation | TODO | Build on Debian 13, boot in QEMU, collect logs, fix issues. |
| MOKO-006 Real app launcher | TODO | Replace demo tiles with desktop-entry model and process launcher. |
| MOKO-007 MOKO Core D-Bus services | TODO | Define first stable APIs for system status/settings. |
| MOKO-008 MOKO AI system action contract | TODO | Permissioned action API; no provider lock-in. |
| MOKO-009 Hardware diagnostics | TODO | Inventory Wi-Fi/audio/GPU/input/power and export report. |
| MOKO-010 Intel Mac Live USB | BLOCKED by MOKO-005/009 | Test non-destructively after QEMU validation. |

## Definition of first real milestone
A bootable `MOKO-OS-v0.1-dev-amd64.hybrid.iso` that reaches the MOKO Shell Developer Preview in QEMU, with keyboard/mouse, shutdown and basic network stack available.
