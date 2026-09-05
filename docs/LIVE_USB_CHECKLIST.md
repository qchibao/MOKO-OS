# MOKO OS v0.1 Live USB test checklist

Use this checklist only with the non-installing Developer Preview. Keep a full
backup of the test machine. Do not partition, format or mount an internal disk
for write access during this test.

## Image and boot safety

- [ ] Confirm the ISO SHA-256 matches `SHA256SUMS`.
- [ ] Confirm the machine is x86_64 and record its exact manufacturer/model.
- [ ] Boot `Try MOKO OS` through UEFI when the machine supports it.
- [ ] Confirm MOKO Shell appears and accepts keyboard and pointer input.
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
- [ ] Hardware Diagnostics reports the correct GPU vendor/model and driver.
- [ ] Record the active renderer and Wayland renderer.
- [ ] Check for corruption, flicker, black frames and cursor artifacts.

## Input

- [ ] Built-in and USB keyboard keys work, including modifiers and function keys.
- [ ] Mouse movement, primary/secondary click and wheel scrolling work.
- [ ] Trackpad movement, click, two-finger scrolling and palm rejection work.
- [ ] Record whether the Apple keyboard/trackpad is detected on an Intel Mac.

## Network and Bluetooth

- [ ] Ethernet obtains a connection and transfers data when present.
- [ ] Wi-Fi device, kernel driver and firmware status are reported accurately.
- [ ] Wi-Fi can discover and connect to a test network without exposing secrets
      in the exported report.
- [ ] Bluetooth controller, driver and BlueZ status are reported accurately.
- [ ] A Bluetooth test device can be discovered and connected.

## Audio, microphone and camera

- [ ] `wpctl status` shows the expected PipeWire/WirePlumber devices.
- [ ] Speaker or headphone output is audible on the selected device.
- [ ] Output volume and mute state respond correctly.
- [ ] `arecord -l` lists the expected microphone/capture device.
- [ ] A short non-sensitive microphone test records and plays back correctly.
- [ ] `v4l2-ctl --list-devices` reports the built-in/USB webcam.
- [ ] Webcam video is stable at a supported resolution.

## Power, storage and lifecycle

- [ ] Battery percentage and charging state match the hardware indication.
- [ ] Suspend is reported by systemd-logind and completes successfully.
- [ ] Resume restores display, input, network and audio.
- [ ] USB storage is detected without modifying internal storage.
- [ ] Internal NVMe/SATA storage model and capacity are detected read-only.
- [ ] Normal shutdown powers the machine off cleanly.
- [ ] Reboot returns to firmware/boot selection cleanly.

## Result record

- [ ] Save `moko-hardware-report.json` and `moko-hardware-report.txt`.
- [ ] Record ISO SHA-256, build information, machine model and firmware version.
- [ ] Mark unverified devices `UNKNOWN`; do not promote them to `SUPPORTED`.
- [ ] File one issue per failed subsystem with only non-secret logs and IDs.
