#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ISO=${1:-$ROOT/out/MOKO-OS-v0.1.1-dev-amd64.hybrid.iso}
RUNS=${MOKO_BOOT_RUNS:-1}
TIMEOUT_SECONDS=${MOKO_BOOT_TIMEOUT:-300}
SCREENSHOT_TIMEOUT_SECONDS=${MOKO_SCREENSHOT_TIMEOUT:-180}
SHUTDOWN_TIMEOUT_SECONDS=${MOKO_SHUTDOWN_TIMEOUT:-60}
BOOT_MODE=${MOKO_BOOT_MODE:-desktop}
BOOT_FIRMWARE=${MOKO_BOOT_FIRMWARE:-bios}
LAUNCH_QUERY=${MOKO_LAUNCH_QUERY:-}
LAUNCH_APP_ID=${MOKO_LAUNCH_APP_ID:-}
LAUNCH_SETTLE_SECONDS=${MOKO_LAUNCH_SETTLE_SECONDS:-12}
APP_READY_TIMEOUT_SECONDS=${MOKO_APP_READY_TIMEOUT:-240}
REQUIRE_APP_READY=${MOKO_REQUIRE_APP_READY:-0}
EXPECT_HARDWARE_REPORT=${MOKO_EXPECT_HARDWARE_REPORT:-0}
SETTINGS_OPEN_HARDWARE=${MOKO_SETTINGS_OPEN_HARDWARE:-0}
WINDOW_WORKFLOW=${MOKO_WINDOW_WORKFLOW:-0}
CONTROL_CENTER_TEST=${MOKO_CONTROL_CENTER_TEST:-0}
INPUT_TEST=${MOKO_INPUT_TEST:-0}
USABILITY_TEST=${MOKO_USABILITY_TEST:-0}
BROWSER_TEST=${MOKO_BROWSER_TEST:-0}
RESUME_TEST=${MOKO_RESUME_TEST:-0}
AI_PROMPT=${MOKO_AI_PROMPT:-}
AI_EXPECT_ACTION=${MOKO_AI_EXPECT_ACTION:-}
AI_EXPECT_APP_ID=${MOKO_AI_EXPECT_APP_ID:-}
IMAGE=${MOKO_QEMU_IMAGE:-moko-os-debian13-qemu}
QEMU_ACCEL=${MOKO_QEMU_ACCEL:-tcg,thread=multi,tb-size=2048}
QEMU_CPU=${MOKO_QEMU_CPU:-max}
QEMU_VIDEO_DEVICE=${MOKO_QEMU_VIDEO_DEVICE:-virtio-vga}
QEMU_EXIT_ACTION=${MOKO_QEMU_EXIT_ACTION:-powerdown}
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
[[ "$SHUTDOWN_TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] || {
  echo "MOKO_SHUTDOWN_TIMEOUT must be a positive integer." >&2
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
case "$QEMU_VIDEO_DEVICE" in
  virtio-vga) QEMU_VIDEO_ARGUMENTS=(-device virtio-vga) ;;
  std) QEMU_VIDEO_ARGUMENTS=(-vga std) ;;
  *)
    echo "MOKO_QEMU_VIDEO_DEVICE must be virtio-vga or std." >&2
    exit 1
    ;;
esac
case "$QEMU_EXIT_ACTION" in
  powerdown|quit) ;;
  *)
    echo "MOKO_QEMU_EXIT_ACTION must be powerdown or quit." >&2
    exit 1
    ;;
esac
for boolean_name in REQUIRE_APP_READY EXPECT_HARDWARE_REPORT SETTINGS_OPEN_HARDWARE WINDOW_WORKFLOW CONTROL_CENTER_TEST INPUT_TEST USABILITY_TEST BROWSER_TEST RESUME_TEST; do
  boolean_value=${!boolean_name}
  [[ "$boolean_value" == 0 || "$boolean_value" == 1 ]] || {
    echo "MOKO_${boolean_name} must be 0 or 1." >&2
    exit 1
  }
done
if [[ "$USABILITY_TEST" == 1 && "$BOOT_MODE" != desktop ]]; then
  echo "MOKO_USABILITY_TEST requires the normal desktop profile." >&2
  exit 1
fi
if [[ "$RESUME_TEST" == 1 ]]; then
  [[ "$BOOT_MODE" == desktop ]] || {
    echo "MOKO_RESUME_TEST requires the normal desktop profile." >&2
    exit 1
  }
  [[ "$LAUNCH_QUERY" == browser && "$LAUNCH_APP_ID" == org.moko.Browser ]] || {
    echo "MOKO_RESUME_TEST requires the Browser launcher query and app id." >&2
    exit 1
  }
  [[ "$REQUIRE_APP_READY" == 1 ]] || {
    echo "MOKO_RESUME_TEST requires MOKO_REQUIRE_APP_READY=1." >&2
    exit 1
  }
fi
if [[ "$QEMU_EXIT_ACTION" == quit && "$RESUME_TEST" != 1 ]]; then
  echo "MOKO_QEMU_EXIT_ACTION=quit is restricted to the QEMU resume gate." >&2
  exit 1
fi
if [[ "$BROWSER_TEST" == 1 ]]; then
  [[ "$BOOT_MODE" == desktop ]] || {
    echo "MOKO_BROWSER_TEST requires the normal desktop profile." >&2
    exit 1
  }
  [[ "$LAUNCH_QUERY" == browser && "$LAUNCH_APP_ID" == org.moko.Browser ]] || {
    echo "MOKO_BROWSER_TEST requires the Browser launcher query and app id." >&2
    exit 1
  }
  [[ "$REQUIRE_APP_READY" == 1 ]] || {
    echo "MOKO_BROWSER_TEST requires MOKO_REQUIRE_APP_READY=1." >&2
    exit 1
  }
fi
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
  [[ "$APP_READY_TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] || {
    echo "MOKO_APP_READY_TIMEOUT must be a positive integer." >&2
    exit 1
  }
fi
if [[ "$WINDOW_WORKFLOW" == 1 ]]; then
  [[ "$BOOT_MODE" == desktop ]] || {
    echo "MOKO_WINDOW_WORKFLOW requires the normal desktop profile." >&2
    exit 1
  }
  [[ "$LAUNCH_APP_ID" == org.moko.Files ]] || {
    echo "MOKO_WINDOW_WORKFLOW requires MOKO_LAUNCH_APP_ID=org.moko.Files." >&2
    exit 1
  }
  [[ "$REQUIRE_APP_READY" == 1 ]] || {
    echo "MOKO_WINDOW_WORKFLOW requires MOKO_REQUIRE_APP_READY=1." >&2
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
if [[ "$QEMU_VIDEO_DEVICE" != virtio-vga ]]; then
  ARTIFACT_PREFIX="$ARTIFACT_PREFIX-$QEMU_VIDEO_DEVICE"
fi
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

qmp_request() {
  local command=$1
  local response
  response=$(printf '%s\n%s\n' '{"execute":"qmp_capabilities"}' "$command" | docker exec -i "$CONTAINER" \
    socat - UNIX-CONNECT:/tmp/qemu-qmp.sock)
  if grep -Fq '"error"' <<<"$response"; then
    printf '%s\n' "$response" >&2
    return 1
  fi
  printf '%s\n' "$response"
}

qmp() {
  qmp_request "$1" >/dev/null
}

wait_for_qemu_status() {
  local expected=$1
  local timeout=$2
  local deadline=$((SECONDS + timeout))
  local response
  while :; do
    response=$(qmp_request '{"execute":"query-status"}')
    if grep -Eq '"status"[[:space:]]*:[[:space:]]*"'"$expected"'"' <<<"$response"; then
      return 0
    fi
    if (( SECONDS >= deadline )); then
      printf '%s\n' "$response" >&2
      echo "QEMU did not reach $expected state within $timeout seconds." >&2
      return 1
    fi
    sleep 1
  done
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

dock_app_click() {
  local app_id=$1
  local app_slot
  case "$app_id" in
    org.moko.Files) app_slot=1 ;;
    org.moko.Browser) app_slot=2 ;;
    org.moko.Settings) app_slot=3 ;;
    org.moko.Terminal) app_slot=4 ;;
    org.moko.HardwareDiagnostics) app_slot=5 ;;
    *)
      echo "No QEMU Dock slot is defined for $app_id." >&2
      return 1
      ;;
  esac

  # Mirrors Dock.qml's compact 1280x800 geometry and pinned application order.
  local app_count=5
  local button_extent=44
  local button_spacing=4
  local item_count=$((app_count + 2))
  local dock_width=$((28 + item_count * button_extent + (app_count + 1) * button_spacing))
  local row_width=$((item_count * button_extent + (item_count - 1) * button_spacing))
  local row_left=$(((1280 - dock_width) / 2 + (dock_width - row_width) / 2))
  local x=$((row_left + app_slot * (button_extent + button_spacing) + button_extent / 2))
  pointer_click "$x" 747
}

pointer_move() {
  local x=$1
  local y=$2
  local absolute_x=$((x * 32767 / 1279))
  local absolute_y=$((y * 32767 / 799))
  qmp "{\"execute\":\"input-send-event\",\"arguments\":{\"events\":[{\"type\":\"abs\",\"data\":{\"axis\":\"x\",\"value\":$absolute_x}},{\"type\":\"abs\",\"data\":{\"axis\":\"y\",\"value\":$absolute_y}}]}}"
}

pointer_drag() {
  local start_x=$1
  local start_y=$2
  local end_x=$3
  local end_y=$4
  local midpoint_x=$(((start_x + end_x) / 2))
  local midpoint_y=$(((start_y + end_y) / 2))
  # Give the Wayland client a distinct enter/motion pair before the press.
  pointer_move "$((start_x - 4))" "$((start_y - 4))"
  sleep 0.5
  pointer_move "$start_x" "$start_y"
  sleep 1.5
  qmp '{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":true,"button":"left"}}]}}'
  sleep 1
  pointer_move "$midpoint_x" "$midpoint_y"
  sleep 0.5
  pointer_move "$end_x" "$end_y"
  sleep 1
  qmp '{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":false,"button":"left"}}]}}'
}

send_text() {
  local value=$1
  local character key
  local -i index
  for ((index = 0; index < ${#value}; index++)); do
    character=${value:index:1}
    case "$character" in
      [a-z0-9]) key=$character ;;
      " ") key=spc ;;
      ".") key=dot ;;
      "/") key=slash ;;
      "-") key=minus ;;
      "_") key=shift-minus ;;
      *)
        echo "Unsupported QEMU text-entry character: $character" >&2
        return 1
        ;;
    esac
    monitor "sendkey $key"
    sleep 0.08
  done
}

serial_line_count() {
  wc -l < "$SERIAL_PATH" 2>/dev/null || printf '0\n'
}

wait_for_serial_since() {
  local start_line=$1
  local pattern=$2
  local timeout=$3
  local failure_message=$4
  local deadline=$((SECONDS + timeout))
  while ! awk -v start="$start_line" -v pattern="$pattern" \
      'NR > start && $0 ~ pattern { found = 1 } END { exit !found }' "$SERIAL_PATH"; do
    if (( SECONDS >= deadline )); then
      tail -140 "$SERIAL_PATH" >&2
      echo "$failure_message" >&2
      return 1
    fi
    sleep 1
  done
  awk -v start="$start_line" -v pattern="$pattern" \
    'NR > start && $0 ~ pattern { line = $0 } END { print line }' "$SERIAL_PATH"
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
      -extract /MOKO/MOKO-OS-v0.1.1-dev-amd64.packages.txt /tmp/moko-packages.txt \
      >/dev/null 2>&1
    cmp /tmp/filesystem.packages /tmp/moko-packages.txt
    cut -f1 /tmp/filesystem.packages | sed "s/:.*$//" > /tmp/package-names

    if grep -Eiq "^(gnome-shell|gnome-core|gnome-session|task-gnome-desktop|plasma-desktop|kde-standard|task-kde-desktop|xfce4|task-xfce-desktop|calamares|debian-installer|gparted|parted|udisks2)$" /tmp/package-names; then
      echo "Desktop environment, automounter or installer package found in ISO." >&2
      exit 1
    fi
    if grep -Eiq "^(build-essential|cmake|libvterm-dev|libwayland-dev|libwlroots-0.18-dev|ninja-build|pkg-config|qt6-base-dev|qt6-base-dev-tools|qt6-declarative-dev|qt6-declarative-dev-tools|qt6-webengine-dev|qt6-webengine-dev-tools|libxkbcommon-dev|wayland-protocols)$" /tmp/package-names; then
      echo "Build-only dependency found in ISO." >&2
      exit 1
    fi

    for package in \
      live-config network-manager rfkill iw pipewire wireplumber \
      libspa-0.2-bluetooth libspa-0.2-libcamera alsa-utils \
      brightnessctl grim bluez power-profiles-daemon cage libwlroots-0.18 greetd xwayland mesa-utils mesa-vulkan-drivers \
      libgl1-mesa-dri libinput-tools v4l-utils qt6-wayland \
      qml6-module-qtwebengine libqt6webenginecore6 libqt6webenginequick6 \
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
    grep -Fq "MOKO OS v0.1.1 Hardware & Usability Preview" /tmp/isolinux-menu.cfg
    for label in "Try MOKO OS" "Hardware Diagnostics" "Safe Graphics Mode"; do
      grep -Fq "$label" /tmp/syslinux-live.cfg
      grep -Fq "$label" /tmp/grub-menu.cfg
    done
    for mode in desktop hardware-diagnostics safe-graphics; do
      grep -Fq "moko.mode=$mode" /tmp/syslinux-live.cfg
      grep -Fq "moko.mode=$mode" /tmp/grub-menu.cfg
    done
    test -s /tmp/bootx64.efi
    grep -Fxq "Artifact: MOKO-OS-v0.1.1-dev-amd64.hybrid.iso" /tmp/moko-build-info.txt
    grep -Fq "Installer: disabled" /tmp/moko-build-info.txt
    grep -Fq "non-installing Live USB preview" /tmp/moko-known-issues.txt
    grep -Fq "internal SSD/HDD partitions have no mountpoint" /tmp/moko-live-usb-checklist.md
    for path in \
      usr/local/bin/moko-shell \
      usr/local/bin/moko-compositor \
      usr/local/bin/moko-session \
      usr/local/bin/moko-cage-session \
      usr/local/bin/moko-desktop-session \
      usr/local/bin/moko-files \
      usr/local/bin/moko-browser \
      usr/local/bin/moko-settings \
      usr/local/bin/moko-terminal \
      usr/local/bin/moko-hardware-diagnostics \
      usr/local/bin/moko-ai-daemon \
      usr/local/libexec/moko-live-health-check \
      usr/local/libexec/moko-live-disk-safety-check \
      usr/local/libexec/moko-live-launch-monitor \
      usr/local/share/applications/org.moko.Files.desktop \
      usr/local/share/applications/org.moko.Browser.desktop \
      usr/local/share/applications/org.moko.Settings.desktop \
      usr/local/share/applications/org.moko.Terminal.desktop \
      usr/local/share/applications/org.moko.HardwareDiagnostics.desktop \
      usr/local/share/applications/zutty.desktop \
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
    unsquashfs -cat /tmp/filesystem.squashfs etc/greetd/config.toml \
      | grep -Fxq "command = \"/usr/local/bin/moko-desktop-session\""
    unsquashfs -cat /tmp/filesystem.squashfs \
      usr/local/share/applications/org.moko.Browser.desktop \
      | grep -Fxq "Exec=moko-browser %U"
    unsquashfs -cat /tmp/filesystem.squashfs \
      usr/local/share/applications/zutty.desktop > /tmp/zutty.desktop
    grep -Fxq "NoDisplay=true" /tmp/zutty.desktop
    grep -Fxq "Hidden=true" /tmp/zutty.desktop
    unsquashfs -cat /tmp/filesystem.squashfs \
      usr/local/libexec/moko-live-launch-monitor \
      > /tmp/moko-live-launch-monitor
    grep -Fq "MOKO_INPUT_*" /tmp/moko-live-launch-monitor
    grep -Fq "MOKO_DESKTOP_*" /tmp/moko-live-launch-monitor
    grep -Fq "MOKO_GLOBAL_ACTION" /tmp/moko-live-launch-monitor
    grep -Fq "MOKO_SHELL_OVERLAY" /tmp/moko-live-launch-monitor
    grep -Fq "MOKO_SCREENSHOT" /tmp/moko-live-launch-monitor
    grep -Fq "MOKO_NOTIFICATION_*" /tmp/moko-live-launch-monitor
    grep -Fq "MOKO_SLEEP" /tmp/moko-live-launch-monitor
    grep -Fq "MOKO_RESUME_*" /tmp/moko-live-launch-monitor
    unsquashfs -cat /tmp/filesystem.squashfs \
      usr/local/share/dbus-1/interfaces/org.moko.AI1.xml \
      | grep -Fq "method name=\"providerStatus\""
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
    if unsquashfs -lln /tmp/filesystem.squashfs var/lib/apt/lists \
        | awk '\''$1 ~ /^-/ { found = 1 } END { exit !found }'\''; then
      echo "Apt index metadata found in ISO; mirror timestamps break reproducibility." >&2
      exit 1
    fi
  '

for run in $(seq 1 "$RUNS"); do
  CONTAINER="moko-iso-smoke-$$-$run"
  SERIAL_NAME="$ARTIFACT_PREFIX-boot-$run.serial.log"
  DEBUG_NAME="$ARTIFACT_PREFIX-boot-$run.debug.log"
  SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run.png"
  SERIAL_PATH="$ISO_DIR/$SERIAL_NAME"

  echo "Cold boot $run/$RUNS (video: $QEMU_VIDEO_DEVICE)"
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
      "${QEMU_VIDEO_ARGUMENTS[@]}" \
      -audiodev driver=none,id=moko-audio \
      -device ich9-intel-hda \
      -device hda-duplex,audiodev=moko-audio \
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
  expected_compositor=moko
  expected_safe_graphics=0
  if [[ "$BOOT_MODE" == safe-graphics ]]; then
    expected_graphics=software
    expected_compositor=cage
    expected_safe_graphics=1
  fi
  health_pattern="MOKO_HEALTH result=pass mode=$BOOT_MODE .*compositor=$expected_compositor .*greetd_restarts=0 graphics=$expected_graphics firmware=$BOOT_FIRMWARE"
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

  if [[ "$BOOT_MODE" == desktop ]]; then
    grep -Fq "MOKO_COMPOSITOR_SHELL state=mapped app_id=org.moko.Shell width=1280 height=800 fullscreen=1" "$SERIAL_PATH" || {
      tail -100 "$SERIAL_PATH" >&2
      echo "MOKO Shell was not mapped fullscreen at the QEMU output size." >&2
      exit 1
    }
  fi

  if [[ "$run" == 1 && "$CONTROL_CENTER_TEST" == 1 ]]; then
    marker=$(serial_line_count)
    pointer_click 1018 20
    wait_for_serial_since "$marker" \
      "MOKO_CONTROL_CENTER state=open page=0 network_manager=1 wifi_device=[01] bluez_service=[01] bluetooth_adapter=[01] audio=[01] brightness=[01] battery=[01] power_mode=[01] uid=1000" 30 \
      "Control Center did not report real NetworkManager and Bluetooth hardware state."
    pointer_click 1212 82
    sleep 5
    marker=$(serial_line_count)
    pointer_click 1014 128
    wait_for_serial_since "$marker" \
      "MOKO_CONTROL_CENTER state=open page=1 network_manager=1 wifi_device=[01] bluez_service=[01] bluetooth_adapter=[01] audio=1 brightness=[01] battery=[01] power_mode=[01] uid=1000" 30 \
      "Control Center did not expose the live PipeWire audio state."
    CONTROL_CENTER_SOUND_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-control-center-sound.png"
    monitor "screendump /artifacts/$CONTROL_CENTER_SOUND_SCREENSHOT_NAME -f png"
    test "$(docker exec "$CONTAINER" stat -c %s "/artifacts/$CONTROL_CENTER_SOUND_SCREENSHOT_NAME")" -gt 10000
    marker=$(serial_line_count)
    pointer_click 1228 230
    wait_for_serial_since "$marker" \
      "MOKO_CONTROL_ACTION action=output_mute value=[01] ok=1 uid=1000" 30 \
      "Control Center did not change the real PipeWire output mute state."
    CONTROL_CENTER_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-control-center.png"
    monitor "screendump /artifacts/$CONTROL_CENTER_SCREENSHOT_NAME -f png"
    test "$(docker exec "$CONTAINER" stat -c %s "/artifacts/$CONTROL_CENTER_SCREENSHOT_NAME")" -gt 10000
    pointer_click 1018 20
  fi

  if [[ "$run" == 1 && "$INPUT_TEST" == 1 ]]; then
    marker=$(serial_line_count)
    pointer_click 1018 20
    wait_for_serial_since "$marker" \
      "MOKO_CONTROL_CENTER state=open page=0 network_manager=1 wifi_device=[01] bluez_service=[01] bluetooth_adapter=[01] audio=[01] brightness=[01] battery=[01] power_mode=[01] uid=1000" 30 \
      "Control Center did not open before selecting the Input page."
    sleep 5
    marker=$(serial_line_count)
    pointer_click 1212 126
    wait_for_serial_since "$marker" \
      "MOKO_CONTROL_CENTER state=open page=4 network_manager=1 wifi_device=[01] bluez_service=[01] bluetooth_adapter=[01] audio=[01] brightness=[01] battery=[01] power_mode=[01] uid=1000" 30 \
      "Control Center did not open the compositor-backed Input page."
    wait_for_serial_since "$marker" \
      "MOKO_INPUT_PANEL state=open protocol=1 touchpads=0 capabilities=0 input_state=0 acceleration=20 uid=1000" 30 \
      "Input page did not report the real QEMU no-trackpad state."
    grep -Eq "MOKO_INPUT_STATE touchpads=0 capabilities=0 state=0 acceleration=200" "$SERIAL_PATH" || {
      tail -120 "$SERIAL_PATH" >&2
      echo "Compositor did not publish its real input state." >&2
      exit 1
    }
    # TCG can acknowledge the QML state change before the next frame is ready.
    sleep 5
    INPUT_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-input.png"
    monitor "screendump /artifacts/$INPUT_SCREENSHOT_NAME -f png"
    test "$(docker exec "$CONTAINER" stat -c %s "/artifacts/$INPUT_SCREENSHOT_NAME")" -gt 10000
    sleep 5
    pointer_click 1018 20
  fi

  if [[ "$run" == 1 && "$USABILITY_TEST" == 1 ]]; then
    grep -Fq "MOKO_DESKTOP_CONFIG scale=100 scale_capabilities=1 keyboard_layout=0" "$SERIAL_PATH" || {
      tail -120 "$SERIAL_PATH" >&2
      echo "QEMU output did not reject unsafe 200% scaling." >&2
      exit 1
    }

    marker=$(serial_line_count)
    monitor "sendkey ctrl-spc"
    wait_for_serial_since "$marker" \
      "MOKO_DESKTOP_CONFIG scale=100 scale_capabilities=1 keyboard_layout=1" 20 \
      "Ctrl+Space did not select the Vietnamese keyboard layout."

    marker=$(serial_line_count)
    monitor "sendkey ctrl-spc"
    wait_for_serial_since "$marker" \
      "MOKO_DESKTOP_CONFIG scale=100 scale_capabilities=1 keyboard_layout=0" 20 \
      "Ctrl+Space did not restore the English keyboard layout."

    marker=$(serial_line_count)
    monitor "sendkey meta_l-n"
    wait_for_serial_since "$marker" \
      "MOKO_GLOBAL_ACTION action=3" 20 \
      "Meta+N did not reach the compositor-owned Notification Center action."
    wait_for_serial_since "$marker" \
      "MOKO_NOTIFICATION_CENTER state=open count=[0-9]+ unread=[0-9]+ uid=1000" 20 \
      "Notification Center did not open in the running Shell."
    USABILITY_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-usability.png"
    # TCG can emit the QML state marker before the updated frame is presented.
    sleep 5
    monitor "screendump /artifacts/$USABILITY_SCREENSHOT_NAME -f png"
    test "$(docker exec "$CONTAINER" stat -c %s "/artifacts/$USABILITY_SCREENSHOT_NAME")" -gt 10000

    marker=$(serial_line_count)
    monitor "sendkey esc"
    wait_for_serial_since "$marker" \
      "MOKO_SHELL_OVERLAY state=hidden" 20 \
      "Escape did not dismiss the Shell overlay."

    marker=$(serial_line_count)
    monitor "sendkey print"
    wait_for_serial_since "$marker" \
      "MOKO_GLOBAL_ACTION action=4" 20 \
      "Print did not reach the compositor-owned screenshot action."
    wait_for_serial_since "$marker" \
      "MOKO_SCREENSHOT state=saved file=Screenshot.*\.png uid=1000" 30 \
      "grim did not save a screenshot from the unprivileged Live session."
  fi

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
    for format in json txt; do
      if [[ "$format" == json ]]; then
        pointer_click 876 119
      else
        pointer_click 987 119
      fi
      sleep 2
      monitor "sendkey ret"
      hardware_deadline=$((SECONDS + 45))
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

    ai_ready_deadline=$((SECONDS + 30))
    while ! grep -Fq "MOKO_AI_UI state=ready provider=local-stub uid=1000" "$SERIAL_PATH"; do
      if (( SECONDS >= ai_ready_deadline )); then
        tail -100 "$SERIAL_PATH" >&2
        echo "MOKO AI UI did not report an available provider." >&2
        exit 1
      fi
      sleep 1
    done
    grep -F "MOKO_AI_UI state=ready provider=local-stub uid=1000" "$SERIAL_PATH" | tail -1

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
    marker=$(serial_line_count)
    monitor "sendkey ret"

    wait_for_serial_since "$marker" \
      "MOKO_AI_UI state=processing provider=local-stub uid=1000" 30 \
      "MOKO AI UI did not enter its processing state."

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
      while ! grep -E -q "MOKO_COMPOSITOR_WINDOW state=mapped id=[0-9]+ app_id=$AI_EXPECT_APP_ID" "$SERIAL_PATH"; do
        if (( SECONDS >= ai_app_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "MOKO AI launched app did not map in moko-compositor." >&2
          exit 1
        fi
        sleep 1
      done
      if grep -Fq "MOKO_SHELL_SURFACE state=hidden app_id=$AI_EXPECT_APP_ID" "$SERIAL_PATH"; then
        echo "MOKO Shell incorrectly used the Cage workaround under moko-compositor." >&2
        exit 1
      fi
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
      while ! grep -E -q "MOKO_COMPOSITOR_WINDOW state=unmapped id=[0-9]+ app_id=$AI_EXPECT_APP_ID" "$SERIAL_PATH"; do
        if (( SECONDS >= ai_return_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "MOKO AI launched app did not close in moko-compositor." >&2
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
      sleep 0.2
    done
    sleep 1
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
      ready_deadline=$((SECONDS + APP_READY_TIMEOUT_SECONDS))
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
      for format in json txt; do
        if [[ "$format" == json ]]; then
          pointer_click 876 119
          sleep 2
          monitor "screendump /artifacts/$ARTIFACT_PREFIX-boot-$run-hardware-click.png -f png"
        else
          pointer_click 987 119
        fi
        sleep 2
        monitor "sendkey ret"
        hardware_deadline=$((SECONDS + 45))
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

    surface_deadline=$((SECONDS + APP_READY_TIMEOUT_SECONDS))
    if [[ "$BOOT_MODE" == safe-graphics ]]; then
      while ! grep -Fq "MOKO_SHELL_SURFACE state=hidden app_id=$LAUNCH_APP_ID" "$SERIAL_PATH"; do
        if (( SECONDS >= surface_deadline )); then
          tail -100 "$SERIAL_PATH" >&2
          echo "MOKO Shell did not yield the Cage surface to $LAUNCH_APP_ID." >&2
          exit 1
        fi
        sleep 1
      done
      grep -F "MOKO_SHELL_SURFACE state=hidden app_id=$LAUNCH_APP_ID" "$SERIAL_PATH" | tail -1
    else
      while ! grep -E -q "MOKO_COMPOSITOR_WINDOW state=mapped id=[0-9]+ app_id=$LAUNCH_APP_ID" "$SERIAL_PATH"; do
        if (( SECONDS >= surface_deadline )); then
          tail -100 "$SERIAL_PATH" >&2
          echo "$LAUNCH_APP_ID did not map in moko-compositor." >&2
          exit 1
        fi
        sleep 1
      done
      if grep -Fq "MOKO_SHELL_SURFACE state=hidden app_id=$LAUNCH_APP_ID" "$SERIAL_PATH"; then
        echo "MOKO Shell incorrectly used the Cage workaround under moko-compositor." >&2
        exit 1
      fi
      grep -E "MOKO_COMPOSITOR_WINDOW state=mapped id=[0-9]+ app_id=$LAUNCH_APP_ID" "$SERIAL_PATH" | tail -1
    fi

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
    launch_screenshot_deadline=$((SECONDS + SCREENSHOT_TIMEOUT_SECONDS))
    launch_screenshot_size=0
    while :; do
      monitor "screendump /artifacts/$LAUNCH_SCREENSHOT_NAME -f png"
      launch_screenshot_size=$(docker exec "$CONTAINER" stat -c %s "/artifacts/$LAUNCH_SCREENSHOT_NAME")
      if (( launch_screenshot_size > 10000 )); then
        break
      fi
      if (( SECONDS >= launch_screenshot_deadline )); then
        echo "Launched application framebuffer remained blank after $SCREENSHOT_TIMEOUT_SECONDS seconds ($launch_screenshot_size bytes)." >&2
        exit 1
      fi
      sleep 5
    done
    if grep -Fq "MOKO_APP_LAUNCH app_id=$LAUNCH_APP_ID state=failed" "$SERIAL_PATH"; then
      tail -80 "$SERIAL_PATH" >&2
      echo "Application exited during launch validation." >&2
      exit 1
    fi

    if [[ "$RESUME_TEST" == 1 ]]; then
      ai_ready_deadline=$((SECONDS + 30))
      while ! grep -Fq "MOKO_AI_UI state=ready provider=local-stub uid=1000" "$SERIAL_PATH"; do
        if (( SECONDS >= ai_ready_deadline )); then
          tail -120 "$SERIAL_PATH" >&2
          echo "MOKO AI was not ready before the suspend baseline." >&2
          exit 1
        fi
        sleep 1
      done

      marker=$(serial_line_count)
      monitor "sendkey ctrl-l"
      sleep 0.5
      send_text "example.com"
      monitor "sendkey ret"
      wait_for_serial_since "$marker" \
        "MOKO_BROWSER_PAGE state=loaded scheme=https host=example.com uid=1000" 90 \
        "MOKO Browser did not load HTTPS before the suspend test."
      wait_for_serial_since "$marker" \
        "MOKO_BROWSER_JAVASCRIPT state=pass scheme=https host=example.com uid=1000" 30 \
        "MOKO Browser did not execute JavaScript before the suspend test."

      marker=$(serial_line_count)
      pointer_click 1078 20
      wait_for_serial_since "$marker" \
        "MOKO_CONTROL_CENTER state=open page=3 .* uid=1000" 30 \
        "Control Center did not open the Power page before suspend."
      sleep 3
      pointer_click 1060 407
      sleep 1
      pointer_click 1160 454
      wait_for_serial_since "$marker" \
        "MOKO_CONTROL_ACTION action=suspend state=requested uid=1000" 30 \
        "The unprivileged MOKO Power action did not request suspend through logind."
      wait_for_qemu_status suspended 60
      qmp '{"execute":"system_wakeup"}'
      wait_for_qemu_status running 30
      wait_for_serial_since "$marker" \
        "MOKO_SLEEP state=preparing compositor=1 browser_running=1 ai=1 network_manager=1 wifi_connected=[01] bluez_service=[01] bluetooth_adapter=[01] bluetooth_powered=[01] audio=1 input_protocol=1 touchpads=[0-9]+ battery=[01] power_mode=[01] uid=1000" 60 \
        "The Shell did not record the real pre-suspend service baseline."
      wait_for_serial_since "$marker" \
        "MOKO_SLEEP state=resumed uid=1000" 60 \
        "The Shell did not observe logind resume."
      wait_for_serial_since "$marker" \
        "MOKO_RESUME_HEALTH result=pass desktop_protocol=1 compositor=1 browser_expected=1 browser_mapped=1 ai=1 provider=1 network_manager=1 wifi_device=[01] wifi_enabled=[01] wifi_connected=[01] bluez_service=[01] bluetooth_adapter=[01] bluetooth_powered=[01] audio=1 input_protocol=1 touchpads=[0-9]+ battery=[01] brightness=[01] power_mode=[01] uid=1000" 90 \
        "The Shell did not recover its compositor, Browser, AI and system-service state."
      if awk -v start="$marker" \
          'NR > start && /MOKO_RESUME_HEALTH result=fail/ { failed = 1 } END { exit !failed }' \
          "$SERIAL_PATH"; then
        tail -140 "$SERIAL_PATH" >&2
        echo "The Shell reported failed resume health." >&2
        exit 1
      fi

      marker=$(serial_line_count)
      monitor "sendkey esc"
      wait_for_serial_since "$marker" \
        "MOKO_SHELL_OVERLAY state=hidden" 30 \
        "The Power overlay did not dismiss and restore Browser focus after resume."

      RESUME_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-resumed.png"
      resume_screenshot_deadline=$((SECONDS + SCREENSHOT_TIMEOUT_SECONDS))
      resume_screenshot_size=0
      while (( resume_screenshot_size <= 10000 )); do
        monitor "screendump /artifacts/$RESUME_SCREENSHOT_NAME -f png"
        resume_screenshot_size=$(docker exec "$CONTAINER" \
          stat -c %s "/artifacts/$RESUME_SCREENSHOT_NAME")
        if (( SECONDS >= resume_screenshot_deadline )); then
          echo "Display output did not recover within $SCREENSHOT_TIMEOUT_SECONDS seconds after resume ($resume_screenshot_size bytes)." >&2
          exit 1
        fi
        sleep 5
      done

      marker=$(serial_line_count)
      monitor "sendkey ctrl-r"
      wait_for_serial_since "$marker" \
        "MOKO_BROWSER_PAGE state=loaded scheme=https host=example.com uid=1000" 90 \
        "MOKO Browser did not reload HTTPS after resume."
      wait_for_serial_since "$marker" \
        "MOKO_BROWSER_JAVASCRIPT state=pass scheme=https host=example.com uid=1000" 30 \
        "MOKO Browser did not execute JavaScript after resume."
    fi

    if [[ "$WINDOW_WORKFLOW" == 1 ]]; then
      marker=$(serial_line_count)
      monitor "sendkey meta_l-left"
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Files state=17 " 20 \
        "MOKO Files did not snap left through compositor state."

      marker=$(serial_line_count)
      monitor "sendkey meta_l-right"
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Files state=33 " 20 \
        "MOKO Files did not snap right through compositor state."

      marker=$(serial_line_count)
      monitor "sendkey meta_l-up"
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Files state=3 " 20 \
        "MOKO Files did not maximize through compositor state."

      marker=$(serial_line_count)
      monitor "sendkey meta_l-down"
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Files state=1 " 20 \
        "MOKO Files did not restore from maximized state."

      marker=$(serial_line_count)
      monitor "sendkey meta_l-f"
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Files state=9 " 20 \
        "MOKO Files did not enter fullscreen compositor state."

      marker=$(serial_line_count)
      monitor "sendkey meta_l-f"
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Files state=1 " 20 \
        "MOKO Files did not leave fullscreen compositor state."

      marker=$(serial_line_count)
      monitor "sendkey meta_l-down"
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Files state=4 " 20 \
        "MOKO Files did not minimize through compositor state."

      marker=$(serial_line_count)
      dock_app_click org.moko.Files
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Files state=1 " 20 \
        "The Files Dock icon did not focus and restore its compositor window."
      files_launch_count=$(grep -Ec \
        'MOKO_APP_LAUNCH app_id=org.moko.Files state=running pid=[1-9][0-9]* uid=1000' \
        "$SERIAL_PATH")
      [[ "$files_launch_count" == 1 ]] || {
        echo "The Files Dock icon relaunched the app instead of focusing it." >&2
        exit 1
      }

      marker=$(serial_line_count)
      dock_app_click org.moko.Settings
      wait_for_serial_since "$marker" \
        "MOKO_COMPOSITOR_WINDOW state=mapped id=[0-9]+ app_id=org.moko.Settings" 30 \
        "MOKO Settings did not map alongside MOKO Files."
      wait_for_serial_since "$marker" \
        "MOKO_APP_READY app_id=org.moko.Settings state=ready" 30 \
        "MOKO Settings did not become ready in the multi-window workflow."

      # Let the Qt client commit the compositor-requested work-area size before
      # targeting its bottom-right resize handle.
      sleep 3
      monitor "screendump /artifacts/$ARTIFACT_PREFIX-boot-$run-multi-window.png -f png"

      marker=$(serial_line_count)
      pointer_drag 1184 680 1220 710
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_INTERACTION state=end operation=resize id=[0-9]+ app_id=org.moko.Settings .* changed=1" 20 \
        "Dragging the MOKO Settings resize handle did not resize the real window."

      sleep 2
      marker=$(serial_line_count)
      pointer_drag 245 130 345 180
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_INTERACTION state=end operation=move id=[0-9]+ app_id=org.moko.Settings .* changed=1" 20 \
        "Dragging the MOKO Settings title bar did not move the real window."

      marker=$(serial_line_count)
      monitor "sendkey alt-tab"
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Files state=1 " 20 \
        "Alt+Tab did not focus MOKO Files."

      marker=$(serial_line_count)
      monitor "sendkey alt-tab"
      wait_for_serial_since "$marker" \
        "MOKO_WINDOW_STATE id=[0-9]+ app_id=org.moko.Settings state=1 " 20 \
        "Alt+Tab did not focus MOKO Settings."

      marker=$(serial_line_count)
      monitor "sendkey ctrl-q"
      wait_for_serial_since "$marker" \
        "MOKO_COMPOSITOR_WINDOW state=unmapped id=[0-9]+ app_id=org.moko.Settings" 30 \
        "MOKO Settings did not close cleanly after the multi-window workflow."
    fi

    if [[ "$BROWSER_TEST" == 1 ]]; then
      grep -Fq "MOKO_BROWSER_READY sandbox=enabled web_security=enabled uid=1000" "$SERIAL_PATH" || {
        tail -120 "$SERIAL_PATH" >&2
        echo "MOKO Browser did not retain its sandbox and web-security policy." >&2
        exit 1
      }

      marker=$(serial_line_count)
      monitor "sendkey ctrl-l"
      sleep 0.5
      send_text "example.com"
      monitor "sendkey ret"
      wait_for_serial_since "$marker" \
        "MOKO_BROWSER_PAGE state=loaded scheme=https host=example.com uid=1000" 90 \
        "MOKO Browser did not render the real HTTPS validation page."
      wait_for_serial_since "$marker" \
        "MOKO_BROWSER_JAVASCRIPT state=pass scheme=https host=example.com uid=1000" 30 \
        "MOKO Browser did not execute JavaScript on the HTTPS validation page."

      marker=$(serial_line_count)
      monitor "sendkey ctrl-l"
      sleep 0.5
      send_text "deb.debian.org/debian/pool/main/h/hello/hello_2.10-5_amd64.deb"
      monitor "sendkey ret"
      wait_for_serial_since "$marker" \
        "MOKO_BROWSER_DOWNLOAD state=completed file=hello_2.10-5_amd64.deb bytes=[1-9][0-9]+ uid=1000" 120 \
        "MOKO Browser did not complete the real HTTPS download."

      BROWSER_DOWNLOAD_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-browser-download.png"
      monitor "screendump /artifacts/$BROWSER_DOWNLOAD_SCREENSHOT_NAME -f png"
      test "$(docker exec "$CONTAINER" stat -c %s "/artifacts/$BROWSER_DOWNLOAD_SCREENSHOT_NAME")" -gt 10000

      marker=$(serial_line_count)
      monitor "sendkey ctrl-shift-o"
      wait_for_serial_since "$marker" \
        "MOKO_BROWSER_ACTION action=show_downloads state=accepted uid=1000" 30 \
        "MOKO Browser did not open its download directory through MOKO Files."
      wait_for_serial_since "$marker" \
        "MOKO_FILES_LOCATION location=.*/Downloads count=[1-9][0-9]* uid=1000" 30 \
        "MOKO Files did not show the downloaded file."
      wait_for_serial_since "$marker" \
        "MOKO_COMPOSITOR_WINDOW state=mapped id=[0-9]+ app_id=org.moko.Files" 30 \
        "MOKO Files did not map above the Browser download workflow."

      BROWSER_FILES_SCREENSHOT_NAME="$ARTIFACT_PREFIX-boot-$run-browser-files.png"
      sleep 2
      monitor "screendump /artifacts/$BROWSER_FILES_SCREENSHOT_NAME -f png"
      test "$(docker exec "$CONTAINER" stat -c %s "/artifacts/$BROWSER_FILES_SCREENSHOT_NAME")" -gt 10000

      marker=$(serial_line_count)
      monitor "sendkey ctrl-q"
      wait_for_serial_since "$marker" \
        "MOKO_COMPOSITOR_WINDOW state=unmapped id=[0-9]+ app_id=org.moko.Files" 30 \
        "MOKO Files did not close after validating the Browser download."
    fi

    if [[ "$REQUIRE_APP_READY" == 1 ]]; then
      monitor "sendkey ctrl-q"
      if [[ "$SETTINGS_OPEN_HARDWARE" == 1 ]]; then
        sleep 3
        monitor "sendkey ctrl-q"
      fi
      return_deadline=$((SECONDS + 30))
      return_pattern="MOKO_COMPOSITOR_WINDOW state=unmapped id=[0-9]+ app_id=$LAUNCH_APP_ID"
      if [[ "$BOOT_MODE" == safe-graphics ]]; then
        return_pattern="MOKO_SHELL_SURFACE state=shown app_id=$LAUNCH_APP_ID"
      fi
      while ! grep -E -q "$return_pattern" "$SERIAL_PATH"; do
        if (( SECONDS >= return_deadline )); then
          tail -100 "$SERIAL_PATH" >&2
          echo "$LAUNCH_APP_ID did not close cleanly." >&2
          exit 1
        fi
        sleep 1
      done
      grep -E "$return_pattern" "$SERIAL_PATH" | tail -1
    fi
  fi

  if [[ "$QEMU_EXIT_ACTION" == powerdown ]]; then
    monitor system_powerdown
  else
    # QEMU standard VGA/q35 can fail its emulated S5 transition after S3.
    # Normal regression runs still use powerdown; this only cleans up after
    # the resume gate has independently passed every guest assertion.
    monitor quit
  fi
  deadline=$((SECONDS + SHUTDOWN_TIMEOUT_SECONDS))
  eject_deadline=$((SECONDS + 30))
  eject_sent=0
  while [[ $(docker inspect -f '{{.State.Running}}' "$CONTAINER") == true ]]; do
    if [[ "$QEMU_EXIT_ACTION" == powerdown && "$eject_sent" == 0 \
        && $SECONDS -ge $eject_deadline ]]; then
      monitor "eject ide2-cd0"
      monitor "sendkey ret"
      eject_sent=1
    fi
    if (( SECONDS >= deadline )); then
      echo "Cold boot $run did not complete $QEMU_EXIT_ACTION within $SHUTDOWN_TIMEOUT_SECONDS seconds." >&2
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
