#!/bin/sh
set -eu
lb config \
  --mode debian \
  --distribution trixie \
  --architectures amd64 \
  --binary-images iso-hybrid \
  --chroot-squashfs-compression-level 5 \
  --chroot-squashfs-compression-type zstd \
  --archive-areas "main contrib non-free-firmware" \
  --apt-indices false \
  --debian-installer none \
  --iso-application "MOKO OS v0.1.1 Hardware & Usability Preview" \
  --iso-preparer "MOKO OS live-build" \
  --iso-publisher "MOKO" \
  --iso-volume "MOKO_OS_V0_1_1" \
  --bootappend-live "boot=live components noprompt noeject username=moko hostname=moko-os nottyautologin quiet splash loglevel=0 systemd.show_status=false rd.systemd.show_status=false udev.log_level=0 vt.global_cursor_default=0 logo.nologo plymouth.ignore-serial-consoles console=ttyS0,115200n8" \
  --bootappend-live-failsafe none
