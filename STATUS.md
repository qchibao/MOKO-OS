# MOKO OS v0.1 status

Updated: 2026-09-05

| Task | State | Notes |
|---|---|---|
| MOKO-001 Repository bootstrap | DONE | Structure, engineering rules and docs created. |
| MOKO-002 Shell Developer Preview | DONE (validated) | Builds and renders under Debian 13 Wayland at 1280x720 and in the live ISO at 1280x800. |
| MOKO-003 Debian build environment | DONE (validated) | Dockerized Debian 13 CMake/Ninja build and CTest smoke test pass. |
| MOKO-004 Live Wayland developer session | DONE (validated) | greetd/Cage reaches MOKO Shell with network/input and zero greetd restarts across 3 cold boots. |
| MOKO-005 Live ISO validation | DONE (validated) | 3/3 QEMU boots and clean shutdowns pass; SHA-256 `f656f962f4dae9f13dd1049f24425baf53bbf1f3d52109c561cc67e28a5c05e6`. |
| MOKO-006 Real app launcher | DONE (validated) | MOKO registry/search/shared Dock model launches a real app without shell evaluation; 3/3 cold boots pass on ISO SHA-256 `a9beeb87f1ba7683f073ab03d75e6798686d3ef5ce0143bc8942f6146a694790`. |
| MOKO-007 Real MOKO system apps | DONE (validated) | Native Files, Settings and PTY Terminal launch from the shared registry/Dock as UID 1000; 3/3 cold boots pass on ISO SHA-256 `c3c2b42568bf44034fee97cef968bad9649c023b6fed04f7336023e8fa96e155`. |
| MOKO-008 MOKO AI system action contract | DONE (validated) | Unprivileged D-Bus daemon, allowlisted action layer and provider-backed Shell UI pass real system-summary and app-launch tests; 3/3 cold-boot regression passes on ISO SHA-256 `4c1a00c43bf0060198b998c6978d7cf178d0eac0b72673572beab50a8ac4fb9f`. |
| MOKO-009 Hardware diagnostics | DONE (validated) | Native read-only diagnostics, privacy-safe JSON/text exports and Launcher/Settings/AI integration pass in QEMU; 3/3 cold boots pass on ISO SHA-256 `9c6608a10bbb705a09c62caa0a31d4be38a514df6d550f796146329cf799749e`. |
| MOKO-010 Live USB readiness | DONE (validated in QEMU) | Reproducible BIOS/UEFI hybrid Live ISO, safe graphics, direct diagnostics, disk safety guard and release metadata pass all automated gates; physical PC/Intel Mac certification remains pending. Final SHA-256 `21345808f10a208c9a1d43f16a4c2feab1668104dee4c7226aca382bc2ca6a4c`. |

## Definition of first real milestone
A bootable `MOKO-OS-v0.1-dev-amd64.hybrid.iso` that reaches the MOKO Shell Developer Preview in QEMU, with keyboard/mouse, shutdown and basic network stack available.

Milestone achieved on 2026-09-04. Validation artifacts: `out/moko-iso-smoke-20260904T144109Z-boot-*`.
