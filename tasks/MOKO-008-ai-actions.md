# MOKO-008 - MOKO AI daemon and safe action contract
State: DONE (validated)

Create a provider-agnostic AI daemon contract and permissioned system-action broker.

Do not give model output arbitrary shell/root execution.

## Architecture
- `moko-ai-ui` connects the existing Shell panel to the user-session D-Bus.
- `moko-ai-daemon` owns `org.moko.AI1` and refuses normal execution as root.
- `moko-ai-actions` exposes only ping, system summary, home-file search,
  allowlisted application/file opening, battery, network and storage status.
- `AiProvider` separates request interpretation from action execution.
  `LocalStubProvider` is the offline v0.1 implementation;
  `RemoteApiProvider` is an interface with no key or provider lock-in.
- Application opening returns through the Shell-owned
  `org.moko.Applications1` service, so AI, Launcher and Dock share the same MOKO
  application registry.

## Safety and rollback
- There is no shell-command capability. Shell-like prompts are refused before
  action dispatch, and providers cannot launch processes directly.
- Application ids require an explicit allowlist. File paths must exist,
  canonicalize inside the current user's home and open through `xdg-open` with a
  fixed executable plus argument list.
- The daemon and all accepted actions were validated as UID 1000. No writable
  virtual disk is attached during ISO testing.
- Rollback is reverting this milestone commit; the validated MOKO-007 ISO hash
  remains recorded in `tasks/MOKO-007-core-dbus.md`.

## Validation
- Debian 13 Shell CTest: 2/2 passed.
- Debian 13 AI CTest: 3/3 passed, including real D-Bus round trips and explicit
  shell-command refusal.
- Debian 13 application CTest: 6/6 passed.
- Live system-summary request passed through the graphical AI panel:
  `out/moko-iso-smoke-20260904T194422Z-boot-1-ai.png`.
- Live `open files` request launched `org.moko.Files` through the shared registry
  as UID 1000 and emitted both process and app-ready markers:
  `out/moko-iso-smoke-20260904T195708Z-boot-1-ai.png`.
- Cold-boot regression artifacts: `out/moko-iso-smoke-20260904T200120Z-boot-*`.
  Result: 3/3 graphical boots, network/input health, zero greetd restarts and
  clean shutdowns passed.
- Validated ISO SHA-256:
  `4c1a00c43bf0060198b998c6978d7cf178d0eac0b72673572beab50a8ac4fb9f`.
