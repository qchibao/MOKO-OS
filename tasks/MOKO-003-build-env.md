# MOKO-003 — Debian 13 build environment
State: READY

Run `scripts/bootstrap-debian.sh` on a clean Debian 13 amd64 VM, then compile `moko-shell`.

## Acceptance
- all package names resolve;
- CMake configure succeeds;
- Ninja build succeeds;
- shell launches under Wayland.
