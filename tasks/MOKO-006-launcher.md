# MOKO-006 — Real application launcher
State: DONE (validated)

Replace hard-coded demo tiles with a model reading Freedesktop `.desktop` entries. Add search and process launch through a non-privileged service/controller.

## Architecture
- `ApplicationRegistry` is the MOKO-owned application API and source model. It reads `.desktop` files by XDG precedence, rejects hidden/invalid entries and exposes stable roles for identity, metadata, icon, category, keywords and launch state.
- `ApplicationFilterModel` provides live multi-term search and the pinned Dock view over the same registry.
- `Exec` is tokenized into a program and argument vector. Field codes are expanded or rejected structurally and `QProcess` launches the program directly; launcher input never reaches a shell.
- A custom image provider resolves standard icon names while existing MOKO glyphs remain the fallback visual language.
- The ISO includes Foot only as a real Wayland launch target. `moko-terminal-bootstrap` is explicitly DEV_ONLY and will be replaced by native MOKO Terminal in MOKO-007.

## Acceptance
- list apps;
- search;
- launch selected app;
- hide invalid/NoDisplay entries;
- no shell command injection.

## Validation
- Debian 13 compile and CTest: 2/2 passed, including desktop-entry parsing,
  multi-term filtering, controlled argument launching and failure reporting.
- Clean hybrid ISO build passed with zstd SquashFS level 5. This compression is
  retained because xz cold boots exceeded the 300-second TCG quality gate on
  the validation host; rollback is removing the two compression flags from
  `image/live-build/auto-config.sh`.
- QEMU artifact prefix `out/moko-iso-smoke-20260904T171445Z`: 3/3 cold boots,
  graphical framebuffer, input/network health, zero greetd restarts and clean
  shutdowns passed.
- Launcher keyboard search started `org.moko.Terminal` as PID 1001 with UID
  1000. A second exact-ISO run captured the mapped interactive terminal at
  `out/moko-iso-smoke-20260904T172816Z-boot-1-launched.png`. ISO SHA-256:
  `a9beeb87f1ba7683f073ab03d75e6798686d3ef5ce0143bc8942f6146a694790`.
