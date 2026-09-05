# moko-compositor

`moko-compositor` is the MOKO-owned Wayland compositor introduced for the
v0.1.1 Hardware & Usability Preview. It uses Debian 13's wlroots 0.18 library
for hardware, protocol and rendering primitives while MOKO owns desktop window
policy.

Implemented policy includes:

- simultaneous XDG toplevels;
- focus and stacking;
- client and modifier-driven move/resize;
- minimize/restore, maximize/restore and fullscreen;
- left/right snap;
- Alt/Meta+Tab switching;
- a bounded MOKO Wayland protocol for Shell/Dock window control;
- clipboard selection plumbing.

Cage remains installed as the Safe Graphics and rollback compositor until the
new normal desktop path passes QEMU and physical Intel Mac validation.

## Build

```bash
cmake -S compositor/moko-compositor -B build/moko-compositor -G Ninja
cmake --build build/moko-compositor
ctest --test-dir build/moko-compositor --output-on-failure
```

## Test backend

The compositor supports wlroots' headless backend and an explicit socket:

```bash
XDG_RUNTIME_DIR=/tmp/moko-runtime \
WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 \
build/moko-compositor/moko-compositor --socket wayland-moko-test
```

The compositor does not accept or evaluate startup shell strings. Session
launch is owned by `core/moko-session`.
