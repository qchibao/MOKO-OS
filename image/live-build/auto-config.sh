#!/bin/sh
set -eu
lb config \
  --mode debian \
  --distribution trixie \
  --architectures amd64 \
  --binary-images iso-hybrid \
  --archive-areas "main contrib non-free-firmware" \
  --debian-installer none \
  --bootappend-live "boot=live components username=moko hostname=moko-os quiet"
