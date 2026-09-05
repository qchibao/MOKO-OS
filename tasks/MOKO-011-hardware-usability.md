# MOKO-011 - v0.1.1 Hardware & Usability Preview
State: IN PROGRESS - PHASE 1 QEMU VALIDATED

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
