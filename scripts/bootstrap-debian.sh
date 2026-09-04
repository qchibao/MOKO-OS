#!/usr/bin/env bash
set -euo pipefail
if ! grep -qiE 'debian.*(13|trixie)' /etc/os-release 2>/dev/null; then
  echo "Warning: Debian 13/trixie is the reference host. Continuing anyway."
fi
if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run with sudo: sudo $0" >&2
  exit 1
fi
apt-get update
apt-get install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  live-build debootstrap squashfs-tools xorriso rsync qemu-system-x86 ovmf git
