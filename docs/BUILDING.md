# Building MOKO OS v0.1

## Supported build host for the first milestone
Debian 13 amd64 is the reference host. A Debian 13 VM is recommended when developing from macOS.

## Shell dependencies
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts
```

Build:
```bash
./scripts/build-shell.sh
```

From macOS or another Docker host, validate against the Debian 13 reference
environment without installing Qt on the host:

```bash
./scripts/test-shell-debian.sh
```

## ISO dependencies
```bash
sudo apt install -y live-build debootstrap squashfs-tools xorriso rsync
```

Build:
```bash
sudo ./scripts/build-iso.sh
```

From macOS or another Docker host, run the same Debian 13 build in a
privileged container:

```bash
./scripts/build-iso-docker.sh
```

The wrapper copies only the source/assets required by the live image into the live-build chroot include tree, then lets a chroot hook compile/install `moko-shell`. Downloaded live-build artifacts are retained in the `moko-os-live-build-cache` Docker volume so later clean builds can reuse them.

Run a repeatable headless QEMU smoke test from macOS or Linux:

```bash
./scripts/test-iso-docker.sh out/MOKO-OS-v0.1-dev-amd64.hybrid.iso
MOKO_BOOT_RUNS=3 ./scripts/test-iso-docker.sh
MOKO_LAUNCH_QUERY=terminal MOKO_LAUNCH_APP_ID=org.moko.Terminal ./scripts/test-iso-docker.sh
```

Each run waits for the live image's `MOKO_HEALTH` marker, captures the serial
log and polls the framebuffer until MOKO Shell has rendered in `out/`, then
verifies an ACPI shutdown through the live-media removal prompt. Override the
boot and framebuffer limits with `MOKO_BOOT_TIMEOUT` and
`MOKO_SCREENSHOT_TIMEOUT`; defaults are 300 and 180 seconds. The test never
creates or attaches a writable disk.

The Docker QEMU path defaults to multi-threaded TCG with a 2 GiB translation
block cache. Override it with `MOKO_QEMU_ACCEL` only when comparing emulator
configurations; the timeout and health assertions remain unchanged.

The optional launcher variables drive the visible launcher through QEMU keyboard
input and require its sanitized runtime event to report the selected process as
running before a second framebuffer capture is accepted. The capture waits 12
seconds by default for a newly mapped Wayland surface; use
`MOKO_LAUNCH_SETTLE_SECONDS` when profiling unusually slow emulation.
Set `MOKO_REQUIRE_APP_READY=1` for native MOKO apps; the test then requires an
in-process readiness marker in addition to the Shell's process-start marker.

## macOS Intel
Do not run Debian `live-build` directly on macOS. Use the Docker wrapper, a
Debian 13 VM, a dedicated Linux machine, or a suitable Linux CI runner; then
test the resulting ISO with QEMU on macOS using HVF acceleration.
