# MOKO OS status

Updated: 2026-09-09

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
| MOKO-010 Live USB readiness | DONE (validated in QEMU) | The MacBook 2015 serial false-failure is fixed without weakening disk safety. Two clean builds are identical; BIOS `3/3`, UEFI, Safe Graphics, Diagnostics and shutdown pass on SHA-256 `0427dc95224f582184ca3a61f54b2a1750a5308b2edec41c72b0da0dd5da1483`. Physical MacBook retest remains pending. |
| MOKO-011 v0.1.1 Hardware & Usability Preview | IN PROGRESS | Physical hotfix H1-H8 implementation and automated QEMU gates are complete. Final clean/reproducible artifact generation and physical MacBook Pro 2015 validation remain required before completion. |

## Current release target
A reproducible `MOKO-OS-v0.1.1-dev-amd64.hybrid.iso` that reaches the MOKO Hardware & Usability Preview in QEMU, preserves disk safety and supports the validated multi-window, system-control, Browser, AI and suspend/resume workflows. The remaining gate is physical validation on the Intel MacBook Pro 2015.

## v0.1.1 physical hotfix

The hotfix branch preserves the previous QEMU release behavior while applying
the physical-test specification in phases H1-H8. Normal boot and shutdown are
now consumer-only visual paths, display changes have timed rollback, Vietnam
timezone and charging state are truthful, trackpad gestures are compositor
owned, connectivity pages preload real state, Settings/Files/Browser match the
locked references, and downloaded Debian packages open in a narrowly scoped
MOKO review/install flow. MOKO AI remains frozen and the OS installer remains
disabled. The H8 component suite and isolated QEMU functional gates pass with
zero unexpected block mounts and black shutdown frames. The Intel MacBook Pro
2015 remains the final authority for physical validation.

## Physical hotfix automated gate

- Debian 13 component suite: compositor `4/4`, Shell `8/8`, existing AI `3/3`
  and native apps `15/15` passed; Browser network and Qt multi-window sessions
  also passed.
- Three independent BIOS desktop boots and three UEFI desktop boots passed disk
  safety, graphical health, zero greetd restarts and clean shutdown.
- AI/Terminal, real multi-window state, Control Center, input/usability,
  Browser HTTPS/JavaScript/download/temporary `.deb` install, Safe Graphics,
  direct and Launcher Diagnostics, and QEMU suspend/resume passed.
- MOKO-011 stays `IN PROGRESS` until the locked physical checklist passes on
  the Intel MacBook Pro 2015. MOKO-012 remains disabled.

## Previous v0.1.1 QEMU release candidate

- ISO: `out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso`
- SHA-256: `4bcb3baee0a268175fb5b6e0461a77010d3eaca400ce6ede9bfa4f9b81a06ed8`
- ISO source commit: `d42b2bb70e893dbc6068ed3405fb83c8db831cf5`
- Build timestamp: `2026-09-06T09:33:10Z`
- Reproducibility: two clean builds from the same commit are byte-identical.
- Debian tests: disk safety passed; compositor `4/4`, Shell `8/8`, AI `3/3`, apps `11/11`, Browser network and Qt multi-window sessions passed.
- QEMU: BIOS `3/3`, UEFI `3/3`, Control Center, input/usability, AI/Terminal, Files/Settings multi-window, Browser HTTPS/JavaScript/download, Safe Graphics, Diagnostics export and suspend/resume passed.
- Third-party compatibility: Debian Chromium, Google Chrome and Tor Browser were installed and run temporarily as UID 1000; none is bundled in the ISO.
- Installer and persistence remain disabled; every QEMU run attached no writable disk and reported `unexpected_block_mounts=0`.

The original v0.1 milestone was achieved on 2026-09-04. Validation artifacts: `out/moko-iso-smoke-20260904T144109Z-boot-*`.
