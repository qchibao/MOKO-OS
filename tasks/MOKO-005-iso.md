# MOKO-005 — Bootable ISO validation
State: TODO

Build `MOKO-OS-v0.1-dev-amd64.hybrid.iso`, boot it in QEMU, record build/boot issues and iterate.

## Acceptance
- ISO boots to MOKO Shell in QEMU;
- cold boot repeated 3 times;
- network interface is visible;
- no destructive installer is present;
- resulting SHA256 is recorded in `out/SHA256SUMS`.
