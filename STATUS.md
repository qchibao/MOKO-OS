# MOKO OS status

Updated: 2026-09-12

| Task | State | Notes |
|---|---|---|
| MOKO-001 Repository bootstrap | DONE | Structure, engineering rules and docs created. |
| MOKO-002 Shell Developer Preview | DONE (validated) | Builds and renders under Debian 13 Wayland at 1280x720 and in the live ISO at 1280x800. |
| MOKO-003 Debian build environment | DONE (validated) | Dockerized Debian 13 CMake/Ninja build and CTest smoke test pass. |
| MOKO-004 Live Wayland developer session | DONE (validated) | greetd/Cage reaches MOKO Shell with network/input and zero greetd restarts across 3 cold boots. |
| MOKO-005 Live ISO validation | DONE (validated) | 3/3 QEMU boots and clean shutdowns pass; SHA-256 `f656f962f4dae9f13dd1049f24425baf53bbf1f3d52109c561cc67e28a5c05e6`. |
| MOKO-006 Real app launcher | DONE (validated) | MOKO registry/search/shared Dock model launches a real app without shell evaluation; 3/3 cold boots pass on ISO SHA-256 `a9beeb87f1ba7683f073ab03d75e6798686d3ef5ce0143bc8942f6146a694790`. |
| MOKO-007 Real MOKO system apps | DONE (validated) | Native Files, Settings and PTY Terminal launch from the shared registry/Dock as UID 1000; 3/3 cold boots pass on ISO SHA-256 `c3c2b42568bf44034fee97cef968bad9649c023b6fed04f7336023e8fa96e155`. |
| MOKO-008 MOKO AI system action contract | DONE (validated) | Unprivileged D-Bus daemon, allowlisted action layer and provider-backed Shell UI pass real system-summary and app-launch tests; 3/3 cold-boot regression passes on ISO SHA-256 `4c1a00c43bf0060198b998c6978d7cf178d0eac0b72673572beab50a8ac4fb9f`. |
| MOKO-009 Hardware diagnostics | DONE (validated) | Native read-only diagnostics, privacy-safe JSON/text exports and Launcher/Settings/AI integration pass in QEMU; 3/3 cold boots pass on ISO SHA-256 `9c6608a10bbb705a09c62caa0a31d4be38a514df6d550f796146329cf799749e`. |
| MOKO-010 Live USB readiness | DONE (validated in QEMU) | The MacBook 2015 serial false-failure is fixed without weakening disk safety. Two clean builds are identical; BIOS `3/3`, UEFI, Safe Graphics, Diagnostics and shutdown pass on SHA-256 `0427dc95224f582184ca3a61f54b2a1750a5308b2edec41c72b0da0dd5da1483`. Physical MacBook retest remains pending. |
| MOKO-011 v0.1.1 Hardware & Usability Preview | IN PROGRESS | Physical hotfix H1-H8 implementation and automated QEMU gates are complete. Final clean/reproducible artifact generation and physical MacBook Pro 2015 validation remain required before completion. |

## Current release target
A reproducible `MOKO-OS-v0.1.1-dev-amd64.hybrid.iso` that reaches the MOKO Hardware & Usability Preview in QEMU, preserves disk safety and supports the validated multi-window, system-control, Browser, AI and suspend/resume workflows. The remaining gate is physical validation on the Intel MacBook Pro 2015.

## v0.1.1 physical hotfix

The hotfix branch preserves the previous QEMU release behavior while applying
the physical-test specification in phases H1-H8. Normal boot and shutdown are
now consumer-only visual paths, display changes have timed rollback, Vietnam
timezone and charging state are truthful, trackpad gestures are compositor
owned, connectivity pages preload real state, Settings/Files/Browser match the
locked references, and downloaded Debian packages open in a narrowly scoped
MOKO review/install flow. MOKO AI remains frozen and the OS installer remains
disabled. The H8 component suite and isolated QEMU functional gates pass with
zero unexpected block mounts and black shutdown frames. The Intel MacBook Pro
2015 remains the final authority for physical validation.

## Physical hotfix automated gate

- Debian 13 component suite: compositor `4/4`, Shell `8/8`, existing AI `3/3`
  and native apps `15/15` passed; Browser network and Qt multi-window sessions
  also passed.
- Three independent BIOS desktop boots and three UEFI desktop boots passed disk
  safety, graphical health, zero greetd restarts and clean shutdown.
- AI/Terminal, real multi-window state, Control Center, input/usability,
  Browser HTTPS/JavaScript/download/temporary `.deb` install, Safe Graphics,
  direct and Launcher Diagnostics, and QEMU suspend/resume passed.
- MOKO-011 stays `IN PROGRESS` until the locked physical checklist passes on
  the Intel MacBook Pro 2015. MOKO-012 remains disabled.

## Boot-time work (unvalidated by a rebuilt ISO)

Status: five changes, committed on `hotfix/v0.1.1-physical-ui` as `4a5a99a`
(locale/timezone), `ec112d6` (unit masking + modprobe hygiene) and `431f48d`
(firmware keep-list). **None has been through an `lb build` yet, so none is
validated.** Every number below was measured on the *shipped* v0.1.1 release
ISO under QEMU/TCG on macOS, not on a rebuilt one.

`main` and tag `v0.1.0` are untouched.

**How it was measured.** The release ISO's own `vmlinuz` + `initrd` were booted
directly with `qemu -kernel/-initrd/-append`, serial captured through a FIFO and
wall-clock timestamped per line. No rebuild was needed to get attribution. Five
runs: baseline, `diag5` (udev-trigger masked), `diag6` (modprobe blacklist),
`diag7` (clean baseline + `live-config.debug`), `diag8` (locale payload
injection).

**What actually dominates** (`diag7`, `systemd-analyze critical-chain`):
`live-config.service @14.735s +58.035s` sits on the critical chain to `greetd`,
and its own per-component split is `0050-locales 36s` + `0070-tzdata 7s`, with
all 33 other components at 0-3s each.

**Changes landed**

1. `0160-moko-locale-tz.hook.chroot` - generate the locale archive at build time
   so live-config's `0050-locales` hits Debian's `locale-gen --keep-existing`
   skip guard instead of recompiling `en_US.UTF-8` from source on every boot.
   Also pre-seeds `Asia/Ho_Chi_Minh`, paired with the matching `timezone=` in
   `auto-config.sh` (without it `0070-tzdata` rewrites `/etc/timezone` and
   deletes `/etc/localtime` on every boot).
2. `auto-config.sh` - `--firmware-chroot false` plus `timezone=` on
   `--bootappend-live`.
3. `config/package-lists/moko.list.chroot` - explicit keep-list for the six
   consumer-laptop firmware packages that `--firmware-chroot false` would
   otherwise drop (Atheros, Realtek, Intel SOF, Intel sound, Cirrus, Marvell).
4. `0300-greetd.hook.chroot` - masks `systemd-udev-settle` and
   `NetworkManager-wait-online`; carries a correction note because an earlier
   revision blamed `systemd-udev-trigger` for ~62s, which was wrong (3.6s).
5. `includes.chroot/etc/modprobe.d/moko-live-boot.conf` - blacklists
   datacentre NIC/HBA modules and legacy parallel-port drivers. Its header
   states plainly that this is hygiene and *not* a measured boot-time win:
   `diag6` A/B'd it and the boot came out slower on every marker, which is TCG
   variance (+-15-30s) rather than a regression from the file. `kvm_amd` /
   `kvm_intel` were in an earlier revision and were removed on purpose - they
   bought nothing measurable while genuinely disabling nested virtualisation
   in the shipped image, which is a capability loss rather than hygiene.

**Honest caveats, both material**

- *TCG inflation.* macOS has no KVM, so every run is emulated. Fork/exec-heavy
  shell phases are inflated roughly 18x: `locale-gen --keep-existing` measured
  2.03s cold and 0.03s warm in an emulated trixie container (a 68x ratio), while
  `0050-locales` measured 36-38s in the VM. The *ratio* is environment
  independent - it is the difference between running `localedef` and not running
  it - so the fix is real. The *absolute* saving is not: on real x86 hardware
  `0050-locales` is probably ~1-2s, not 36s. The same applies to most of
  `live-config`'s 58s. Run-to-run TCG variance is +-15-30s, which is why only
  within-run attribution is quoted here.
- *The transferable wins are the I/O-bound ones.* Firmware is the largest:
  `--firmware-chroot false` removes 22 packages / 281.8 MiB installed, and
  because initramfs-tools copies firmware per included module
  (`hook-functions:128 --firmwaredirs`) those packages leave the initrd too -
  roughly 40 MiB off a 138 MB initrd that must be read off the USB stick before
  the kernel can start. Compressed firmware does not shrink under zstd, so that
  reduction is close to one-for-one on the wire.
- *10s is not reachable for this image.* A Debian-live squashfs USB image has to
  read ~1.2 GB over USB and bring up kernel, initrd, systemd, live-config, DRM,
  a wlroots compositor and a Qt6 Shell. The realistic optimised floor is
  ~20-30s. Getting near 10s requires installing to internal NVMe, which is
  explicitly frozen ("do not enable internal-disk OS installation").

**Test / rollback.** `scripts/test-iso-docker.sh` asserts the presence of all
21 firmware packages now expected and the absence of the 16 datacentre/legacy
ones, so a regression of `--firmware-chroot false` fails the gate loudly.
Rollback per change: delete the `0160-` hook; drop `--firmware-chroot false`
and `timezone=` from `auto-config.sh`; revert `moko.list.chroot`;
`systemctl unmask systemd-udev-settle.service NetworkManager-wait-online.service`;
delete the `modprobe.d` file. None of these touches disk safety, the installer
gate or MOKO AI.

**Verified without a rebuild.** `lb config` was run against both forms in a
trixie container: with `--firmware-chroot false` the value persists as
`config/binary:110 LB_FIRMWARE_CHROOT="false"`, and without it the same line
reads `"true"` (matching the `${LB_FIRMWARE_CHROOT:-true}` default at
`configuration.sh:488`). The package list still resolves to 53 non-comment
entries, so the new comment blocks in `moko.list.chroot` are inert as intended
(`packagelists.sh:122-124` has a `\#*)` case that skips them).

**Still open.** `diag8`'s attempt to A/B the locale fix at boot level failed -
the injected initrd payload never unpacked (`MOKO_DIAG8 PAYLOAD_MISSING`), and
that harness was lost when the host rebooted. The archive the hook bakes in is
byte-for-byte identical to the one `locale-gen` writes at boot (sha256
`4c3b8c3ae5e03113701a5e760ca2135573679e212e048a578d04c1651cee43c0`, 3063024
bytes both), so the payload is verified even though the boot never consumed it.
The cheap way to close this without any injection is
`live-config.nocomponents=locales` on the kernel cmdline.

## Previous v0.1.1 QEMU release candidate

- ISO: `out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso`
- SHA-256: `4bcb3baee0a268175fb5b6e0461a77010d3eaca400ce6ede9bfa4f9b81a06ed8`
- ISO source commit: `d42b2bb70e893dbc6068ed3405fb83c8db831cf5`
- Build timestamp: `2026-09-06T09:33:10Z`
- Reproducibility: two clean builds from the same commit are byte-identical.
- Debian tests: disk safety passed; compositor `4/4`, Shell `8/8`, AI `3/3`, apps `11/11`, Browser network and Qt multi-window sessions passed.
- QEMU: BIOS `3/3`, UEFI `3/3`, Control Center, input/usability, AI/Terminal, Files/Settings multi-window, Browser HTTPS/JavaScript/download, Safe Graphics, Diagnostics export and suspend/resume passed.
- Third-party compatibility: Debian Chromium, Google Chrome and Tor Browser were installed and run temporarily as UID 1000; none is bundled in the ISO.
- Installer and persistence remain disabled; every QEMU run attached no writable disk and reported `unexpected_block_mounts=0`.

The original v0.1 milestone was achieved on 2026-09-04. Validation artifacts: `out/moko-iso-smoke-20260904T144109Z-boot-*`.
