#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ISO=${1:-$ROOT/out/MOKO-OS-v0.1-dev-amd64.hybrid.iso}
RUNS=${MOKO_BOOT_RUNS:-1}
TIMEOUT_SECONDS=${MOKO_BOOT_TIMEOUT:-300}
SCREENSHOT_TIMEOUT_SECONDS=${MOKO_SCREENSHOT_TIMEOUT:-180}
IMAGE=${MOKO_QEMU_IMAGE:-moko-os-debian13-qemu}
STAMP=$(date -u +%Y%m%dT%H%M%SZ)

command -v docker >/dev/null || {
  echo "Docker is required for the containerized ISO smoke test." >&2
  exit 1
}
[[ -f "$ISO" ]] || {
  echo "ISO not found: $ISO" >&2
  exit 1
}
[[ "$RUNS" =~ ^[1-9][0-9]*$ ]] || {
  echo "MOKO_BOOT_RUNS must be a positive integer." >&2
  exit 1
}
[[ "$SCREENSHOT_TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] || {
  echo "MOKO_SCREENSHOT_TIMEOUT must be a positive integer." >&2
  exit 1
}

ISO_DIR=$(cd "$(dirname "$ISO")" && pwd)
ISO_NAME=$(basename "$ISO")
ARTIFACT_PREFIX="moko-iso-smoke-$STAMP"
CONTAINER=""

cleanup() {
  if [[ -n "$CONTAINER" ]]; then
    docker rm -f "$CONTAINER" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT INT TERM

monitor() {
  local command=$1
  printf '%s\n' "$command" | docker exec -i "$CONTAINER" \
    socat - UNIX-CONNECT:/tmp/qemu-monitor.sock >/dev/null
}

wait_for_monitor() {
  local deadline=$((SECONDS + 30))
  until docker exec "$CONTAINER" test -S /tmp/qemu-monitor.sock 2>/dev/null; do
    if (( SECONDS >= deadline )); then
      echo "QEMU monitor did not become ready." >&2
      return 1
    fi
    sleep 1
  done
}

docker build --platform linux/amd64 -t "$IMAGE" "$ROOT/tools/debian-qemu"

docker run --rm --platform linux/amd64 \
  --mount "type=bind,source=$ISO_DIR,target=/artifacts,readonly" \
  --env "ISO_NAME=$ISO_NAME" \
  "$IMAGE" bash -lc '
    set -euo pipefail
    iso="/artifacts/$ISO_NAME"
    xorriso -osirrox on -indev "$iso" \
      -extract /live/filesystem.packages /tmp/filesystem.packages \
      -extract /live/filesystem.squashfs /tmp/filesystem.squashfs \
      -extract /isolinux/isolinux.cfg /tmp/isolinux.cfg \
      -extract /boot/grub/config.cfg /tmp/grub.cfg >/dev/null 2>&1

    if grep -Eiq "^(gnome-shell|gnome-core|gnome-session|task-gnome-desktop|plasma-desktop|kde-standard|task-kde-desktop|calamares|debian-installer)([[:space:]]|$)" /tmp/filesystem.packages; then
      echo "Desktop environment or installer package found in ISO." >&2
      exit 1
    fi
    if grep -Eiq "^(build-essential|cmake|ninja-build|qt6-base-dev|qt6-declarative-dev|qt6-declarative-dev-tools|libxkbcommon-dev)([[:space:]]|$)" /tmp/filesystem.packages; then
      echo "Build-only dependency found in ISO." >&2
      exit 1
    fi

    grep -q "timeout 50" /tmp/isolinux.cfg
    grep -q "set timeout=5" /tmp/grub.cfg
    for path in \
      usr/local/bin/moko-shell \
      usr/local/bin/moko-session \
      usr/local/libexec/moko-live-health-check \
      etc/greetd/config.toml \
      etc/systemd/system/moko-live-health.service
    do
      unsquashfs -ll /tmp/filesystem.squashfs "$path" | grep -Fq "squashfs-root/$path"
    done
    unsquashfs -ll /tmp/filesystem.squashfs etc/systemd/system/getty@tty1.service \
      | grep -Fq "squashfs-root/etc/systemd/system/getty@tty1.service -> /dev/null"
  '

for run in $(seq 1 "$RUNS"); do
  CONTAINER="moko-iso-smoke-$$-$run"
  SERIAL_NAME="$ARTIFACT_PREFIX-boot-$run.serial.log"
  DEBUG_NAME="$ARTIFACT_PREFIX-boot-$run.debug.log"
  SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run.png"
  SERIAL_PATH="$ISO_DIR/$SERIAL_NAME"

  echo "Cold boot $run/$RUNS"
  docker run -d --name "$CONTAINER" --platform linux/amd64 \
    --mount "type=bind,source=$ISO_DIR,target=/artifacts" \
    "$IMAGE" \
    qemu-system-x86_64 \
      -name "MOKO OS cold boot $run" \
      -machine q35,accel=tcg \
      -cpu max \
      -smp 4 \
      -m 3072 \
      -device virtio-vga \
      -display none \
      -vnc :0 \
      -monitor unix:/tmp/qemu-monitor.sock,server=on,wait=off \
      -serial "file:/artifacts/$SERIAL_NAME" \
      -D "/artifacts/$DEBUG_NAME" \
      -d guest_errors \
      -device qemu-xhci \
      -device usb-kbd \
      -device usb-tablet \
      -nic user,model=virtio-net-pci \
      -cdrom "/artifacts/$ISO_NAME" \
      -boot once=d,menu=off \
      -no-reboot >/dev/null

  wait_for_monitor
  deadline=$((SECONDS + TIMEOUT_SECONDS))
  while ! grep -q 'MOKO_HEALTH result=pass' "$SERIAL_PATH" 2>/dev/null; do
    if grep -q 'MOKO_HEALTH result=fail' "$SERIAL_PATH" 2>/dev/null; then
      tail -80 "$SERIAL_PATH" >&2
      echo "Cold boot $run reported a failed health check." >&2
      exit 1
    fi
    if [[ $(docker inspect -f '{{.State.Running}}' "$CONTAINER") != true ]]; then
      tail -80 "$SERIAL_PATH" >&2 || true
      echo "QEMU exited before the health check passed." >&2
      exit 1
    fi
    if (( SECONDS >= deadline )); then
      tail -80 "$SERIAL_PATH" >&2 || true
      echo "Cold boot $run timed out after $TIMEOUT_SECONDS seconds." >&2
      exit 1
    fi
    sleep 2
  done

  screenshot_deadline=$((SECONDS + SCREENSHOT_TIMEOUT_SECONDS))
  screenshot_size=0
  while (( screenshot_size < 100000 )); do
    monitor "screendump /artifacts/$SCREENSHOT_NAME -f png"
    screenshot_size=$(docker exec "$CONTAINER" stat -c %s "/artifacts/$SCREENSHOT_NAME")
    if (( SECONDS >= screenshot_deadline )); then
      echo "Framebuffer remained blank after $SCREENSHOT_TIMEOUT_SECONDS seconds ($screenshot_size bytes)." >&2
      exit 1
    fi
    sleep 5
  done
  grep 'MOKO_HEALTH result=pass' "$SERIAL_PATH" | tail -1

  monitor system_powerdown
  sleep 30
  if [[ $(docker inspect -f '{{.State.Running}}' "$CONTAINER") == true ]]; then
    monitor "eject ide2-cd0"
    monitor "sendkey ret"
  fi

  deadline=$((SECONDS + 30))
  while [[ $(docker inspect -f '{{.State.Running}}' "$CONTAINER") == true ]]; do
    if (( SECONDS >= deadline )); then
      echo "Cold boot $run did not power off cleanly." >&2
      exit 1
    fi
    sleep 1
  done

  exit_code=$(docker inspect -f '{{.State.ExitCode}}' "$CONTAINER")
  [[ "$exit_code" == 0 ]] || {
    echo "QEMU exited with status $exit_code." >&2
    exit 1
  }
  docker rm "$CONTAINER" >/dev/null
  CONTAINER=""
  echo "Cold boot $run passed; artifacts: $ISO_DIR/$ARTIFACT_PREFIX-boot-$run.*"
done
