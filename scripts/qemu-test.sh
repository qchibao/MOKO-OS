#!/usr/bin/env bash
set -euo pipefail
ISO=${1:-}
if [[ -z "$ISO" || ! -f "$ISO" ]]; then
  echo "Usage: $0 path/to/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso" >&2
  exit 1
fi
command -v qemu-system-x86_64 >/dev/null || { echo "qemu-system-x86_64 not found" >&2; exit 1; }

ACCEL="tcg"
CPU="max"
if [[ "$(uname -s)" == "Darwin" ]]; then
  ACCEL="hvf"
  CPU="host"
elif [[ -e /dev/kvm ]]; then
  ACCEL="kvm"
  CPU="host"
fi

exec qemu-system-x86_64 \
  -name "MOKO OS v0.1.1" \
  -machine q35,accel="$ACCEL" \
  -cpu "$CPU" \
  -smp 4 \
  -m 4096 \
  -device virtio-vga \
  -display default \
  -device qemu-xhci \
  -device usb-kbd \
  -device usb-tablet \
  -nic user,model=virtio-net-pci \
  -cdrom "$ISO" \
  -boot d
