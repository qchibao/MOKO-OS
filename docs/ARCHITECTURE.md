# MOKO OS v0.1 architecture

## Product boundary
MOKO OS v0.1 is **not** a Linux distribution with only wallpaper/icon/theme changes. Linux and Debian provide the hardware/userland foundation while MOKO owns the product-facing system layer.

```text
Applications
  MOKO Files / Settings / Terminal / Store / AI
                 │
MOKO UX + Framework
  MOKO Shell / Design Tokens / App APIs
                 │
MOKO Core Services
  session / settings / power / permissions / search / AI actions
                 │
System Infrastructure
  D-Bus / systemd / NetworkManager / PipeWire / BlueZ
                 │
Wayland compositor
  Bootstrap: Cage        Target: moko-compositor
                 │
Debian minimal userland
                 │
Linux kernel + drivers
                 │
Hardware
```

## Why Debian is underneath
The v0.1 goal is to spend engineering effort on the MOKO product layer instead of reimplementing mature kernel drivers, filesystems, power management and networking. Debian is an implementation base, not the visible desktop identity.

## Shell boundaries
`moko-shell` owns:
- top bar;
- app launcher;
- dock;
- overview/workspace experience;
- notification center;
- control center;
- AI surface;
- lock screen UI (after authentication backend is defined).

It should eventually consume system state through MOKO D-Bus services rather than calling privileged commands directly.

## Compositor plan
### Bootstrap stage
Use Cage only to give the full-screen MOKO Shell a deterministic Wayland environment for Developer Preview builds.

### v0.1.1 transition
`moko-compositor` is a MOKO-owned wlroots compositor. It owns XDG toplevel
placement, focus, move/resize, minimize, maximize, fullscreen, snapping and
global window switching. The Shell remains a separate unprivileged Wayland
client and consumes compositor window state instead of simulating window
operations.

Normal desktop mode moves to `moko-compositor` only after its multi-window
tests and ISO boot gate pass. The validated Cage session remains available as
an explicit rollback path and continues to back Safe Graphics while the new
renderer path is qualified on physical hardware.

### Native MOKO stage
Extend `moko-compositor` with workspaces, shell protocols, multi-monitor
policy, effects and accessibility without moving policy into application UIs.

## Security boundary
The AI UI is unprivileged. Privileged system actions require a MOKO action broker with explicit capability checks/polkit-style authorization. AI provider credentials belong in the user secret store, never in shell QML or ISO source.

## MOKO AI v0.1 boundary
`moko-ai-ui` is a Shell-side controller that talks to `moko-ai-daemon` over the
user session bus at `org.moko.AI1`. The daemon interprets requests through an
`AiProvider`, then dispatches only named capabilities through `moko-ai-actions`.
The local provider is deterministic and offline; the remote provider remains an
interface and must obtain future credentials from user configuration or secret
storage.

Application actions are restricted to an explicit MOKO allowlist and route back
through the Shell-owned `org.moko.Applications1` registry API. File actions
canonicalize existing paths and remain inside the current user's home. System
status reads NetworkManager D-Bus, `/proc`, `/sys` and mounted-filesystem APIs.
There is no shell-command action and model/provider output cannot create a
process directly.

## Hardware diagnostics boundary
`moko-hardware-diagnostics` is an unprivileged Qt 6/QML app backed by a
MOKO-owned C++ probe. It reads procfs/sysfs, system D-Bus and fixed inspection
utilities for CPU, memory, graphics, storage, network, Bluetooth, audio, input,
power and Intel Mac evidence. Compatibility states remain `UNKNOWN` when the
available evidence is insufficient; the app never claims hardware support from
device identity alone.

The probe is read-only and does not mount, partition or format storage. JSON and
text exports omit serial numbers, UUIDs, MAC addresses, host names and personal
file content. Settings launches the app through the Shell-owned
`org.moko.Applications1` registry, and MOKO AI can request the same allowlisted
application ID without gaining a general process or shell capability.

## Control Center boundary
`moko-shell` owns the Control Center UI and exposes consumer-facing state without
showing raw D-Bus or PipeWire internals. Its unprivileged `SystemControl`
controller talks directly to the standard system services and kernel interfaces:

- NetworkManager D-Bus for Wi-Fi radio state, scans, access points, connection
  creation and disconnect;
- BlueZ D-Bus for adapter power, discovery, pairing, connection and device
  removal, with a MOKO-owned user confirmation agent;
- fixed `wpctl` argument vectors for PipeWire/WirePlumber volume, mute and
  endpoint selection;
- `brightnessctl` or a writable `/sys/class/backlight` device for display
  brightness;
- read-only `/sys/class/power_supply` battery data and the standard Power
  Profiles D-Bus service where available.

UI text never becomes a command. The only helper processes use fixed executable
names and structured argument lists, and failures remain visible to the user.
Hardware that is absent or not writable is reported as unavailable rather than
simulated. The compositor forwards only bounded brightness-step events for the
XF86 brightness keys; the same unprivileged controller performs the adjustment.

## Input boundary
`moko-compositor` applies desktop input policy directly through libinput. It
discovers touchpads by capability, not vendor/product ID, and enables supported
tap, drag, two-finger scroll, clickfinger secondary click, adaptive acceleration
and disable-while-typing features. Libinput continues to own device-specific
palm detection and hardware quirks.

The versioned `moko_window_manager_v1` protocol reports aggregate capabilities
and applied state to the Shell Input page. Its only mutable input requests are a
bounded natural-scroll boolean and acceleration value; older version 1 clients
retain the window-management contract. Standard Wayland pointer gesture events
are forwarded for applications and future MOKO workspace gestures. Cage remains
the rollback and Safe Graphics input path until physical MacBook validation is
complete.

## Live USB boot boundary
The Developer Preview exposes three fixed profiles through both GRUB/UEFI and
ISOLINUX/legacy BIOS: `desktop`, `hardware-diagnostics` and `safe-graphics`.
The kernel command line carries only the `moko.mode` enum. `moko-cage-session`
validates that enum before Cage starts; no menu value becomes a command or an
arbitrary environment assignment.

The diagnostics profile opens the native read-only app directly and starts the
normal Shell if the app is closed. Safe Graphics Mode forces the Cage pixman
renderer, disables hardware cursors and uses Qt Quick software rendering before
any user-visible MOKO process starts. It is a compatibility fallback, not a
replacement desktop or a support claim.

`moko-live-disk-safety.service` runs before greetd and fails the graphical boot
health gate if a block device other than the live medium was mounted during
startup. A greetd unit dependency also prevents the graphical session from
starting after a failed audit. The image does not include an installer or
`udisks2`, masks the udisks2 service defensively, and QEMU validation never
attaches a writable disk.

The audit's exit status is independent from diagnostic output. Normal service
output is handled by the systemd journal. A `/dev/ttyS0` mirror exists only for
QEMU test observability, is used only when it is a writable character device,
and ignores open or write errors. Missing or unusable serial hardware can never
turn a passing storage audit into a failed boot gate.

## Architecture targets
- v0.1: amd64 only.
- Later: arm64 feasibility track after desktop APIs stabilize.
- No compatibility promise for Apple silicon in v0.1.
