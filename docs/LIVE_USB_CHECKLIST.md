# MOKO OS v0.1.1 Live USB test checklist

Use this checklist only with the non-installing Developer Preview. Keep a full
backup of the test machine. Do not partition, format or mount an internal disk
for write access during this test.

## Image and boot safety

- [ ] Confirm the ISO SHA-256 matches `SHA256SUMS`.
- [ ] Confirm the machine is x86_64 and record its exact manufacturer/model.
- [ ] Boot `Try MOKO OS` through UEFI when the machine supports it.
- [ ] Confirm normal boot shows only the black MOKO sphere, pulsing highlight
      and stable loading bar, with no Debian/Linux/systemd/debug text or cursor.
- [ ] Confirm MOKO Shell appears and accepts keyboard and pointer input.
- [ ] Open Files, Settings, Terminal and Browser together; move, resize,
      minimize, restore, maximize, snap and switch between their real windows.
- [ ] Reboot and open `Hardware Diagnostics` directly from the boot menu.
- [ ] Reboot and confirm `Safe Graphics Mode` reaches MOKO Shell.
- [ ] Open MOKO Hardware Diagnostics and export both report formats.
- [ ] Run `findmnt` and confirm internal SSD/HDD partitions have no mountpoint.
- [ ] Run `lsblk -o NAME,TYPE,TRAN,SIZE,FSTYPE,MOUNTPOINTS,RO,RM,MODEL` and
      confirm only the live medium/root filesystem was mounted automatically.
- [ ] Confirm no installer, partitioning or formatting action is offered.

## Display and graphics

- [ ] Internal display and every connected external display are visible.
- [ ] Native panel resolution and expected refresh rate are available.
- [ ] Brightness control is reported accurately; note if it is read-only.
- [ ] Brightness slider and the keyboard brightness keys change the real panel
      backlight without requiring elevated application privileges.
- [ ] Hardware Diagnostics reports the correct GPU vendor/model and driver.
- [ ] Record the active renderer and Wayland renderer.
- [ ] Check for corruption, flicker, black frames and cursor artifacts.
- [ ] Try every exposed scale/output mode; confirm the UI remains reachable,
      the confirmation countdown appears, timeout reverts, and Keep preserves
      only the current Live-session setting.

## Input

- [ ] Built-in and USB keyboard keys work, including modifiers and function keys.
- [ ] Mouse movement, primary/secondary click and wheel scrolling work.
- [ ] Trackpad movement, tap-to-click, physical click, secondary click,
      two-finger scrolling, drag and palm rejection work.
- [ ] Natural scrolling and pointer acceleration controls apply immediately.
- [ ] Three-finger drag moves a normal app window without disrupting vertical
      two-finger scrolling.
- [ ] Two-finger horizontal swipe navigates Browser history back and forward.
- [ ] Record whether the Apple keyboard/trackpad is detected on an Intel Mac.

## Network and Bluetooth

- [ ] Ethernet obtains a connection and transfers data when present.
- [ ] Wi-Fi device, kernel driver and firmware status are reported accurately.
- [ ] Wi-Fi can discover and connect to a test network without exposing secrets
      in the exported report.
- [ ] Opening Network immediately shows cached state and begins discovery
      without a hidden prerequisite click; the UI stays responsive while scanning.
- [ ] Wi-Fi can disconnect, reconnect and recover after closing Control Center.
- [ ] Bluetooth controller, driver and BlueZ status are reported accurately.
- [ ] A Bluetooth test device can be discovered and connected.
- [ ] Opening Bluetooth immediately shows cached devices and starts discovery;
      complete any PIN/passkey confirmation using the MOKO dialog.
- [ ] The Bluetooth device can disconnect, reconnect and be forgotten with
      confirmation.

## Audio, microphone and camera

- [ ] `wpctl status` shows the expected PipeWire/WirePlumber devices.
- [ ] Speaker or headphone output is audible on the selected device.
- [ ] Output volume and mute state respond correctly.
- [ ] Output and input device selection remains active after reopening Control
      Center.
- [ ] `arecord -l` lists the expected microphone/capture device.
- [ ] A short non-sensitive microphone test records and plays back correctly.
- [ ] `v4l2-ctl --list-devices` reports the built-in/USB webcam.
- [ ] Webcam video is stable at a supported resolution.

## Power, storage and lifecycle

- [ ] Battery percentage and charging state match the hardware indication.
- [ ] Plug and unplug power; confirm the top-bar charging icon and Power page
      change promptly from the same source of truth.
- [ ] Select `Asia/Ho_Chi_Minh`; confirm Settings, top bar and Notifications use
      the same local time with no incorrect UTC/GMT label.
- [ ] Battery health and power mode are truthful where the hardware exposes
      them; unavailable values remain read-only.
- [ ] Suspend is reported by systemd-logind and completes successfully.
- [ ] Resume restores display, compositor windows, trackpad, Wi-Fi, Bluetooth,
      audio, Browser state and battery status.
- [ ] USB storage is detected without modifying internal storage.
- [ ] Internal NVMe/SATA storage model and capacity are detected read-only.
- [ ] Normal shutdown powers the machine off cleanly.
- [ ] Confirm shutdown fades to black and remains completely black, with no
      text, logo, spinner or console flash, until poweroff.
- [ ] Reboot returns to firmware/boot selection cleanly.

## Browser, AI and desktop services

- [ ] MOKO AI accepts a typed request, shows Processing, and returns real
      battery, network, storage or system information.
- [ ] MOKO AI opens Files, Settings and Terminal only through allowlisted app
      actions; arbitrary shell requests remain unavailable.
- [ ] MOKO Browser renders an HTTPS site, executes JavaScript and downloads a
      non-sensitive test file with visible progress.
- [ ] The downloaded file appears in MOKO Files and opens through the system
      MIME handler.
- [ ] Double-click a compatible downloaded `.deb`; confirm MOKO Package
      Installer shows its real name, version, architecture, size, publisher and
      trust warning before any authorization request.
- [ ] Cancel once and confirm no package is installed; repeat, explicitly
      confirm Install, and verify the package result is truthful. Confirm the
      installation disappears after reboot because Live mode has no persistence.
- [ ] Open a local `.rpm` and confirm MOKO reports it as unsupported without an
      installation or conversion attempt.
- [ ] Text clipboard copy/paste works between MOKO Terminal, Browser and native
      MOKO text fields.
- [ ] File copy/cut/paste works in MOKO Files with confirmation for destructive
      operations.
- [ ] Notification Center receives a test notification and the screenshot
      shortcut creates a real image.
- [ ] English/Vietnamese layout switching and configured HiDPI scale remain
      usable after application launches and resume.
- [ ] Switch Light, Dark and Glass appearance modes live; confirm contrast stays
      readable and Safe Graphics reduces/disables expensive blur.

## Result record

- [ ] Save `moko-hardware-report.json` and `moko-hardware-report.txt`.
- [ ] Record ISO SHA-256, build information, machine model and firmware version.
- [ ] Mark unverified devices `UNKNOWN`; do not promote them to `SUPPORTED`.
- [ ] File one issue per failed subsystem with only non-secret logs and IDs.
