# MOKO-005 — Bootable ISO validation
State: DONE (validated)

Build `MOKO-OS-v0.1-dev-amd64.hybrid.iso`, boot it in QEMU, record build/boot issues and iterate.

## Acceptance
- ISO boots to MOKO Shell in QEMU;
- cold boot repeated 3 times;
- network interface is visible;
- no destructive installer is present;
- resulting SHA256 is recorded in `out/SHA256SUMS`.

## Validation
- 2026-09-04: `./scripts/build-iso-docker.sh` completed a clean Debian 13 live-build and produced `out/MOKO-OS-v0.1-dev-amd64.hybrid.iso` (1.1 GiB).
- SHA-256: `f656f962f4dae9f13dd1049f24425baf53bbf1f3d52109c561cc67e28a5c05e6`, recorded in `out/SHA256SUMS`.
- Three cold boots passed in QEMU TCG; screenshots and serial/debug logs are `out/moko-iso-smoke-20260904T144109Z-boot-{1,2,3}.*`.
- The ISO has a five-second BIOS/GRUB timeout, no writable disk attached during tests and `--debian-installer none`; package/file audits found no destructive installer or full GNOME/KDE desktop.

## Rollback
Keep the previous ISO/hash as a separate artifact when changing boot or image configuration. Rebuild from the prior source revision and rerun all three cold boots before replacing `out/SHA256SUMS`.
