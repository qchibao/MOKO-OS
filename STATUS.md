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

## Boot-time work (built and statically verified, not yet boot-tested)

Status: six changes on `hotfix/v0.1.1-physical-ui` -- `4a5a99a`
(locale/timezone), `ec112d6` (unit masking + modprobe hygiene), `431f48d`
(firmware keep-list) and a sixth, `0250-moko-firmware-trim.hook.chroot`, added
after the first build measured what the keep-list actually produced.

**The first five have been through an `lb build`.** Build of 2026-09-12
(`12m20s`, exit 0, clean tree at `388521d`) produced
`out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso`, sha256
`0f026a4768989d37cd69151f20e4c4e90b488fe73b73d72762438515bda31ca1`. Size and
package results are measured below. **No boot result exists for it yet** -- it
has not been through QEMU or physical hardware, so the boot-time claims are
still unvalidated. The `diag*` numbers further down were measured on the
*shipped* v0.1.1 release ISO under QEMU/TCG on macOS.

`main` and tag `v0.1.0` are untouched.

**Measured result of the 2026-09-12 build**

| | shipped v0.1.1 | rebuilt | delta |
|---|---|---|---|
| `hybrid.iso` | 1,383,333,888 B | 1,295,843,328 B | **-83 MiB** |
| `filesystem.squashfs` | 1,218,408,448 B | 1,135,702,016 B | **-79 MiB** |
| `initrd.img` (compressed) | 135,312 KiB | 129,735 KiB | **-5.4 MiB** |
| `initrd.img` (uncompressed) | 296.8 MiB | 266 MiB | -30.8 MiB |
| firmware in initrd (uncompressed) | 198.1 MiB | 163 MiB | -35.1 MiB |
| firmware/microcode packages | 37 | 22 | -15 |
| total packages | 713 | 691 | -22 |

Baselines are `out/MOKO-OS-v0.1.1-boot-profile.iso` and
`-repro-a.hybrid.iso`, both 2026-09-09 -- *not* a strict A/B, because the
2026-09-12 build overwrote the release ISO at the canonical path. The validated
release candidate survives byte-identically as
`out/MOKO-OS-v0.1.1-dev-amd64.repro-d42-a.iso` (sha256 `4bcb3bae...81a06ed8`,
verified after the fact); the "Previous v0.1.1 QEMU release candidate" section
below now names that file.

`0160-moko-locale-tz.hook.chroot` is confirmed to have *run*: build log line
2986, output `en_US.UTF-8... done` / `Generation complete.`. Whether that
removes the 36-38s `0050-locales` cost at boot is the open question and needs a
QEMU run of this ISO.

`i915` firmware (11 MiB / 43 files) is present in the rebuilt initrd, which is
the check that matters for the Intel MacBook this OS is validated on.

**Two claims from earlier revisions that measurement corrected**

- *"roughly 40 MiB off a 138 MB initrd"* was wrong as stated. The uncompressed
  saving is real (-30.8 MiB) and close to the prediction; the *compressed*
  saving is only -5.4 MiB, because firmware blobs barely compress. The earlier
  text compared an uncompressed prediction against a compressed measurement.
  The mechanism was right, the arithmetic framing was not.
- *`--firmware-chroot false` removes `firmware-marvell-prestera`* was wrong.
  That package never came through `chroot_firmware`. An archive-wide scan of
  every relation field in trixie found exactly one edge into it --
  `firmware-libertas --Recommends--> firmware-marvell-prestera` -- and
  `firmware-libertas` is on the keep-list. `apt-cache showpkg` reports an empty
  Reverse Depends for it because that section lists only `Depends`, never
  `Recommends`, which is what made the provenance look unexplained.
  `0250-moko-firmware-trim.hook.chroot` now purges it (59 MiB; zero packages in
  the archive `Depends` on it, so the purge cascades nowhere), bringing the
  count to 21. **That hook has not been through a build yet**, so the
  2026-09-12 ISO still contains the package and
  `scripts/test-iso-docker.sh` would fail on it until the next build.
- The two biggest blobs are deliberately *not* stripped: `amdgpu` 80 MiB and
  `nvidia` 63 MiB are 88% of the 163 MiB of firmware still in the initrd. A
  failed GPU probe inside the initrd may not recover before `switch_root` and
  can drop to a text console, which violates "normal boot must never show
  Debian/Linux/systemd/debug text". Removing them needs multi-machine physical
  testing, not a QEMU run.

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
6. `0250-moko-firmware-trim.hook.chroot` - purges `firmware-marvell-prestera`,
   the one 59 MiB datacentre package the keep-list drags back in through a
   `Recommends` edge. Numbered `0250` because hooks run *after* every
   `chroot_package-lists` pass (build log: installs at lines 220 and 2661, hooks
   from 2981), and after `0200`'s own apt-get of the Qt6 build toolchain. It
   fails the build rather than shipping silently if `firmware-libertas` did not
   survive the purge.

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
  `--firmware-chroot false` plus `0250-` removes 16 packages (~282 MiB
  installed, ~341 MiB with the Marvell purge), and because initramfs-tools
  copies firmware per included module (`hook-functions:128 --firmwaredirs`) some
  of that leaves the initrd too. Measured, not predicted: -30.8 MiB
  uncompressed but only **-5.4 MiB compressed**, because firmware blobs barely
  compress -- the earlier "close to one-for-one on the wire" claim was wrong in
  the direction that flattered the change. The wire-level win is really the
  **-83 MiB ISO / -79 MiB squashfs**, which is what the USB stick has to read.
- *10s is not reachable for this image.* A Debian-live squashfs USB image has to
  read ~1.2 GB over USB and bring up kernel, initrd, systemd, live-config, DRM,
  a wlroots compositor and a Qt6 Shell. The realistic optimised floor is
  ~20-30s. Getting near 10s requires installing to internal NVMe, which is
  explicitly frozen ("do not enable internal-disk OS installation").

**Test / rollback.** `scripts/test-iso-docker.sh` asserts the presence of all
21 firmware/microcode packages the image should carry and the absence of 15
datacentre/legacy ones, so a regression of `--firmware-chroot false` fails the
gate loudly. `firmware-marvell-prestera` is asserted absent by its own check
with its own message, because attributing it to the flag would point whoever
debugs a failure at the wrong file; `firmware-libertas` is asserted present next
to it so the purge cannot quietly take the Wi-Fi firmware with it. All 21 are now asserted, including
the five that arrive only through the closure rather than the explicit
keep-list (`firmware-linux-free`, `firmware-linux-nonfree`,
`firmware-intel-misc`, `firmware-ath9k-htc`, `firmware-carl9170`) -- an earlier
revision of this file noted the keep-list comment claimed coverage the test did
not actually provide, and the test was tightened to match the claim rather than
the claim being weakened to match the test.
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

- ISO: `out/MOKO-OS-v0.1.1-dev-amd64.repro-d42-a.iso`
- SHA-256: `4bcb3baee0a268175fb5b6e0461a77010d3eaca400ce6ede9bfa4f9b81a06ed8`
  (re-verified against the file on 2026-09-12. The canonical path
  `out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso` was overwritten by the 2026-09-12
  boot-fix build, so this candidate now lives under the repro filename only.
  Nothing validated was lost.)
- ISO source commit: `d42b2bb70e893dbc6068ed3405fb83c8db831cf5`
- Build timestamp: `2026-09-06T09:33:10Z`
- Reproducibility: two clean builds from the same commit are byte-identical.
- Debian tests: disk safety passed; compositor `4/4`, Shell `8/8`, AI `3/3`, apps `11/11`, Browser network and Qt multi-window sessions passed.
- QEMU: BIOS `3/3`, UEFI `3/3`, Control Center, input/usability, AI/Terminal, Files/Settings multi-window, Browser HTTPS/JavaScript/download, Safe Graphics, Diagnostics export and suspend/resume passed.
- Third-party compatibility: Debian Chromium, Google Chrome and Tor Browser were installed and run temporarily as UID 1000; none is bundled in the ISO.
- Installer and persistence remain disabled; every QEMU run attached no writable disk and reported `unexpected_block_mounts=0`.

The original v0.1 milestone was achieved on 2026-09-04. Validation artifacts: `out/moko-iso-smoke-20260904T144109Z-boot-*`.
