#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ISO=${1:-$ROOT/out/MOKO-OS-v0.1-dev-amd64.hybrid.iso}
RUNS=${MOKO_BOOT_RUNS:-1}
TIMEOUT_SECONDS=${MOKO_BOOT_TIMEOUT:-300}
SCREENSHOT_TIMEOUT_SECONDS=${MOKO_SCREENSHOT_TIMEOUT:-180}
BOOT_MODE=${MOKO_BOOT_MODE:-desktop}
BOOT_FIRMWARE=${MOKO_BOOT_FIRMWARE:-bios}
LAUNCH_QUERY=${MOKO_LAUNCH_QUERY:-}
LAUNCH_APP_ID=${MOKO_LAUNCH_APP_ID:-}
LAUNCH_SETTLE_SECONDS=${MOKO_LAUNCH_SETTLE_SECONDS:-12}
REQUIRE_APP_READY=${MOKO_REQUIRE_APP_READY:-0}
EXPECT_HARDWARE_REPORT=${MOKO_EXPECT_HARDWARE_REPORT:-0}
SETTINGS_OPEN_HARDWARE=${MOKO_SETTINGS_OPEN_HARDWARE:-0}
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
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] || {
  echo "MOKO_BOOT_TIMEOUT must be a positive integer." >&2
  exit 1
}
[[ "$SCREENSHOT_TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] || {
  echo "MOKO_SCREENSHOT_TIMEOUT must be a positive integer." >&2
  exit 1
}
case "$BOOT_MODE" in
  desktop|hardware-diagnostics|safe-graphics) ;;
  *)
    echo "MOKO_BOOT_MODE must be desktop, hardware-diagnostics or safe-graphics." >&2
    exit 1
    ;;
esac
case "$BOOT_FIRMWARE" in
  bios|uefi) ;;
  *)
    echo "MOKO_BOOT_FIRMWARE must be bios or uefi." >&2
    exit 1
    ;;
esac
for boolean_name in REQUIRE_APP_READY EXPECT_HARDWARE_REPORT SETTINGS_OPEN_HARDWARE; do
  boolean_value=${!boolean_name}
  [[ "$boolean_value" == 0 || "$boolean_value" == 1 ]] || {
    echo "MOKO_${boolean_name} must be 0 or 1." >&2
    exit 1
  }
done
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
if [[ "$BOOT_MODE" == hardware-diagnostics ]] \
    && [[ -n "$LAUNCH_QUERY" || -n "$AI_PROMPT" ]]; then
  echo "Launcher and AI interaction tests require a Shell boot profile." >&2
  exit 1
fi

ISO_DIR=$(cd "$(dirname "$ISO")" && pwd)
ISO_NAME=$(basename "$ISO")
ARTIFACT_PREFIX="moko-iso-smoke-$STAMP-$BOOT_FIRMWARE-$BOOT_MODE"
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

qmp() {
  local command=$1
  local response
  response=$(printf '%s\n%s\n' '{"execute":"qmp_capabilities"}' "$command" | docker exec -i "$CONTAINER" \
    socat - UNIX-CONNECT:/tmp/qemu-qmp.sock)
  if grep -Fq '"error"' <<<"$response"; then
    printf '%s\n' "$response" >&2
    return 1
  fi
}

pointer_click() {
  local x=$1
  local y=$2
  local absolute_x=$((x * 32767 / 1279))
  local absolute_y=$((y * 32767 / 799))
  qmp "{\"execute\":\"input-send-event\",\"arguments\":{\"events\":[{\"type\":\"abs\",\"data\":{\"axis\":\"x\",\"value\":$absolute_x}},{\"type\":\"abs\",\"data\":{\"axis\":\"y\",\"value\":$absolute_y}}]}}"
  sleep 1
  qmp '{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":true,"button":"left"}}]}}'
  sleep 1
  qmp '{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":false,"button":"left"}}]}}'
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

select_boot_profile() {
  local hotkey=""
  local menu_delay=${MOKO_BOOT_MENU_DELAY:-}
  case "$BOOT_MODE" in
    hardware-diagnostics) hotkey=h ;;
    safe-graphics) hotkey=s ;;
    desktop) return ;;
  esac

  if [[ -z "$menu_delay" ]]; then
    if [[ "$BOOT_FIRMWARE" == uefi ]]; then
      menu_delay=6
    else
      menu_delay=3
    fi
  fi
  [[ "$menu_delay" =~ ^[1-9][0-9]*$ ]] || {
    echo "MOKO_BOOT_MENU_DELAY must be a positive integer." >&2
    return 1
  }

  # Let SeaBIOS/OVMF hand control to the ISO before sending a menu hotkey.
  sleep "$menu_delay"
  monitor "sendkey $hotkey"
  sleep 0.5
  monitor "sendkey ret"
}

docker build --platform linux/amd64 -t "$IMAGE" "$ROOT/tools/debian-qemu"

docker run --rm --platform linux/amd64 \
  --mount "type=bind,source=$ISO_DIR,target=/artifacts,readonly" \
  --mount "type=bind,source=$ROOT,target=/source,readonly" \
  --env "ISO_NAME=$ISO_NAME" \
  "$IMAGE" bash -lc '
    set -euo pipefail
    iso="/artifacts/$ISO_NAME"
    xorriso -osirrox on -indev "$iso" \
      -extract /live/filesystem.packages /tmp/filesystem.packages \
      -extract /live/filesystem.squashfs /tmp/filesystem.squashfs \
      -extract /isolinux/isolinux.cfg /tmp/isolinux.cfg \
      -extract /isolinux/menu.cfg /tmp/isolinux-menu.cfg \
      -extract /isolinux/live.cfg /tmp/syslinux-live.cfg \
      -extract /boot/grub/config.cfg /tmp/grub-config.cfg \
      -extract /boot/grub/grub.cfg /tmp/grub-menu.cfg \
      -extract /EFI/boot/bootx64.efi /tmp/bootx64.efi \
      -extract /MOKO/BUILD-INFO.txt /tmp/moko-build-info.txt \
      -extract /MOKO/KNOWN-ISSUES.txt /tmp/moko-known-issues.txt \
      -extract /MOKO/LIVE_USB_CHECKLIST.md /tmp/moko-live-usb-checklist.md \
      -extract /MOKO/MOKO-OS-v0.1-dev-amd64.packages.txt /tmp/moko-packages.txt \
      >/dev/null 2>&1
    cmp /tmp/filesystem.packages /tmp/moko-packages.txt
    cut -f1 /tmp/filesystem.packages | sed "s/:.*$//" > /tmp/package-names

    if grep -Eiq "^(gnome-shell|gnome-core|gnome-session|task-gnome-desktop|plasma-desktop|kde-standard|task-kde-desktop|xfce4|task-xfce-desktop|calamares|debian-installer|gparted|parted|udisks2)$" /tmp/package-names; then
      echo "Desktop environment, automounter or installer package found in ISO." >&2
      exit 1
    fi
    if grep -Eiq "^(build-essential|cmake|libvterm-dev|ninja-build|pkg-config|qt6-base-dev|qt6-base-dev-tools|qt6-declarative-dev|qt6-declarative-dev-tools|libxkbcommon-dev)$" /tmp/package-names; then
      echo "Build-only dependency found in ISO." >&2
      exit 1
    fi

    for package in \
      live-config network-manager rfkill iw pipewire wireplumber alsa-utils \
      bluez cage greetd xwayland mesa-utils mesa-vulkan-drivers \
      libgl1-mesa-dri libinput-tools v4l-utils qt6-wayland \
      firmware-linux firmware-misc-nonfree firmware-iwlwifi \
      firmware-amd-graphics firmware-brcm80211
    do
      grep -Fxq "$package" /tmp/package-names || {
          echo "Required Live USB package missing: $package" >&2
          exit 1
        }
    done

    grep -q "timeout 100" /tmp/isolinux.cfg
    grep -q "set timeout=10" /tmp/grub-config.cfg
    grep -Fq "MOKO OS v0.1 Developer Preview" /tmp/isolinux-menu.cfg
    for label in "Try MOKO OS" "Hardware Diagnostics" "Safe Graphics Mode"; do
      grep -Fq "$label" /tmp/syslinux-live.cfg
      grep -Fq "$label" /tmp/grub-menu.cfg
    done
    for mode in desktop hardware-diagnostics safe-graphics; do
      grep -Fq "moko.mode=$mode" /tmp/syslinux-live.cfg
      grep -Fq "moko.mode=$mode" /tmp/grub-menu.cfg
    done
    test -s /tmp/bootx64.efi
    grep -Fxq "Artifact: MOKO-OS-v0.1-dev-amd64.hybrid.iso" /tmp/moko-build-info.txt
    grep -Fq "Installer: disabled" /tmp/moko-build-info.txt
    grep -Fq "non-installing Live USB preview" /tmp/moko-known-issues.txt
    grep -Fq "internal SSD/HDD partitions have no mountpoint" /tmp/moko-live-usb-checklist.md
    for path in \
      usr/local/bin/moko-shell \
      usr/local/bin/moko-session \
      usr/local/bin/moko-cage-session \
      usr/local/bin/moko-files \
      usr/local/bin/moko-settings \
      usr/local/bin/moko-terminal \
      usr/local/bin/moko-hardware-diagnostics \
      usr/local/bin/moko-ai-daemon \
      usr/local/libexec/moko-live-health-check \
      usr/local/libexec/moko-live-disk-safety-check \
      usr/local/libexec/moko-live-launch-monitor \
      usr/local/share/applications/org.moko.Files.desktop \
      usr/local/share/applications/org.moko.Settings.desktop \
      usr/local/share/applications/org.moko.Terminal.desktop \
      usr/local/share/applications/org.moko.HardwareDiagnostics.desktop \
      usr/local/share/dbus-1/interfaces/org.moko.AI1.xml \
      usr/local/share/dbus-1/services/org.moko.AI1.service \
      etc/greetd/config.toml \
      etc/systemd/system/greetd.service.d/10-moko-live-safety.conf \
      etc/systemd/system/moko-live-disk-safety.service \
      etc/systemd/system/moko-live-health.service \
      etc/systemd/system/moko-live-launch-monitor.service
    do
      unsquashfs -ll /tmp/filesystem.squashfs "$path" | grep -Fq "squashfs-root/$path"
    done
    unsquashfs -cat /tmp/filesystem.squashfs \
      usr/local/libexec/moko-live-disk-safety-check \
      > /tmp/moko-live-disk-safety-check
    chmod 0755 /tmp/moko-live-disk-safety-check
    MOKO_DISK_SAFETY_CHECK=/tmp/moko-live-disk-safety-check \
      bash /source/tests/test-live-disk-safety.sh
    for unit in moko-live-disk-safety.service moko-live-health.service; do
      unsquashfs -cat /tmp/filesystem.squashfs \
        "etc/systemd/system/$unit" > "/tmp/$unit"
      grep -Fxq "StandardOutput=journal" "/tmp/$unit"
      grep -Fxq "StandardError=journal" "/tmp/$unit"
      if grep -Fq "journal+console" "/tmp/$unit"; then
        echo "Boot gate still depends on console logging: $unit" >&2
        exit 1
      fi
    done
    unsquashfs -ll /tmp/filesystem.squashfs etc/systemd/system/getty@tty1.service \
      | grep -Fq "squashfs-root/etc/systemd/system/getty@tty1.service -> /dev/null"
    unsquashfs -ll /tmp/filesystem.squashfs etc/systemd/system/udisks2.service \
      | grep -Fq "squashfs-root/etc/systemd/system/udisks2.service -> /dev/null"
    if unsquashfs -ll /tmp/filesystem.squashfs usr/local/bin/moko-installer \
        | grep -Fq "squashfs-root/usr/local/bin/moko-installer"; then
      echo "Installer executable found in ISO." >&2
      exit 1
    fi
    for path in \
      root/.wget-hsts \
      var/cache/apt/pkgcache.bin \
      var/cache/apt/srcpkgcache.bin
    do
      if unsquashfs -ll /tmp/filesystem.squashfs "$path" \
          | grep -Fq "squashfs-root/$path"; then
        echo "Non-reproducible build cache found in ISO: $path" >&2
        exit 1
      fi
    done
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
    --env "MOKO_QEMU_FIRMWARE=$BOOT_FIRMWARE" \
    "$IMAGE" \
    moko-qemu qemu-system-x86_64 \
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
      -qmp unix:/tmp/qemu-qmp.sock,server=on,wait=off \
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
  select_boot_profile
  deadline=$((SECONDS + TIMEOUT_SECONDS))
  expected_graphics=hardware
  expected_safe_graphics=0
  if [[ "$BOOT_MODE" == safe-graphics ]]; then
    expected_graphics=software
    expected_safe_graphics=1
  fi
  health_pattern="MOKO_HEALTH result=pass mode=$BOOT_MODE .*greetd_restarts=0 graphics=$expected_graphics firmware=$BOOT_FIRMWARE"
  while ! grep -E -q "$health_pattern" "$SERIAL_PATH" 2>/dev/null; do
    if grep -q 'MOKO_DISK_SAFETY result=fail' "$SERIAL_PATH" 2>/dev/null; then
      tail -80 "$SERIAL_PATH" >&2
      echo "Cold boot $run violated the live disk-safety policy." >&2
      exit 1
    fi
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

  grep -Fq "MOKO_DISK_SAFETY result=pass unexpected_block_mounts=0 automounter=absent" "$SERIAL_PATH" || {
    tail -100 "$SERIAL_PATH" >&2
    echo "Live session did not prove the read-only startup mount policy." >&2
    exit 1
  }
  profile_deadline=$((SECONDS + 30))
  while ! grep -Fq "MOKO_BOOT_PROFILE mode=$BOOT_MODE safe_graphics=$expected_safe_graphics uid=1000" "$SERIAL_PATH"; do
    if (( SECONDS >= profile_deadline )); then
      tail -100 "$SERIAL_PATH" >&2
      echo "The selected MOKO boot profile did not reach the user session." >&2
      exit 1
    fi
    sleep 1
  done

  screenshot_deadline=$((SECONDS + SCREENSHOT_TIMEOUT_SECONDS))
  screenshot_size=0
  screenshot_minimum=100000
  if [[ "$BOOT_MODE" == hardware-diagnostics ]]; then
    screenshot_minimum=50000
  fi
  while (( screenshot_size < screenshot_minimum )); do
    monitor "screendump /artifacts/$SCREENSHOT_NAME -f png"
    screenshot_size=$(docker exec "$CONTAINER" stat -c %s "/artifacts/$SCREENSHOT_NAME")
    if (( SECONDS >= screenshot_deadline )); then
      echo "Framebuffer remained blank after $SCREENSHOT_TIMEOUT_SECONDS seconds ($screenshot_size bytes)." >&2
      exit 1
    fi
    sleep 5
  done
  grep -E "$health_pattern" "$SERIAL_PATH" | tail -1

  if [[ "$BOOT_MODE" == hardware-diagnostics ]]; then
    hardware_deadline=$((SECONDS + 45))
    while ! grep -E -q "MOKO_HW_REPORT state=ready overall=(SUPPORTED|PARTIAL|UNKNOWN) manufacturer=QEMU .* architecture=x86_64 .* writable_disk_detected=0" "$SERIAL_PATH"; do
      if (( SECONDS >= hardware_deadline )); then
        tail -120 "$SERIAL_PATH" >&2
        echo "Direct-boot Hardware Diagnostics did not produce the QEMU report." >&2
        exit 1
      fi
      sleep 1
    done
    grep -Fq "MOKO_APP_READY app_id=org.moko.HardwareDiagnostics state=ready" "$SERIAL_PATH"
    pointer_click 1050 69
    pointer_click 1162 69
    for format in json txt; do
      while ! grep -E -q "MOKO_HW_EXPORT format=$format state=written bytes=[1-9][0-9]{2,} uid=1000" "$SERIAL_PATH"; do
        if (( SECONDS >= hardware_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "Direct-boot Hardware Diagnostics did not export $format." >&2
          exit 1
        fi
        sleep 1
      done
    done
    grep -E "MOKO_HW_REPORT state=ready|MOKO_HW_EXPORT" "$SERIAL_PATH" | tail -3
  fi

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
    sleep 2
    monitor "sendkey ctrl-a"
    sleep 0.5
    # Absorb a possible first-key focus transition; the provider trims whitespace.
    monitor "sendkey spc"
    sleep 0.5
    for ((index = 0; index < ${#AI_PROMPT}; index++)); do
      key=${AI_PROMPT:index:1}
      if [[ "$key" == " " ]]; then
        key=spc
      fi
      monitor "sendkey $key"
      sleep 0.2
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
    pointer_click 220 108
    sleep 1
    monitor "sendkey ctrl-a"
    sleep 0.5
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

    if [[ "$EXPECT_HARDWARE_REPORT" == 1 ]]; then
      [[ "$LAUNCH_APP_ID" == "org.moko.HardwareDiagnostics" ]] || {
        echo "MOKO_EXPECT_HARDWARE_REPORT requires org.moko.HardwareDiagnostics." >&2
        exit 1
      }
      hardware_deadline=$((SECONDS + 45))
      while ! grep -E -q "MOKO_HW_REPORT state=ready overall=(SUPPORTED|PARTIAL|UNKNOWN) manufacturer=QEMU .* architecture=x86_64 .* writable_disk_detected=0" "$SERIAL_PATH"; do
        if (( SECONDS >= hardware_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "Hardware Diagnostics did not report the expected read-only QEMU inventory." >&2
          exit 1
        fi
        sleep 1
      done
      sleep 3
      HARDWARE_READY_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-hardware-ready.png"
      monitor "screendump /artifacts/$HARDWARE_READY_SCREENSHOT_NAME -f png"
      pointer_click 1050 69
      monitor "screendump /artifacts/$ARTIFACT_PREFIX-boot-$run-hardware-click.png -f png"
      pointer_click 1162 69
      for format in json txt; do
        while ! grep -E -q "MOKO_HW_EXPORT format=$format state=written bytes=[1-9][0-9]{2,} uid=1000" "$SERIAL_PATH"; do
          if (( SECONDS >= hardware_deadline )); then
            tail -120 "$SERIAL_PATH" >&2
            echo "Hardware Diagnostics did not export the $format report." >&2
            exit 1
          fi
          sleep 1
        done
      done
      grep -E "MOKO_HW_REPORT state=ready|MOKO_HW_EXPORT" "$SERIAL_PATH" | tail -3
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
    if [[ "$SETTINGS_OPEN_HARDWARE" == 1 ]]; then
      [[ "$LAUNCH_APP_ID" == "org.moko.Settings" ]] || {
        echo "MOKO_SETTINGS_OPEN_HARDWARE requires org.moko.Settings." >&2
        exit 1
      }
      pointer_click 100 481
      sleep 2
      monitor "screendump /artifacts/$ARTIFACT_PREFIX-boot-$run-settings-hardware.png -f png"
      pointer_click 770 695
      settings_deadline=$((SECONDS + 45))
      while ! grep -Fq "MOKO_SETTINGS_ACTION action=open_hardware_diagnostics state=accepted uid=1000" "$SERIAL_PATH"; do
        if (( SECONDS >= settings_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "MOKO Settings did not launch Hardware Diagnostics." >&2
          exit 1
        fi
        sleep 1
      done
      while ! grep -Fq "MOKO_APP_READY app_id=org.moko.HardwareDiagnostics state=ready" "$SERIAL_PATH"; do
        if (( SECONDS >= settings_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "Hardware Diagnostics did not become ready after the Settings action." >&2
          exit 1
        fi
        sleep 1
      done
      sleep 3
      monitor "screendump /artifacts/$ARTIFACT_PREFIX-boot-$run-settings-launched-hardware.png -f png"
    fi
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
      if [[ "$SETTINGS_OPEN_HARDWARE" == 1 ]]; then
        sleep 3
        monitor "sendkey ctrl-q"
      fi
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
