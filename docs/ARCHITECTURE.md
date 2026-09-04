# MOKO OS v0.1 architecture

## Product boundary
MOKO OS v0.1 is **not** a Linux distribution with only wallpaper/icon/theme changes. Linux and Debian provide the hardware/userland foundation while MOKO owns the product-facing system layer.

```text
Applications
  MOKO Files / Settings / Terminal / Store / AI
                 │
MOKO UX + Framework
  MOKO Shell / Design Tokens / App APIs
                 │
MOKO Core Services
  session / settings / power / permissions / search / AI actions
                 │
System Infrastructure
  D-Bus / systemd / NetworkManager / PipeWire / BlueZ
                 │
Wayland compositor
  Bootstrap: Cage        Target: moko-compositor
                 │
Debian minimal userland
                 │
Linux kernel + drivers
                 │
Hardware
```

## Why Debian is underneath
The v0.1 goal is to spend engineering effort on the MOKO product layer instead of reimplementing mature kernel drivers, filesystems, power management and networking. Debian is an implementation base, not the visible desktop identity.

## Shell boundaries
`moko-shell` owns:
- top bar;
- app launcher;
- dock;
- overview/workspace experience;
- notification center;
- control center;
- AI surface;
- lock screen UI (after authentication backend is defined).

It should eventually consume system state through MOKO D-Bus services rather than calling privileged commands directly.

## Compositor plan
### Bootstrap stage
Use Cage only to give the full-screen MOKO Shell a deterministic Wayland environment for Developer Preview builds.

### Native MOKO stage
Develop `moko-compositor` using wlroots or another well-maintained Wayland compositor library. It will own window placement, workspaces, effects, global shortcuts, input routing and shell protocol integration.

## Security boundary
The AI UI is unprivileged. Privileged system actions require a MOKO action broker with explicit capability checks/polkit-style authorization. AI provider credentials belong in the user secret store, never in shell QML or ISO source.

## MOKO AI v0.1 boundary
`moko-ai-ui` is a Shell-side controller that talks to `moko-ai-daemon` over the
user session bus at `org.moko.AI1`. The daemon interprets requests through an
`AiProvider`, then dispatches only named capabilities through `moko-ai-actions`.
The local provider is deterministic and offline; the remote provider remains an
interface and must obtain future credentials from user configuration or secret
storage.

Application actions are restricted to an explicit MOKO allowlist and route back
through the Shell-owned `org.moko.Applications1` registry API. File actions
canonicalize existing paths and remain inside the current user's home. System
status reads NetworkManager D-Bus, `/proc`, `/sys` and mounted-filesystem APIs.
There is no shell-command action and model/provider output cannot create a
process directly.

## Architecture targets
- v0.1: amd64 only.
- Later: arm64 feasibility track after desktop APIs stabilize.
- No compatibility promise for Apple silicon in v0.1.
