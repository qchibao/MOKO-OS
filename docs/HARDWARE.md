# MOKO OS v0.1 hardware target

## First architecture
`x86_64 / amd64`, UEFI-oriented live image.

## CPU target
- Intel Core i3/i5/i7/i9 x86-64
- Intel Core Ultra x86-64 models
- Intel Xeon x86-64
- AMD Ryzen 3/5/7/9
- AMD Threadripper
- AMD EPYC

CPU instruction compatibility is usually not the hard part; graphics, Wi-Fi, audio, Bluetooth, suspend, camera, trackpad and firmware are the main certification work.

## Minimum developer-preview target
- 64-bit x86 CPU, 2 cores
- 4 GiB RAM
- 32 GiB storage if installed
- 1280×720 display
- UEFI or legacy BIOS supported by the live image

## Recommended
- 4+ cores
- 8–16 GiB RAM
- SSD 64 GiB+
- 1920×1080+
- Intel or AMD graphics for early certification

## Intel Mac policy
1. VM/QEMU first.
2. Live USB second.
3. External SSD install third.
4. Internal dual boot only after hardware diagnostics and backups.

T2-equipped Intel Macs require a separate compatibility track. Do not claim support until storage, input, Wi-Fi, audio, suspend and boot have been tested on the exact model.

## MOKO Hardware Diagnostics
Launch `MOKO Hardware Diagnostics` from the Launcher, Dock, MOKO Settings or the
allowlisted MOKO AI action. The scan is read-only and reports four evidence
levels: `SUPPORTED`, `PARTIAL`, `UNKNOWN` and `UNSUPPORTED`. `UNKNOWN` is the
required result when the running system cannot provide enough evidence.

The app can explicitly export `moko-hardware-report.json` and
`moko-hardware-report.txt` to the current user's home directory. Reports include
the kernel version, loaded modules and relevant PCI/USB IDs, but exclude serial
numbers, UUIDs, MAC addresses, host names and personal file content.

QEMU is a validation target, not a hardware support claim. Physical PC and
Intel Mac results must be collected from a non-destructive Live USB session and
reviewed per model before changing a compatibility status.

## Live USB Developer Preview
Both legacy BIOS and x86_64 UEFI menus expose `Try MOKO OS`, direct
`Hardware Diagnostics` and `Safe Graphics Mode`. The fallback uses software
rendering for Cage and Qt Quick but still depends on a Linux DRM output; it does
not prove that an untested GPU is supported.

The live image includes NetworkManager, BlueZ, PipeWire/WirePlumber, common
Intel/AMD graphics firmware, Intel/Broadcom Wi-Fi firmware and standard input,
NVMe and USB kernel support. Presence in the image is not certification for a
specific machine. Use `docs/LIVE_USB_CHECKLIST.md`, export both diagnostics
reports, and leave any unverified subsystem `UNKNOWN`.

The v0.1.1 Control Center reads those same real services. QEMU validates service
discovery and a virtual HDA PipeWire control path, but cannot certify MacBook
Wi-Fi association, Bluetooth pairing, Apple backlight writes, battery health or
power-profile authorization. Those controls must be exercised in the next
non-destructive physical Live USB pass.
