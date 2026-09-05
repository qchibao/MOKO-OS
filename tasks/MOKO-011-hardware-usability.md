# MOKO-011 - v0.1.1 Hardware & Usability Preview
State: IN PROGRESS - PHASES 1-5 QEMU VALIDATED, PHASE 6 SOURCE VALIDATED

GitHub Issue #1 is the authoritative milestone specification. The work is based
on physical validation on an Intel MacBook Pro 2015 and must not enable an
installer or weaken the v0.1.0 disk-safety gate.

## Phase 1 architecture - real window management

Cage remains the validated rollback compositor and Safe Graphics host. Normal
desktop mode will move incrementally to `moko-compositor`, a MOKO-owned
compositor built on Debian 13's wlroots 0.18 API.

The compositor, rather than application UI, owns real window state:

- simultaneous XDG toplevels;
- focus and stacking;
- pointer move and resize;
- minimize and restore;
- maximize and restore;
- fullscreen;
- close;
- left/right snap;
- Alt/Meta+Tab switching.

MOKO Shell remains unprivileged. It will observe and control windows through a
bounded compositor protocol and will use this state for Dock indicators and
focus/restore behavior. Application launch remains in the existing MOKO
registry; arbitrary command execution is not added.

## Rollback

Set the session compositor to `cage` or boot Safe Graphics. Do not remove the
existing Cage wrapper until normal BIOS, UEFI and physical Intel Mac boot paths
have passed with `moko-compositor`.

## Validation required for Phase 1

- compositor unit/state tests;
- headless Wayland multi-window integration test;
- two simultaneously mapped MOKO applications;
- move and resize;
- minimize/restore and maximize/restore;
- fullscreen and close;
- focus switching and left/right snap;
- Dock running state and focus/restore action;
- existing Shell, apps, AI and disk-safety tests;
- repeated BIOS and UEFI ISO gates with clean shutdown.

## Phase 1 validation

Validated on 2026-09-05 from a clean Live ISO build. Physical Intel Mac
validation remains pending and is not replaced by these results.

- compositor CTest: `3/3` passed;
- Shell CTest: `2/2` passed;
- AI CTest: `3/3` passed;
- native apps, PTY and diagnostics CTest: `8/8` passed;
- Qt headless Shell + Files + Settings multi-window session: passed;
- BIOS desktop cold boots: `3/3` passed with `greetd_restarts=0`;
- UEFI desktop cold boots: `3/3` passed with `greetd_restarts=0`;
- real ISO workflow: Files and Settings mapped simultaneously; move, resize,
  minimize/restore, maximize/restore, fullscreen, close, snap left/right,
  Dock focus/restore and Alt+Tab passed;
- Safe Graphics: Cage + pixman path passed;
- Hardware Diagnostics direct boot: QEMU report and JSON/text exports passed,
  with `writable_disk_detected=0`;
- AI system summary and allowlisted Files launch: passed;
- Terminal PTY launch/map/close in the graphical session: passed;
- disk-safety serial-sink regression and every ISO preflight audit: passed;
- clean shutdown: passed in every completed boot gate.

The Docker-hosted UEFI `3/3` run used `MOKO_BOOT_TIMEOUT=480` because x86_64
TCG under Docker Desktop can take more than the default five minutes. The guest
health criteria were unchanged and still required disk safety, MOKO Shell,
input devices, `moko-compositor`, and zero greetd restarts.

Rollback remains `MOKO_COMPOSITOR=cage`; Safe Graphics selects Cage
automatically. The installer remains disabled and no storage safety policy was
weakened.

## Phase 2 architecture - Control Center and top bar

The Shell now owns functional Wi-Fi, Bluetooth, sound, brightness, battery and
power-mode surfaces through an unprivileged `SystemControl` backend. Network
and Bluetooth use NetworkManager and BlueZ D-Bus APIs. A MOKO-owned BlueZ agent
holds delayed pairing replies until the user accepts a styled PIN, passkey or
authorization dialog.

PipeWire/WirePlumber integration uses `wpctl` with fixed argument vectors for
volume, mute and endpoint selection. Brightness uses `brightnessctl` or a
writable kernel backlight attribute, and the compositor forwards bounded XF86
brightness-key steps through the existing MOKO window protocol. Battery data
comes from power-supply sysfs; power modes use the standard Power Profiles
D-Bus interface. Missing or read-only hardware stays unavailable in the UI.
No UI text is evaluated as a shell command.

## Phase 2 rollback and safety

The Control Center is a Shell component and does not change boot ordering,
mount policy or disk handling. Removing the new Shell controller/components and
the two runtime packages returns to the Phase 1 top bar. Cage remains the Safe
Graphics fallback. The installer, `udisks2`, automounting and writable QEMU
disks remain absent.

## Phase 2 validation

Validated on 2026-09-05 from a clean Live ISO build. Physical Intel Mac radio,
backlight, battery and suspend checks remain pending.

- compositor CTest: `3/3` passed;
- Shell CTest: `4/4` passed, including parser and fixture-backed real-control
  tests;
- AI CTest: `3/3` passed;
- native apps, PTY and diagnostics CTest: `8/8` passed;
- Qt compositor multi-window integration: passed;
- Control Center BIOS and UEFI gates: NetworkManager state reported, absent
  QEMU Bluetooth hardware reported truthfully, virtual HDA discovered through
  PipeWire, default output mute changed as UID 1000, and rendered capture passed;
- BIOS desktop cold boots: `3/3` passed; boot 1 also passed the complete Files
  and Settings multi-window workflow;
- UEFI desktop cold boots: `3/3` passed; boot 1 also passed the Control Center
  audio mutation;
- Safe Graphics: Cage/software path, Files launch/return and shutdown passed;
- Hardware Diagnostics: QEMU report, `writable_disk_detected=0`, JSON/text
  exports as UID 1000 and shutdown passed;
- MOKO AI: connected unprivileged daemon, allowlisted Settings request,
  application readiness/window map/close and shutdown passed;
- disk-safety serial-sink regression and ISO preflight passed on every run;
- installer remained disabled and no writable disk was attached.

Primary artifacts: `out/moko-iso-smoke-20260905T143039Z-bios-desktop-*`,
`out/moko-iso-smoke-20260905T144427Z-uefi-desktop-*`,
`out/moko-iso-smoke-20260905T145847Z-bios-safe-graphics-*`,
`out/moko-iso-smoke-20260905T150234Z-bios-hardware-diagnostics-*` and
`out/moko-iso-smoke-20260905T150618Z-bios-desktop-*`.

## Phase 3 architecture - trackpad and input integration

`moko-compositor` now owns capability-based libinput policy for pointer devices.
It identifies touchpads from supported libinput features instead of matching a
device ID, then enables tap-to-click, tap-and-drag, two-finger scrolling,
clickfinger secondary click, adaptive acceleration and disable-while-typing
where the hardware exposes each capability. Natural scrolling defaults on and
pointer acceleration defaults to `0.2`; both can be changed at runtime from the
Shell Input page.

Version 2 of `moko_window_manager_v1` reports aggregate touchpad capabilities
and actual applied state to the unprivileged Shell. Version 1 clients remain
compatible. The protocol exposes only bounded natural-scroll and acceleration
requests; it does not expose arbitrary libinput mutation or command execution.
Pointer swipe, pinch and hold events are forwarded through the standard Wayland
pointer-gestures protocol so applications and future MOKO workspace policy can
consume supported multi-finger gestures.

The same phase corrected the Shell's normal compositor mapping: the Shell now
receives an output-sized fullscreen configure before its first buffer and stays
pinned behind application windows. This removed the QEMU-only `1600x900`
decorated/cropped surface while preserving Cage as the Safe Graphics fallback.

## Phase 3 rollback and safety

Set `MOKO_COMPOSITOR=cage` or use Safe Graphics to bypass the new compositor
input path. Unsupported settings remain unavailable and no device node,
privilege, mount policy or disk-safety behavior changed. Physical MacBook
validation is still required for tap, scrolling, palm rejection, gestures and
resume behavior; QEMU truthfully reports that no touchpad is present.

## Phase 3 validation

Validated on 2026-09-06 from a fresh live-build run, Live ISO SHA-256
`fdb863a4fd24e128f84fb5fcdbb38f6b552afc4c614d6f0e8a9d3abb0b5d7a29`.

- compositor CTest: `3/3` passed, including protocol v2, v1 compatibility,
  headless no-touchpad state and output-sized fullscreen Shell configuration;
- Shell CTest: `4/4` passed; AI CTest: `3/3`; apps/PTY/diagnostics: `8/8`;
- Qt compositor multi-window session: passed;
- Input ISO gate: compositor and Shell reported the real QEMU no-touchpad
  state, the Input page rendered, and the Shell mapped fullscreen at
  `1280x800`;
- BIOS desktop cold boots: `3/3` passed with `greetd_restarts=0`;
- UEFI desktop cold boots: `3/3` passed with `greetd_restarts=0`;
- combined Input and window workflow: simultaneous Files/Settings, move,
  resize, minimize/restore, maximize/restore, fullscreen, snap, app switching,
  close and clean shutdown passed;
- Control Center PipeWire mutation, AI allowlisted Settings launch, Safe
  Graphics Files launch/return and Hardware Diagnostics JSON/text export all
  passed;
- every ISO run passed the serial-sink disk-safety regression and reported
  `unexpected_block_mounts=0`; no writable disk was attached and the installer
  remained disabled.

Primary artifacts: `out/moko-iso-smoke-20260905T165932Z-bios-desktop-*`,
`out/moko-iso-smoke-20260905T180755Z-bios-desktop-*`,
`out/moko-iso-smoke-20260905T172048Z-bios-desktop-*`,
`out/moko-iso-smoke-20260905T173215Z-uefi-desktop-*`,
`out/moko-iso-smoke-20260905T174440Z-bios-safe-graphics-*`,
`out/moko-iso-smoke-20260905T174820Z-bios-hardware-diagnostics-*`,
`out/moko-iso-smoke-20260905T175155Z-bios-desktop-*` and
`out/moko-iso-smoke-20260905T175614Z-bios-desktop-*`.

## Phase 4 architecture - usable MOKO AI requests

The existing unprivileged `moko-ai-daemon` remains the only request dispatcher.
Its provider contract now reports availability explicitly, and the D-Bus API
exposes `providerStatus` so the Shell can distinguish a connected daemon from a
provider that cannot serve requests. `moko-ai-ui` presents the required Ready,
Processing, Response, Failed and Provider unavailable states. Generation guards
discard stale asynchronous D-Bus replies across refreshes and daemon restarts.

The deterministic local provider recognizes natural requests to open MOKO
Files, Settings and Terminal, or report actual battery, NetworkManager, mounted
storage and system information. Application requests still pass through the
existing `moko-ai-actions` allowlist and `org.moko.Applications1`; status replies
come from the same Linux interfaces used by the direct daemon methods. Provider
output cannot execute a process or shell command, and no secret or remote API
credential is embedded.

## Phase 4 rollback and safety

The phase changes only the user-session AI provider/controller contract and
Shell panel state. Reverting it returns to the v0.1 request API without changing
boot, compositor, permissions, mount policy or disk safety. The daemon and
launched applications remain UID 1000, arbitrary shell requests remain refused,
and the installer remains absent.

## Phase 4 validation

Validated on 2026-09-06 from a fresh live-build run, Live ISO SHA-256
`a6242fff65d7e45e5ab7dcb72a487e7002d9ffac433686912f3f13be607cfcc6`.

- compositor CTest: `3/3` passed; Shell CTest: `4/4` passed;
- AI CTest: `3/3` passed, including required intent coverage, controller state
  transitions and daemon disconnect/reconnect handling;
- native apps, PTY and diagnostics CTest: `8/8` passed;
- Qt compositor multi-window session: passed;
- BIOS desktop: `open settings` produced Ready -> Processing -> Response,
  invoked allowlisted `open_application` for `org.moko.Settings`, passed health
  with `greetd_restarts=0`, and shut down cleanly;
- UEFI desktop: `system information` returned the real `system_summary`, passed
  health with `greetd_restarts=0`, and shut down cleanly;
- Safe Graphics: Cage/software path and clean shutdown passed;
- Hardware Diagnostics: QEMU report plus JSON/text exports as UID 1000 passed,
  with `writable_disk_detected=0`;
- the serial-sink disk-safety regression and ISO preflight passed on every run;
  no writable disk was attached and the installer remained disabled.

Primary artifacts: `out/moko-iso-smoke-20260905T183829Z-bios-desktop-*`,
`out/moko-iso-smoke-20260905T184309Z-uefi-desktop-*`,
`out/moko-iso-smoke-20260905T184801Z-bios-safe-graphics-*` and
`out/moko-iso-smoke-20260905T185113Z-bios-hardware-diagnostics-*`.

## Phase 5 architecture - MOKO Browser

`org.moko.Browser` is a native Qt 6/QML application backed by Qt WebEngine.
MOKO owns the tabs, navigation chrome, history, find and download experience;
Chromium's mature renderer remains inside the normal WebEngine process model.
The Browser refuses root execution and unsafe sandbox/web-security overrides.
No URL or search text is passed to a shell.

Downloads are written to the current user's `Downloads` directory through the
WebEngine download API. Completed files can be opened through the system MIME
handler, and the Browser uses the shared `org.moko.Applications1` registry to
open the download location in MOKO Files. Browser and Files can remain mapped
simultaneously under `moko-compositor`. All history, downloads and installed
third-party browsers are temporary in the current non-persistent Live session.

## Phase 5 rollback and safety

Removing the Browser target, desktop entry and runtime package returns to Phase
4 without changing compositor selection, boot ordering or disk policy. The
WebEngine sandbox and web security are mandatory; no `--no-sandbox` escape hatch
is present. The installer remains absent, no writable QEMU disk is attached,
and downloaded content is confined to the unprivileged Live user's overlay.

## Phase 5 validation

Validated on 2026-09-06 from a fresh live-build run. A clean committed rebuild
and final reproducibility hash follow this phase commit.

- compositor CTest: `3/3`; Shell CTest: `4/4`; AI CTest: `3/3`;
- native apps, PTY, diagnostics and Browser CTest: `10/10`;
- Browser rendered `https://example.com`, executed JavaScript and downloaded
  Debian's real `hello_2.10-5_amd64.deb` as UID 1000;
- MOKO Files opened `~/Downloads` and both applications mapped under the MOKO
  compositor before closing cleanly;
- BIOS desktop, UEFI desktop, Safe Graphics and direct Hardware Diagnostics
  boot gates passed with `greetd_restarts=0` and clean shutdown;
- AI `open files`, Control Center audio mutation, QEMU no-touchpad reporting,
  Dock restore, move, resize, snap, fullscreen and Alt+Tab gates passed;
- every run passed the serial-sink disk-safety regression and reported
  `unexpected_block_mounts=0`; the installer remained disabled.

Primary artifacts: `out/moko-iso-smoke-20260905T200542Z-bios-desktop-*`,
`out/moko-iso-smoke-20260905T201222Z-uefi-desktop-*`,
`out/moko-iso-smoke-20260905T201639Z-bios-safe-graphics-*`,
`out/moko-iso-smoke-20260905T201952Z-bios-hardware-diagnostics-*`,
`out/moko-iso-smoke-20260905T202327Z-bios-desktop-*` and
`out/moko-iso-smoke-20260905T203531Z-bios-desktop-*`.

## Phase 6 architecture - consumer and developer surfaces

Settings and Hardware Diagnostics now keep technical evidence in their MOKO
models while filtering it from the normal interface. Settings persists an
explicit per-user Developer Mode; Hardware Diagnostics exposes a separate
Advanced view. Compatibility enums remain unchanged in report exports and
tests, but the default graphical UI uses Working, Limited, Not detected and
Unsupported without boxed diagnostic badges.

The shared application registry continues to honor standard desktop-entry
visibility. A local hidden desktop entry suppresses Zutty without teaching the
Launcher about a specific helper app. MOKO Files uses native QML dialogs with
the existing MOKO visual tokens, launcher labels fit across two bounded lines,
and the Shell clock uses the active locale and timezone rather than a fixed
12-hour format. None of these paths changes process permissions, boot ordering,
compositor selection, mount policy or disk safety.

## Phase 6 source validation

Validated on 2026-09-06 before the committed ISO regression run.

- live disk-safety logging regression: passed;
- compositor CTest: `3/3` passed;
- Shell CTest: `4/4` passed;
- AI CTest: `3/3` passed;
- native apps, Browser, PTY and diagnostics CTest: `10/10` passed;
- Browser HTTPS/JavaScript network smoke: passed;
- MOKO compositor Qt multi-window session: passed;
- Settings, Files, Hardware Diagnostics and Control Center render captures:
  passed at 1280-wide layouts;
- Settings Developer Mode filtering and Hardware display-label tests: passed;
- committed Live ISO BIOS/UEFI and workflow validation: pending this phase
  commit.
