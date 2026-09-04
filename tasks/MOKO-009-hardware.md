# MOKO-009 — Hardware diagnostics
State: DONE (validated)

Implement `moko-hw-report` to collect non-secret compatibility data: CPU, GPU, PCI/USB IDs, kernel, firmware/driver names, audio devices, network adapters and display/input inventory.

Redact serial numbers, MAC addresses and usernames by default.

## Implementation
- Native Qt 6/QML `moko-hardware-diagnostics` app with a MOKO-owned read-only probe.
- Evidence-backed system, CPU, memory, graphics, storage, network, Bluetooth,
  audio, input, power and Intel Mac sections.
- Explicit `SUPPORTED`, `PARTIAL`, `UNKNOWN` and `UNSUPPORTED` states without
  optimistic support claims.
- Privacy-safe JSON/text export with kernel, loaded modules and relevant PCI/USB IDs.
- Shared registry integration in Launcher/Dock, a Settings entry routed through
  `org.moko.Applications1`, and an allowlisted MOKO AI intent.

## Validation
- Debian 13 reference suites: Shell `2/2`, AI `3/3`, apps `8/8`.
- Real ISO/QEMU Launcher search opened the app as UID 1000 and rendered the QEMU
  Q35 inventory through the MOKO graphical session.
- QEMU report marker: `overall=PARTIAL`, `manufacturer=QEMU`, `architecture=x86_64`,
  `writable_disk_detected=0`.
- Emulated mouse clicks wrote both reports as UID 1000; Settings and MOKO AI each
  launched the same registered app ID.
- Final ISO hash and 3/3 regression evidence are recorded in `STATUS.md`.
