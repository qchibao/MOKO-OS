# MOKO-011 - v0.1.1 Hardware & Usability Preview
State: IN PROGRESS - QEMU RELEASE GATE PASSED; PHYSICAL MACBOOK VALIDATION PENDING

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

## Phase 6 validation

Validated on 2026-09-06 from clean commit `45234e7`, Live ISO SHA-256
`b6e2a4c11b77c76c0a7566a1393cf61fb1cb0defb73ea02ac0c3e4344c6ac1ef`.

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
- BIOS desktop cold boots: `3/3` passed with `greetd_restarts=0`;
- UEFI desktop cold boots: `3/3` passed with `greetd_restarts=0`;
- combined Control Center, Input and multi-window workflow passed, including
  real PipeWire mute, two mapped apps, move, resize, minimize/restore,
  maximize/restore, fullscreen, snap, Dock restore and Alt+Tab;
- Safe Graphics passed with Cage/software rendering, Files launch/return and
  clean shutdown;
- Hardware Diagnostics direct boot retained the report enum, rendered consumer
  labels, exported JSON/text as UID 1000 and reported
  `writable_disk_detected=0`;
- Browser HTTPS, JavaScript, real Debian package download and MOKO Files
  Downloads handoff passed;
- AI `open settings` passed Ready -> Processing -> Response through the
  allowlisted D-Bus action as UID 1000;
- every ISO run passed disk safety, graphical health and clean shutdown; the
  installer remained disabled and no writable disk was attached.

Primary artifacts: `out/moko-iso-smoke-20260905T213037Z-bios-desktop-*`,
`out/moko-iso-smoke-20260905T214133Z-uefi-desktop-*`,
`out/moko-iso-smoke-20260905T215357Z-bios-desktop-*`,
`out/moko-iso-smoke-20260905T220022Z-bios-safe-graphics-*`,
`out/moko-iso-smoke-20260905T220358Z-bios-hardware-diagnostics-*`,
`out/moko-iso-smoke-20260905T220731Z-bios-desktop-*` and
`out/moko-iso-smoke-20260905T221329Z-bios-desktop-*`.

## Phase 7 architecture - desktop usability

The Shell now owns the standard `org.freedesktop.Notifications` session-bus
service and a MOKO Notification Center. Screenshots use wlroots screencopy
through `grim`, with the compositor exposing only the standard read-only
screencopy and output-description protocols. Clipboard text and file URLs use
normal Wayland MIME offers, while MOKO Files implements matching copy, cut and
paste behavior without inventing a private clipboard format.

MOKO apps share a bounded native Open/Save picker backed by filesystem APIs.
The compositor exposes only two allowlisted keyboard layouts and guarded
integer output scales through the versioned MOKO desktop protocol. QEMU's
1280x800 output correctly advertises only 100 percent, so a scale request
cannot make the Shell unusable. Global Launcher, AI, Notification Center,
screenshot and language shortcuts remain fixed compositor enums rather than
commands or user-provided strings.

## Phase 7 validation

Validated on 2026-09-06 from a clean Live ISO build. Physical clipboard,
multi-display scaling and input-method validation remain part of the MacBook
test pass.

- compositor CTest: `4/4` passed;
- Shell CTest: `7/7` passed;
- AI CTest: `3/3` passed;
- native apps, Browser, PTY, diagnostics and shared picker CTest: `11/11`
  passed;
- Qt compositor multi-window session: passed;
- disk safety, graphical health and clean shutdown: passed;
- English/Vietnamese switching, Notification Center and a real `grim`
  screenshot: passed in the running ISO session;
- Files and Settings remained simultaneously mapped while snap, maximize,
  fullscreen, minimize/restore, resize, move and Alt+Tab passed.

Primary artifact:
`out/moko-iso-smoke-20260906T025232Z-bios-desktop-boot-1.*`.

## Phase 8 architecture - suspend and resume recovery

`systemd-logind` remains the sole suspend authority. MOKO Shell will observe
the standard system-bus `PrepareForSleep(bool)` signal as UID 1000; it will not
add a privileged suspend daemon or bypass logind policy.

Before sleep, the Shell records which recoverable services and applications
were actually present. On resume it refreshes NetworkManager, BlueZ,
PipeWire/WirePlumber, backlight, battery and power state; reconnects the
bounded MOKO compositor observer so windows and libinput state are enumerated
again; and refreshes the MOKO AI daemon connection. If Browser was mapped
before sleep, resume health requires it to remain mapped afterward. Hardware
that was absent before sleep remains truthful and does not become a false
failure.

Structured `MOKO_SLEEP` and `MOKO_RESUME_HEALTH` markers make the recovery
path testable without granting the Shell new capabilities. Direct lifecycle
unit tests simulate logind signal ordering. A QEMU suspend/wakeup gate may
exercise the virtual ACPI path, but it does not replace physical Intel MacBook
validation of display, trackpad, radios, audio and battery recovery.

## Phase 8 rollback and safety

Removing the Shell lifecycle observer returns to Phase 7 behavior. Cage stays
available through Safe Graphics and `MOKO_COMPOSITOR=cage`. The change does not
touch boot ordering, mounts, storage devices, installer policy or disk-safety
checks, and no QEMU validation may attach a writable disk.

## Phase 8 validation

Validated on 2026-09-06 from a fresh Live ISO build, SHA-256
`bbc569c8b14fe08be7dc1e99cc6ac493f4e6231ef0b473916e7443372328f260`.
Physical Intel MacBook suspend/resume remains required.

- live disk-safety serial-sink regression: passed;
- compositor CTest: `4/4` passed;
- Shell CTest: `8/8` passed, including lifecycle recovery, timeout and
  already-absent hardware cases;
- AI CTest: `3/3` passed;
- native apps, Browser, PTY, diagnostics and shared picker CTest: `11/11`
  passed;
- Qt compositor multi-window session: passed;
- standard-VGA QEMU S3 gate: QMP reached `suspended`, woke to `running`, and
  Shell recovery reported compositor, desktop protocol, Browser, AI,
  NetworkManager, PipeWire and input protocol healthy;
- Browser rendered HTTPS and executed JavaScript before suspend, remained
  mapped after wake, then reloaded HTTPS and JavaScript successfully;
- post-resume framebuffer: passed with a nonblank 1280x800 capture at
  `out/moko-iso-smoke-20260906T073453Z-bios-desktop-std-boot-1-resumed.png`;
- normal `virtio-vga` BIOS boot, disk safety, graphical Shell and ACPI
  poweroff: passed at
  `out/moko-iso-smoke-20260906T074129Z-bios-desktop-boot-1.*`;
- installer remained disabled and no writable disk was attached.

QEMU backend limitations are kept explicit. `virtio-vga` retains a live guest
DRM state after S3 but loses host scanout, so it cannot validate framebuffer
recovery. Standard VGA preserves scanout but QEMU q35 can fail its emulated S5
transition after S3 even after the guest reaches `reboot: Power down`. The
resume-only harness therefore uses an allowlisted QMP cleanup after all guest
and framebuffer assertions; normal BIOS/UEFI gates continue to require real
ACPI poweroff with `virtio-vga`.

## Final QEMU release-candidate gate

The release candidate remains a non-installing, non-persistent Live image. Its
source commit is `d42b2bb70e893dbc6068ed3405fb83c8db831cf5`; the embedded build
timestamp is `2026-09-06T09:33:10Z`. Removing live-build apt indexes from the
SquashFS eliminated mirror-controlled `InRelease` timestamp drift, and the ISO
preflight now rejects any apt index that could reintroduce it. A fresh Live
session can still run `apt-get update` and temporarily install packages.

Hardware report buttons now open the shared MOKO Save dialog. The QEMU harness
accepts each suggested filename through that dialog before requiring the JSON
or text write marker; it no longer treats opening the dialog as an export.

Validated on 2026-09-06:

- final ISO: `out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso`;
- SHA-256: `4bcb3baee0a268175fb5b6e0461a77010d3eaca400ce6ede9bfa4f9b81a06ed8`;
- two clean builds from the source commit were byte-identical;
- live disk-safety logging regression: passed;
- compositor CTest: `4/4`; Shell CTest: `8/8`; AI CTest: `3/3`;
- native apps, Browser, PTY, Diagnostics and shared picker CTest: `11/11`;
- real Browser network test and Qt compositor multi-window session: passed;
- BIOS cold boot: `3/3`, each with disk safety, nonblank MOKO Shell,
  `greetd_restarts=0` and ACPI shutdown;
- UEFI cold boot: `3/3` with fresh OVMF variable stores and the same health and
  shutdown requirements;
- Control Center: real PipeWire output mute changed as UID 1000;
- input/usability: truthful QEMU no-touchpad state, safe 100 percent scale,
  English/Vietnamese switching, Notification Center and `grim` screenshot
  passed;
- AI/Terminal: Ready -> Processing -> Response, allowlisted Terminal launch,
  PTY readiness, compositor map/close and shutdown passed;
- Files/Settings multi-window: snap left/right, maximize/restore, fullscreen,
  minimize/Dock restore, simultaneous windows, resize, move, Alt+Tab and close
  passed through real compositor state;
- Browser: sandbox and web security remained enabled; HTTPS, JavaScript, real
  Debian package download and MOKO Files Downloads handoff passed in UEFI;
- AI system information returned a real `system_summary` in UEFI;
- Safe Graphics: Cage/software path, Files launch/return and shutdown passed;
- Hardware Diagnostics: direct boot and Launcher paths produced the QEMU
  report with `writable_disk_detected=0`, then saved JSON (`21133` bytes) and
  text (`5476` bytes) reports through the MOKO dialog as UID 1000;
- standard-VGA suspend/resume: QMP suspend/wake, compositor, Browser, AI,
  NetworkManager, PipeWire and input recovery passed; Browser HTTPS and
  JavaScript also passed after wake;
- every counted QEMU run attached no writable disk, reported
  `unexpected_block_mounts=0`, and kept the installer disabled.

Primary artifacts:

- BIOS `3/3`: `out/moko-iso-smoke-20260906T123532Z-bios-desktop-*`;
- Control Center: `out/moko-iso-smoke-20260906T124715Z-bios-desktop-*`;
- input/usability: `out/moko-iso-smoke-20260906T125150Z-bios-desktop-*`;
- AI/Terminal: `out/moko-iso-smoke-20260906T125701Z-bios-desktop-*`;
- multi-window: `out/moko-iso-smoke-20260906T130157Z-bios-desktop-*`;
- UEFI `3/3` and Browser: `out/moko-iso-smoke-20260906T130832Z-uefi-desktop-*`;
- UEFI AI: `out/moko-iso-smoke-20260906T132600Z-uefi-desktop-*`;
- Safe Graphics: `out/moko-iso-smoke-20260906T133126Z-bios-safe-graphics-*`;
- direct Diagnostics: `out/moko-iso-smoke-20260906T134138Z-bios-hardware-diagnostics-*`;
- suspend/resume: `out/moko-iso-smoke-20260906T134532Z-bios-desktop-std-*`;
- Launcher Diagnostics: `out/moko-iso-smoke-20260906T135952Z-bios-desktop-*`.

## Third-party browser compatibility

These applications were validated as temporary Live-session installs and are
not bundled:

- Debian Chromium `152.0.7977.82`: installed after a real `apt-get update`,
  launched as UID 1000, mapped as a Wayland window, retained the root-owned
  `4755` sandbox helper, contained no `--no-sandbox` policy override and
  rendered HTTPS;
- Google Chrome `152.0.7977.82`: the official amd64 `.deb` installed and ran as
  UID 1000, mapped as `google-chrome`, retained its root-owned `4755` sandbox
  helper, contained no `--no-sandbox` policy override and rendered HTTPS;
- Tor Browser `15.0.21`: the official x86_64 archive downloaded, extracted
  under `~/Applications`, passed dynamic-library checks, mapped as
  `Tor Browser`, bootstrapped the Tor network and rendered
  `https://example.com` as UID 1000.

One combined Chrome-plus-Tor VM exceeded the Docker Desktop test container's
memory limit and was not counted. The clean Tor-only VM used 3 GiB guest RAM,
completed HTTPS and shut down without OOM. Several long, combined pointer-input
runs also missed a UI target under TCG; the corresponding isolated functional
gates above passed and are the counted release evidence.

MOKO-011 remains **IN PROGRESS**. QEMU cannot validate the MacBook trackpad,
Wi-Fi, Bluetooth, backlight, battery, microphone, webcam or physical
suspend/resume path. Stop here and run the Live USB checklist on the same Intel
MacBook Pro 2015 before marking v0.1.1 complete or beginning installer work.

## Physical hotfix H1-H8

The locked physical hotfix pack is implemented on
`hotfix/v0.1.1-physical-ui` without changing the AI contract or enabling an OS
installer. Stable phase commits are:

- H1 `816149e`: quiet branded normal boot and compositor-owned fade-to-black
  shutdown held until poweroff;
- H2 `52cbcb8`: display confirmation rollback, Vietnam timezone and truthful
  charging state;
- H3 `595eccf`: capability-based trackpad gestures and browser history swipe;
- H4 `257a5be`: real connectivity preloading and Light/Dark/Glass appearance;
- H5 `af6e310`: reference-led Settings and Files visual/interaction rebuild;
- H6 `0635682`: MOKO Browser home/new-tab and download completion workflow;
- H7 `a358326`: verified local `.deb` review/install flow with a fixed polkit
  helper, safe dependency handling and application-registry refresh.

H8 started from a clean committed tree and completed the automated release gate
on 2026-09-09. The package flow modifies only the ephemeral Live overlay;
MOKO-012 remains disabled and internal disks remain outside the installation
path.

### H8 automated validation

- static boot/shutdown presentation, serial-sink disk-safety and Live launch
  telemetry regressions passed;
- compositor CTest `4/4`, Shell CTest `8/8`, unchanged AI CTest `3/3`, native
  apps/PTY/Diagnostics/Browser/package-installer CTest `15/15`, Browser network
  and Qt compositor multi-window sessions passed;
- BIOS desktop boots passed in the isolated AI/Terminal, multi-window and
  Browser/package workflows; each reported `unexpected_block_mounts=0`,
  `greetd_restarts=0` and completed a black-frame ACPI shutdown;
- UEFI desktop passed three times using fresh OVMF state: two boots under
  `out/moko-iso-smoke-20260908T235109Z-uefi-desktop-*` and one isolated boot
  under `out/moko-iso-smoke-20260909T015005Z-uefi-desktop-*`;
- Control Center and real PipeWire mute passed under
  `out/moko-iso-smoke-20260909T014104Z-bios-desktop-*`;
- QEMU input truth, safe scale, English/Vietnamese switching, Notification
  Center and screenshot passed under
  `out/moko-iso-smoke-20260909T014522Z-bios-desktop-*`;
- AI Ready -> Processing -> Response and allowlisted Terminal launch passed
  under `out/moko-iso-smoke-20260909T001937Z-bios-desktop-*`;
- Files/Settings snap, maximize, fullscreen, minimize/Dock restore, resize,
  move, Alt+Tab and close passed under
  `out/moko-iso-smoke-20260909T012055Z-bios-desktop-*`;
- Browser sandbox/HTTPS/JavaScript, real Debian download, Files handoff and the
  confirmed temporary `.deb` install passed under
  `out/moko-iso-smoke-20260909T012653Z-bios-desktop-*`;
- Safe Graphics passed under
  `out/moko-iso-smoke-20260909T013418Z-bios-safe-graphics-*`;
- direct and Launcher Hardware Diagnostics produced truthful QEMU inventories,
  reported `writable_disk_detected=0` and exported JSON/text as UID 1000 under
  `out/moko-iso-smoke-20260909T013723Z-bios-hardware-diagnostics-*` and
  `out/moko-iso-smoke-20260909T015428Z-bios-desktop-*`;
- standard-VGA suspend/resume recovered compositor, Browser, AI,
  NetworkManager, PipeWire and input state, then reloaded HTTPS/JavaScript under
  `out/moko-iso-smoke-20260909T020622Z-bios-desktop-std-*`.

Every normal desktop shutdown capture used the strict `bright_pixels=0`
assertion. No QEMU run attached a writable disk. Final clean ISO generation and
byte-for-byte reproducibility use this H8 record commit; the generated
`out/SHA256SUMS` is the release checksum source.

### H8 physical-only remainder

QEMU cannot certify the MacBook internal panel and scaling, three-finger drag,
horizontal trackpad history swipe, charging icon transitions, Wi-Fi and
Bluetooth discovery/pairing, brightness keys, audio/microphone/webcam, or real
suspend/resume and poweroff presentation. MOKO-011 therefore remains
**IN PROGRESS**. Stop after generating the ISO and complete
`docs/LIVE_USB_CHECKLIST.md` on the same Intel MacBook Pro 2015 before any
MOKO-012 work.
