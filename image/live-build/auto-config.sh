#!/bin/sh
set -eu
lb config \
  --mode debian \
  --distribution trixie \
  --architectures amd64 \
  --binary-images iso-hybrid \
  --archive-areas "main contrib non-free-firmware" \
  --debian-installer none \
  --iso-application "MOKO OS v0.1 Developer Preview" \
  --iso-preparer "MOKO OS live-build" \
  --iso-publisher "MOKO" \
  --iso-volume "MOKO_OS_V0_1" \
  --bootappend-live "boot=live components username=moko hostname=moko-os nottyautologin quiet console=ttyS0,115200n8 console=tty0"
