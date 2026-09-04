#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ISO=${1:-$ROOT/out/MOKO-OS-v0.1-dev-amd64.hybrid.iso}
RUNS=${MOKO_BOOT_RUNS:-1}
TIMEOUT_SECONDS=${MOKO_BOOT_TIMEOUT:-300}
SCREENSHOT_TIMEOUT_SECONDS=${MOKO_SCREENSHOT_TIMEOUT:-180}
LAUNCH_QUERY=${MOKO_LAUNCH_QUERY:-}
LAUNCH_APP_ID=${MOKO_LAUNCH_APP_ID:-}
LAUNCH_SETTLE_SECONDS=${MOKO_LAUNCH_SETTLE_SECONDS:-12}
REQUIRE_APP_READY=${MOKO_REQUIRE_APP_READY:-0}
AI_PROMPT=${MOKO_AI_PROMPT:-}
AI_EXPECT_ACTION=${MOKO_AI_EXPECT_ACTION:-}
AI_EXPECT_APP_ID=${MOKO_AI_EXPECT_APP_ID:-}
IMAGE=${MOKO_QEMU_IMAGE:-moko-os-debian13-qemu}
QEMU_ACCEL=${MOKO_QEMU_ACCEL:-tcg,thread=multi,tb-size=2048}
QEMU_CPU=${MOKO_QEMU_CPU:-max}
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
if [[ -n "$LAUNCH_QUERY" || -n "$LAUNCH_APP_ID" ]]; then
  [[ -n "$LAUNCH_QUERY" && -n "$LAUNCH_APP_ID" ]] || {
    echo "MOKO_LAUNCH_QUERY and MOKO_LAUNCH_APP_ID must be set together." >&2
    exit 1
  }
  [[ "$LAUNCH_QUERY" =~ ^[a-z0-9]+$ ]] || {
    echo "MOKO_LAUNCH_QUERY supports lowercase letters and digits." >&2
    exit 1
  }
  [[ "$LAUNCH_SETTLE_SECONDS" =~ ^[1-9][0-9]*$ ]] || {
    echo "MOKO_LAUNCH_SETTLE_SECONDS must be a positive integer." >&2
    exit 1
  }
  [[ "$REQUIRE_APP_READY" == 0 || "$REQUIRE_APP_READY" == 1 ]] || {
    echo "MOKO_REQUIRE_APP_READY must be 0 or 1." >&2
    exit 1
  }
fi
if [[ -n "$AI_PROMPT" || -n "$AI_EXPECT_ACTION" || -n "$AI_EXPECT_APP_ID" ]]; then
  [[ -n "$AI_PROMPT" && -n "$AI_EXPECT_ACTION" ]] || {
    echo "MOKO_AI_PROMPT and MOKO_AI_EXPECT_ACTION must be set together." >&2
    exit 1
  }
  [[ "$AI_PROMPT" =~ ^[a-z0-9\ ]+$ ]] || {
    echo "MOKO_AI_PROMPT supports lowercase letters, digits and spaces." >&2
    exit 1
  }
  [[ "$AI_EXPECT_ACTION" =~ ^[a-z_]+$ ]] || {
    echo "MOKO_AI_EXPECT_ACTION must be a lowercase action identifier." >&2
    exit 1
  }
fi

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
    if grep -Eiq "^(build-essential|cmake|libvterm-dev|ninja-build|pkg-config|qt6-base-dev|qt6-base-dev-tools|qt6-declarative-dev|qt6-declarative-dev-tools|libxkbcommon-dev)([[:space:]]|$)" /tmp/filesystem.packages; then
      echo "Build-only dependency found in ISO." >&2
      exit 1
    fi

    grep -q "timeout 50" /tmp/isolinux.cfg
    grep -q "set timeout=5" /tmp/grub.cfg
    for path in \
      usr/local/bin/moko-shell \
      usr/local/bin/moko-session \
      usr/local/bin/moko-files \
      usr/local/bin/moko-settings \
      usr/local/bin/moko-terminal \
      usr/local/bin/moko-ai-daemon \
      usr/local/libexec/moko-live-health-check \
      usr/local/libexec/moko-live-launch-monitor \
      usr/local/share/applications/org.moko.Files.desktop \
      usr/local/share/applications/org.moko.Settings.desktop \
      usr/local/share/applications/org.moko.Terminal.desktop \
      usr/local/share/dbus-1/interfaces/org.moko.AI1.xml \
      usr/local/share/dbus-1/services/org.moko.AI1.service \
      etc/greetd/config.toml \
      etc/systemd/system/moko-live-health.service \
      etc/systemd/system/moko-live-launch-monitor.service
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
      -machine q35 \
      -accel "$QEMU_ACCEL" \
      -cpu "$QEMU_CPU" \
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

  if [[ "$run" == 1 && -n "$AI_PROMPT" ]]; then
    ai_daemon_deadline=$((SECONDS + 30))
    while ! grep -E -q "MOKO_AI_DAEMON state=ready uid=[1-9][0-9]* provider=local-stub" "$SERIAL_PATH"; do
      if (( SECONDS >= ai_daemon_deadline )); then
        tail -100 "$SERIAL_PATH" >&2
        echo "MOKO AI daemon did not report ready as an unprivileged user." >&2
        exit 1
      fi
      sleep 1
    done
    grep -E "MOKO_AI_DAEMON state=ready uid=[1-9][0-9]* provider=local-stub" "$SERIAL_PATH" | tail -1

    ai_ui_deadline=$((SECONDS + 30))
    while ! grep -Fq "MOKO_AI_UI state=connected uid=1000" "$SERIAL_PATH"; do
      if (( SECONDS >= ai_ui_deadline )); then
        tail -100 "$SERIAL_PATH" >&2
        echo "MOKO AI UI did not connect to the daemon." >&2
        exit 1
      fi
      sleep 1
    done
    grep -F "MOKO_AI_UI state=connected uid=1000" "$SERIAL_PATH" | tail -1

    monitor "sendkey ctrl-alt-a"
    sleep 1
    for ((index = 0; index < ${#AI_PROMPT}; index++)); do
      key=${AI_PROMPT:index:1}
      if [[ "$key" == " " ]]; then
        key=spc
      fi
      monitor "sendkey $key"
      sleep 0.1
    done
    monitor "sendkey ret"

    ai_response_deadline=$((SECONDS + 45))
    while ! grep -Fq "MOKO_AI_UI state=response action=$AI_EXPECT_ACTION ok=1 uid=1000" "$SERIAL_PATH"; do
      if (( SECONDS >= ai_response_deadline )); then
        tail -120 "$SERIAL_PATH" >&2
        echo "MOKO AI UI did not receive the expected successful response." >&2
        exit 1
      fi
      sleep 1
    done
    grep -F "MOKO_AI_UI state=response action=$AI_EXPECT_ACTION ok=1 uid=1000" "$SERIAL_PATH" | tail -1

    if [[ -n "$AI_EXPECT_APP_ID" ]]; then
      ai_app_deadline=$((SECONDS + 45))
      while ! grep -E -q "MOKO_APP_LAUNCH app_id=$AI_EXPECT_APP_ID state=running pid=[1-9][0-9]* uid=1000" "$SERIAL_PATH"; do
        if (( SECONDS >= ai_app_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "MOKO AI action did not launch $AI_EXPECT_APP_ID." >&2
          exit 1
        fi
        sleep 1
      done
      while ! grep -Fq "MOKO_APP_READY app_id=$AI_EXPECT_APP_ID state=ready" "$SERIAL_PATH"; do
        if (( SECONDS >= ai_app_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "MOKO AI launched app did not report application readiness." >&2
          exit 1
        fi
        sleep 1
      done
      while ! grep -Fq "MOKO_SHELL_SURFACE state=hidden app_id=$AI_EXPECT_APP_ID" "$SERIAL_PATH"; do
        if (( SECONDS >= ai_app_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "MOKO AI launched app did not receive the Cage surface." >&2
          exit 1
        fi
        sleep 1
      done
    fi

    sleep "$LAUNCH_SETTLE_SECONDS"
    AI_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-ai.png"
    monitor "screendump /artifacts/$AI_SCREENSHOT_NAME -f png"
    ai_screenshot_size=$(docker exec "$CONTAINER" stat -c %s "/artifacts/$AI_SCREENSHOT_NAME")
    (( ai_screenshot_size > 10000 )) || {
      echo "MOKO AI framebuffer capture is unexpectedly small." >&2
      exit 1
    }

    if [[ -n "$AI_EXPECT_APP_ID" ]]; then
      monitor "sendkey ctrl-q"
      ai_return_deadline=$((SECONDS + 30))
      while ! grep -Fq "MOKO_SHELL_SURFACE state=shown app_id=$AI_EXPECT_APP_ID" "$SERIAL_PATH"; do
        if (( SECONDS >= ai_return_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "MOKO AI launched app did not return to the Shell." >&2
          exit 1
        fi
        sleep 1
      done
    fi
  fi

  if [[ "$run" == 1 && -n "$LAUNCH_QUERY" ]]; then
    for ((index = 0; index < ${#LAUNCH_QUERY}; index++)); do
      monitor "sendkey ${LAUNCH_QUERY:index:1}"
      sleep 0.1
    done
    monitor "sendkey ret"

    launch_deadline=$((SECONDS + 30))
    while ! grep -E -q "MOKO_APP_LAUNCH app_id=$LAUNCH_APP_ID state=running pid=[1-9][0-9]* uid=[1-9][0-9]*" "$SERIAL_PATH"; do
      if grep -Fq "MOKO_APP_LAUNCH app_id=$LAUNCH_APP_ID state=failed" "$SERIAL_PATH"; then
        tail -80 "$SERIAL_PATH" >&2
        echo "Launcher reported a failed application start." >&2
        exit 1
      fi
      if (( SECONDS >= launch_deadline )); then
        tail -80 "$SERIAL_PATH" >&2
        echo "Launcher did not report $LAUNCH_APP_ID running." >&2
        exit 1
      fi
      sleep 1
    done

    grep -E "MOKO_APP_LAUNCH app_id=$LAUNCH_APP_ID state=running pid=[1-9][0-9]* uid=[1-9][0-9]*" "$SERIAL_PATH" | tail -1

    if [[ "$REQUIRE_APP_READY" == 1 ]]; then
      ready_deadline=$((SECONDS + 30))
      while ! grep -Fq "MOKO_APP_READY app_id=$LAUNCH_APP_ID state=ready" "$SERIAL_PATH"; do
        if (( SECONDS >= ready_deadline )); then
          tail -100 "$SERIAL_PATH" >&2
          echo "$LAUNCH_APP_ID started but did not report ready." >&2
          exit 1
        fi
        sleep 1
      done
      grep -F "MOKO_APP_READY app_id=$LAUNCH_APP_ID state=ready" "$SERIAL_PATH" | tail -1
    fi

    surface_deadline=$((SECONDS + 30))
    while ! grep -Fq "MOKO_SHELL_SURFACE state=hidden app_id=$LAUNCH_APP_ID" "$SERIAL_PATH"; do
      if (( SECONDS >= surface_deadline )); then
        tail -100 "$SERIAL_PATH" >&2
        echo "MOKO Shell did not yield the graphical surface to $LAUNCH_APP_ID." >&2
        exit 1
      fi
      sleep 1
    done
    grep -F "MOKO_SHELL_SURFACE state=hidden app_id=$LAUNCH_APP_ID" "$SERIAL_PATH" | tail -1

    sleep "$LAUNCH_SETTLE_SECONDS"
    LAUNCH_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-launched.png"
    monitor "screendump /artifacts/$LAUNCH_SCREENSHOT_NAME -f png"
    launch_screenshot_size=$(docker exec "$CONTAINER" stat -c %s "/artifacts/$LAUNCH_SCREENSHOT_NAME")
    (( launch_screenshot_size > 10000 )) || {
      echo "Launched application framebuffer capture is unexpectedly small." >&2
      exit 1
    }
    if grep -Fq "MOKO_APP_LAUNCH app_id=$LAUNCH_APP_ID state=failed" "$SERIAL_PATH"; then
      tail -80 "$SERIAL_PATH" >&2
      echo "Application exited during launch validation." >&2
      exit 1
    fi

    if [[ "$REQUIRE_APP_READY" == 1 ]]; then
      monitor "sendkey ctrl-q"
      return_deadline=$((SECONDS + 30))
      while ! grep -Fq "MOKO_SHELL_SURFACE state=shown app_id=$LAUNCH_APP_ID" "$SERIAL_PATH"; do
        if (( SECONDS >= return_deadline )); then
          tail -100 "$SERIAL_PATH" >&2
          echo "$LAUNCH_APP_ID did not close back to MOKO Shell." >&2
          exit 1
        fi
        sleep 1
      done
      grep -F "MOKO_SHELL_SURFACE state=shown app_id=$LAUNCH_APP_ID" "$SERIAL_PATH" | tail -1
    fi
  fi

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
