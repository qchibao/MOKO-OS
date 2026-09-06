# Building MOKO OS v0.1.1

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
./scripts/test-iso-docker.sh out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso
MOKO_BOOT_RUNS=3 ./scripts/test-iso-docker.sh
MOKO_LAUNCH_QUERY=terminal MOKO_LAUNCH_APP_ID=org.moko.Terminal ./scripts/test-iso-docker.sh
MOKO_AI_PROMPT="system overview" MOKO_AI_EXPECT_ACTION=system_summary ./scripts/test-iso-docker.sh
MOKO_AI_PROMPT="open files" MOKO_AI_EXPECT_ACTION=open_application \
  MOKO_AI_EXPECT_APP_ID=org.moko.Files ./scripts/test-iso-docker.sh
MOKO_LAUNCH_QUERY=diagnostics \
  MOKO_LAUNCH_APP_ID=org.moko.HardwareDiagnostics \
  MOKO_REQUIRE_APP_READY=1 MOKO_EXPECT_HARDWARE_REPORT=1 \
  ./scripts/test-iso-docker.sh
MOKO_LAUNCH_QUERY=appearance MOKO_LAUNCH_APP_ID=org.moko.Settings \
  MOKO_REQUIRE_APP_READY=1 MOKO_SETTINGS_OPEN_HARDWARE=1 \
  ./scripts/test-iso-docker.sh
MOKO_AI_PROMPT="open hardware diagnostics" MOKO_AI_EXPECT_ACTION=open_application \
  MOKO_AI_EXPECT_APP_ID=org.moko.HardwareDiagnostics ./scripts/test-iso-docker.sh
MOKO_CONTROL_CENTER_TEST=1 ./scripts/test-iso-docker.sh
MOKO_LAUNCH_QUERY=files MOKO_LAUNCH_APP_ID=org.moko.Files \
  MOKO_REQUIRE_APP_READY=1 MOKO_WINDOW_WORKFLOW=1 ./scripts/test-iso-docker.sh
MOKO_LAUNCH_QUERY=browser MOKO_LAUNCH_APP_ID=org.moko.Browser \
  MOKO_REQUIRE_APP_READY=1 MOKO_BROWSER_TEST=1 ./scripts/test-iso-docker.sh
MOKO_BOOT_RUNS=3 MOKO_BOOT_FIRMWARE=uefi MOKO_BOOT_TIMEOUT=480 \
  ./scripts/test-iso-docker.sh
MOKO_BOOT_MODE=hardware-diagnostics ./scripts/test-iso-docker.sh
MOKO_BOOT_MODE=safe-graphics ./scripts/test-iso-docker.sh
MOKO_BOOT_FIRMWARE=uefi ./scripts/test-iso-docker.sh
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

`MOKO_BOOT_MODE` accepts only `desktop`, `hardware-diagnostics` or
`safe-graphics`. `MOKO_BOOT_FIRMWARE` accepts only `bios` or `uefi`; UEFI tests
use a fresh writable copy of the OVMF variable store inside the disposable QEMU
container. Every run statically checks both boot menus, the x86_64 EFI loader,
required hardware packages, release metadata, installer/automounter absence and
the live disk-safety units before starting QEMU. Runtime health must report the
requested mode, firmware, renderer, zero greetd restarts and zero unexpected
block mounts.

The optional launcher variables drive the visible launcher through QEMU keyboard
input and require its sanitized runtime event to report the selected process as
running before a second framebuffer capture is accepted. The capture waits 12
seconds by default for a newly mapped Wayland surface; use
`MOKO_LAUNCH_SETTLE_SECONDS` when profiling unusually slow emulation.
Set `MOKO_REQUIRE_APP_READY=1` for native MOKO apps; the test then requires an
in-process readiness marker in addition to the Shell's process-start marker.
The hardware mode additionally checks the read-only QEMU inventory, clicks the
visible JSON/text export controls through the emulated USB tablet and requires
both reports to be written by UID 1000. The Settings mode clicks its Hardware
Diagnostics entry and verifies that the launch returns through the shared MOKO
application registry.

The optional AI variables drive the real Shell panel through QEMU keyboard
input. They require an unprivileged daemon connection, the expected D-Bus
response, and, for application actions, the registry launch, in-process app
readiness and compositor surface mapping. The test captures the resulting AI
panel or application framebuffer as `*-ai.png`.

`MOKO_CONTROL_CENTER_TEST=1` clicks the visible top-bar controls, requires the
unprivileged backend to report real NetworkManager, Bluetooth-hardware,
PipeWire, backlight, battery and power-profile availability, then toggles the
actual default PipeWire sink mute state. QEMU intentionally has no Wi-Fi,
Bluetooth, backlight or battery device, so zero availability for those devices
is a valid and required truthful result; the virtual HDA endpoint must still be
discovered and controlled.

`MOKO_BROWSER_TEST=1` requires the Browser launcher query, application ID and
readiness check shown above. It verifies that WebEngine starts unprivileged with
its sandbox and web security enabled, renders a real HTTPS page, executes a
JavaScript marker, downloads Debian's `hello` package with progress, and opens
the resulting `~/Downloads` location in a simultaneous MOKO Files window. The
test uses the network and filesystem for real; it does not replace those steps
with a fixture. Browser state and downloads disappear when the non-persistent
Live session ends.

## Release artifacts
A successful build writes these files to `out/`:

```text
MOKO-OS-v0.1.1-dev-amd64.hybrid.iso
MOKO-OS-v0.1.1-dev-amd64.build-info.txt
MOKO-OS-v0.1.1-dev-amd64.packages.txt
MOKO-OS-v0.1.1-dev-amd64.known-issues.txt
MOKO-OS-v0.1.1-dev-amd64.live-usb-checklist.md
SHA256SUMS
```

The build timestamp derives from `SOURCE_DATE_EPOCH`, normally the recorded git
commit time. Build info, known issues, package manifest and the physical
checklist are also placed under `/MOKO` on the ISO. The live-build package
manifest remains available at `/live/filesystem.packages`, and the release test
requires both copies to match byte-for-byte.

To verify reproducibility, preserve the first clean artifact, rebuild from the
same clean commit, then compare the two images:

```bash
cp out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso out/repro-build-a.iso
./scripts/build-iso-docker.sh
./scripts/compare-iso-builds.sh \
  out/repro-build-a.iso out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso
```

## Validated v0.1.1 release candidate

The QEMU release gate completed on 2026-09-06 for source commit
`d42b2bb70e893dbc6068ed3405fb83c8db831cf5`:

```text
ISO: out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso
SHA-256: 4bcb3baee0a268175fb5b6e0461a77010d3eaca400ce6ede9bfa4f9b81a06ed8
Build timestamp: 2026-09-06T09:33:10Z
Reproducibility: two clean builds were byte-identical
```

The counted gates use separate boots for long pointer-driven workflows. This
keeps a missed emulated click from invalidating unrelated checks while every
run still repeats ISO preflight, disk safety, graphical health and shutdown.
The final set passed BIOS `3/3`, UEFI `3/3`, Control Center, input/usability,
AI/Terminal, Files/Settings multi-window, Browser network/download, Safe
Graphics, direct and Launcher Diagnostics exports, and standard-VGA
suspend/resume. Exact artifact prefixes are recorded in
`tasks/MOKO-011-hardware-usability.md`.

Diagnostics export automation must complete the MOKO Save dialog: click one
export button, wait for the dialog, accept the suggested filename, and only
then require `MOKO_HW_EXPORT`. Opening the dialog by itself is not a successful
export.

Debian Chromium, the official Google Chrome amd64 `.deb`, and Tor Browser
15.0.21 were also downloaded/installed and run as UID 1000 in disposable Live
sessions. Chromium and Chrome retained their sandbox helpers and no
`--no-sandbox` policy override was used; Tor Browser connected to Tor and
rendered HTTPS. These packages are not included in the ISO and disappear at
shutdown because Live persistence is not implemented.

This is the QEMU release candidate, not final hardware certification. Complete
`docs/LIVE_USB_CHECKLIST.md` on the Intel MacBook Pro 2015 before marking
MOKO-011 complete. Do not enable the installer during that validation.

## macOS Intel
Do not run Debian `live-build` directly on macOS. Use the Docker wrapper, a
Debian 13 VM, a dedicated Linux machine, or a suitable Linux CI runner; then
test the resulting ISO with QEMU on macOS using HVF acceleration.
