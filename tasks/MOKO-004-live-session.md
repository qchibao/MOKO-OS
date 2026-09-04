# MOKO-004 — Live Wayland developer session
State: DONE (validated)

Validate the DEV_ONLY greetd + Cage session and automatic launch of MOKO Shell inside a Debian live image.

## Acceptance
- no GNOME/KDE installed;
- boot reaches MOKO Shell;
- mouse/keyboard work;
- shutdown works;
- system logs show no repeating crash loop.

## Validation
- 2026-09-04: three independent BIOS cold boots passed with `MOKO_BOOT_RUNS=3 MOKO_BOOT_TIMEOUT=360 MOKO_SCREENSHOT_TIMEOUT=180 ./scripts/test-iso-docker.sh`.
- Every serial log reports `MOKO_HEALTH result=pass`, NIC `enp0s1`, six input event devices and `greetd_restarts=0`; artifacts are under `out/moko-iso-smoke-20260904T144109Z-boot-*`.
- `out/qemu-mouse-click.png` records pointer focus and keyboard input in the launcher. Each automated run also captured a complete framebuffer and exited QEMU with status 0 after ACPI shutdown/live-media eject.
- The ISO manifest audit rejects GNOME Shell, KDE Plasma, installers and build-only Qt/CMake dependencies.

## Rollback
VT1 is reserved for the DEV_ONLY greetd/Cage bootstrap by masking `getty@tty1.service`. Remove the disable/mask lines in `image/live-build/config/hooks/normal/0300-greetd.hook.chroot` to restore a tty1 login, then rebuild and repeat the cold-boot test.
