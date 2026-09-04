# MOKO-007 - Real MOKO system apps
State: DONE (validated)

Implement the first functional native MOKO applications in Qt 6/QML and expose
all three through the MOKO application registry and Dock.

## Architecture
- `moko-files` uses a C++ filesystem model for navigation, history, file
  operations, MIME opening and properties. Destructive deletion requires an
  explicit confirmation and the application refuses to run as root.
- `moko-settings` reads live system state through Qt D-Bus, `/proc`, `/sys`,
  mounted-filesystem APIs and PipeWire inspection. Unsupported mutations remain
  visibly read-only rather than being simulated.
- `moko-terminal` uses `forkpty` and libvterm for a real interactive user shell,
  terminal resizing, scrollback and clipboard integration. It refuses root and
  never elevates the child shell.
- Each application owns its `.desktop` entry; Launcher and Dock consume those
  entries through the same `ApplicationRegistry` implemented by MOKO-006.
- BOOTSTRAP: Cage 0.2 does not raise an independent top-level above the existing
  fullscreen Shell. Until `moko-compositor` owns window management, the Shell
  yields its surface after a registered app starts and restores it when the app
  exits. Serial markers make this handoff testable.

## Safety and rollback
- File mutations use the current user's permissions and never run as root.
- Delete is never silent. Copying a directory into itself is rejected.
- ISO/QEMU validation attaches no writable virtual disk.
- Rollback is reverting this milestone commit; the previously validated
  MOKO-006 ISO remains identified in `tasks/MOKO-006-launcher.md`.

## Validation
- Debian 13 Shell CTest: 2/2 passed.
- Debian 13 application CTest: 6/6 passed.
- Weston render validation passed for Files, Settings and Terminal.
- Real ISO/QEMU application launches passed as UID 1000 with app-ready markers,
  Shell surface hide/show markers and clean shutdown:
  - Terminal PTY: `out/moko-iso-smoke-20260904T184401Z-boot-1-launched.png`
  - Files home directory: `out/moko-iso-smoke-20260904T184915Z-boot-1-launched.png`
  - Settings live system data: `out/moko-iso-smoke-20260904T185403Z-boot-1-launched.png`
- Cold-boot regression artifacts: `out/moko-iso-smoke-20260904T185853Z-boot-*`.
  Result: 3/3 graphical boots, network/input health, zero greetd restarts and
  clean shutdowns passed.
- Validated ISO SHA-256:
  `c3c2b42568bf44034fee97cef968bad9649c023b6fed04f7336023e8fa96e155`.
