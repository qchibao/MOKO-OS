# MOKO OS v0.1 status

Updated: 2026-09-04

| Task | State | Notes |
|---|---|---|
| MOKO-001 Repository bootstrap | DONE | Structure, engineering rules and docs created. |
| MOKO-002 Shell Developer Preview | DONE (validated) | Builds and renders under Debian 13 Wayland at 1280x720 and in the live ISO at 1280x800. |
| MOKO-003 Debian build environment | DONE (validated) | Dockerized Debian 13 CMake/Ninja build and CTest smoke test pass. |
| MOKO-004 Live Wayland developer session | DONE (validated) | greetd/Cage reaches MOKO Shell with network/input and zero greetd restarts across 3 cold boots. |
| MOKO-005 Live ISO validation | DONE (validated) | 3/3 QEMU boots and clean shutdowns pass; SHA-256 `f656f962f4dae9f13dd1049f24425baf53bbf1f3d52109c561cc67e28a5c05e6`. |
| MOKO-006 Real app launcher | TODO | Replace demo tiles with desktop-entry model and process launcher. |
| MOKO-007 MOKO Core D-Bus services | TODO | Define first stable APIs for system status/settings. |
| MOKO-008 MOKO AI system action contract | TODO | Permissioned action API; no provider lock-in. |
| MOKO-009 Hardware diagnostics | TODO | Inventory Wi-Fi/audio/GPU/input/power and export report. |
| MOKO-010 Intel Mac Live USB | BLOCKED by MOKO-009 | QEMU validation is complete; run hardware diagnostics before non-destructive USB testing. |

## Definition of first real milestone
A bootable `MOKO-OS-v0.1-dev-amd64.hybrid.iso` that reaches the MOKO Shell Developer Preview in QEMU, with keyboard/mouse, shutdown and basic network stack available.

Milestone achieved on 2026-09-04. Validation artifacts: `out/moko-iso-smoke-20260904T144109Z-boot-*`.
