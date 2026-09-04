# MOKO-003 — Debian 13 build environment
State: DONE (validated)

Run `scripts/bootstrap-debian.sh` on a clean Debian 13 amd64 VM, then compile `moko-shell`.

## Acceptance
- all package names resolve;
- CMake configure succeeds;
- Ninja build succeeds;
- shell launches under Wayland.

## Validation
- 2026-09-04: `./scripts/test-shell-debian.sh` built the Debian 13 amd64 development image, resolved the Qt 6 dependencies, configured CMake and linked `moko-shell` successfully.
- CTest passed `1/1`; a Weston headless Wayland session launched the shell and produced a non-empty 1280x720 screenshot.
- `./scripts/build-iso-docker.sh` also compiled and installed the release shell inside the Debian 13 live-build chroot before purging build-only dependencies.

## Rollback
The Docker validation environment is disposable. Remove the local `moko-os-debian13-dev` image or revert `tools/debian-dev` and rerun the test; no host Qt packages are installed.
