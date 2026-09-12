#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
AUTO_CONFIG="$ROOT/image/live-build/auto-config.sh"
GRUB_MENU="$ROOT/image/live-build/config/bootloaders/grub-pc/grub.cfg"
SYSLINUX_MENU="$ROOT/image/live-build/config/bootloaders/syslinux_common/live.cfg.in"
PLYMOUTH_SCRIPT="$ROOT/assets/boot/moko.script"

for flag in \
  "quiet splash" \
  "noprompt" \
  "noeject" \
  "loglevel=0" \
  "systemd.show_status=false" \
  "rd.systemd.show_status=false" \
  "udev.log_level=0" \
  "vt.global_cursor_default=0" \
  "logo.nologo" \
  "plymouth.ignore-serial-consoles"
do
  grep -Fq "$flag" "$AUTO_CONFIG"
done

common_append=$(grep -- '--bootappend-live' "$AUTO_CONFIG")
if grep -Fq 'console=tty0' <<<"$common_append"; then
  echo "The normal boot profile still writes to the visible virtual console." >&2
  exit 1
fi

for menu in "$GRUB_MENU" "$SYSLINUX_MENU"; do
  grep -Fq 'moko.mode=desktop' "$menu"
  grep -Fq 'moko.mode=safe-graphics' "$menu"
  diagnostics=$(grep 'moko.mode=hardware-diagnostics' "$menu")
  for flag in plymouth.enable=0 systemd.show_status=true rd.systemd.show_status=true \
    loglevel=4 vt.global_cursor_default=1 console=tty0
  do
    grep -Fq "$flag" <<<"$diagnostics"
  done
done

grep -Fxq 'plymouth' "$ROOT/image/live-build/config/package-lists/moko.list.chroot"
grep -Fxq 'plymouth-themes' "$ROOT/image/live-build/config/package-lists/moko.list.chroot"
grep -Fq 'plymouth-set-default-theme -R moko' \
  "$ROOT/image/live-build/config/hooks/normal/0150-moko-plymouth.hook.chroot"

for image in \
  "$ROOT/assets/boot/moko-boot.png" \
  "$ROOT/assets/boot/moko-highlight-dim.png" \
  "$ROOT/assets/boot/moko-highlight-bright.png" \
  "$ROOT/image/live-build/config/bootloaders/grub-pc/splash.png" \
  "$ROOT/image/live-build/config/bootloaders/isolinux/splash.png"
do
  test -s "$image"
  signature=$(od -An -tx1 -N8 "$image" | tr -d ' \n')
  [[ "$signature" == 89504e470d0a1a0a ]]
done

python3 - "$ROOT" <<'PY'
import sys
from pathlib import Path

from PIL import Image

root = Path(sys.argv[1])
for relative in (
    "image/live-build/config/bootloaders/grub-pc/splash.png",
    "image/live-build/config/bootloaders/isolinux/splash.png",
):
    image = Image.open(root / relative).convert("RGB")
    if image.getextrema() != ((0, 0), (0, 0), (0, 0)):
        raise SystemExit(f"Bootloader splash is not completely black: {relative}")

image = Image.open(root / "assets/boot/moko-boot.png").convert("RGB")
lower = image.crop((0, 575, image.width, image.height))
if max(channel[1] for channel in lower.getextrema()) > 8:
    raise SystemExit("Plymouth still contains the inactive lower loading bar")
center = image.crop((image.width // 3, image.height // 4,
                     image.width * 2 // 3, image.height * 3 // 5))
if max(channel[1] for channel in center.getextrema()) < 220:
    raise SystemExit("Plymouth MOKO wordmark is missing")
PY

grep -Fq 'mode == "boot"' "$PLYMOUTH_SCRIPT"
grep -Fq 'pulse_frames = 210' "$PLYMOUTH_SCRIPT"
grep -Fq 'Plymouth.SetRefreshRate(30)' "$PLYMOUTH_SCRIPT"
grep -Fq 'pulse_opacity = 0.75 + 0.25 * Math.Cos' "$PLYMOUTH_SCRIPT"
grep -Fq 'Plymouth.SetDisplayMessageFunction(ignore_message)' "$PLYMOUTH_SCRIPT"
grep -Fq 'Plymouth.SetDisplayPasswordFunction(ignore_password)' "$PLYMOUTH_SCRIPT"

if grep -R -I -n -E -i 'debian|gnu/linux|hard-hat|systemd' \
    "$ROOT/assets/boot" \
    "$ROOT/image/live-build/config/bootloaders/grub-pc/live-theme" \
    "$ROOT/image/live-build/config/bootloaders/isolinux/stdmenu.cfg"; then
  echo "Consumer boot assets contain forbidden implementation branding or debug text." >&2
  exit 1
fi

grep -Fq 'ForwardToConsole=no' \
  "$ROOT/image/live-build/config/includes.chroot/etc/systemd/journald.conf.d/10-moko-quiet-console.conf"
grep -Fq 'set timeout_style=hidden' \
  "$ROOT/image/live-build/config/bootloaders/grub-pc/config.cfg"
grep -Fq 'set timeout=2' \
  "$ROOT/image/live-build/config/bootloaders/grub-pc/config.cfg"
grep -Fq 'timeout 20' \
  "$ROOT/image/live-build/config/bootloaders/isolinux/isolinux.cfg"
if grep -Fq 'insmod play' "$ROOT/image/live-build/config/bootloaders/grub-pc/config.cfg"; then
  echo "Normal boot still enables the GRUB audio cue." >&2
  exit 1
fi
if grep -Fq '+ progress_bar' "$ROOT/image/live-build/config/bootloaders/grub-pc/live-theme/theme.txt"; then
  echo "The bootloader theme still renders an inactive timeout bar." >&2
  exit 1
fi
if grep -Fq 'console=ttyS0' "$AUTO_CONFIG"; then
  echo "Normal boot still depends on a QEMU serial console." >&2
  exit 1
fi
grep -Fq 'MOKO_SESSION_OUTPUT_ROUTED' "$ROOT/core/moko-session/moko-desktop-session"
grep -Fq 'systemd-cat --identifier=moko-session' "$ROOT/core/moko-session/moko-desktop-session"
grep -Fq 'MOKO_BOOT_TIMING stage=%s uptime_ms=%s uid=%s' \
  "$ROOT/core/moko-session/moko-desktop-session"
grep -Fq 'MOKO_BOOT_TIMING stage=shell-launch uptime_ms=%s uid=%s' \
  "$ROOT/core/moko-session/moko-session"
grep -Fq 'MOKO_BOOT_TIMING stage=%1 uptime_ms=%2 uid=%3' "$ROOT/shell/src/main.cpp"
grep -Fq 'tail -s 0.05 -n +1 -F "$events"' \
  "$ROOT/image/live-build/config/includes.chroot/usr/local/libexec/moko-live-launch-monitor"
grep -Fq 'MOKO_POWER_*' \
  "$ROOT/image/live-build/config/includes.chroot/usr/local/libexec/moko-live-launch-monitor"
grep -Fq 'Before=greetd.service' \
  "$ROOT/image/live-build/config/includes.chroot/etc/systemd/system/moko-live-launch-monitor.service"
grep -Fq 'DefaultDependencies=no' \
  "$ROOT/image/live-build/config/includes.chroot/etc/systemd/system/moko-live-launch-monitor.service"
grep -Eq '^Before=(.*[[:space:]])?shutdown\.target([[:space:]]|$)' \
  "$ROOT/image/live-build/config/includes.chroot/etc/systemd/system/moko-live-launch-monitor.service"
if grep -Fq 'After=greetd.service' \
    "$ROOT/image/live-build/config/includes.chroot/etc/systemd/system/moko-live-launch-monitor.service"; then
  echo "Launch telemetry monitor would stop before the graphical session." >&2
  exit 1
fi

iso_test="$ROOT/scripts/test-iso-docker.sh"
desktop_shutdown=$(sed -n '/^request_desktop_shutdown()/,/^}/p' "$iso_test")
fallback_shutdown=$(sed -n '/^request_fallback_shutdown()/,/^}/p' "$iso_test")
grep -Fq 'qmp_power_key true' <<<"$desktop_shutdown"
grep -Fq 'qmp_power_key false' <<<"$desktop_shutdown"
grep -Fq 'MOKO_POWER_MENU state=requested uid=1000' <<<"$desktop_shutdown"
grep -Fq 'MOKO_CONTROL_ACTION action=poweroff state=requested uid=1000' \
  <<<"$desktop_shutdown"
if grep -Fq 'system_powerdown' <<<"$desktop_shutdown"; then
  echo "Desktop shutdown bypasses the inhibited MOKO Power menu." >&2
  exit 1
fi
grep -Fq 'monitor system_powerdown' <<<"$fallback_shutdown"

power_menu="$ROOT/shell/qml/components/PowerMenu.qml"
compositor_source="$ROOT/compositor/moko-compositor/src/main.c"
shell_main_qml="$ROOT/shell/qml/Main.qml"
grep -Fq 'Qt.callLater(function()' "$power_menu"
grep -Fq 'root.forceActiveFocus()' "$power_menu"
grep -Fq 'shutdownButton.forceActiveFocus()' "$power_menu"
grep -Fq 'return root.activeFocus && shutdownButton.activeFocus' "$power_menu"
grep -Fq 'if (!root.activeFocus || !shutdownButton.activeFocus)' "$power_menu"
grep -Fq 'focusRetry.restart()' "$power_menu"
grep -Fq 'focusRetry.stop()' "$power_menu"
grep -Fq 'sequence: "Return"' "$power_menu"
grep -Fq 'sequence: "Enter"' "$power_menu"
grep -Fq 'onActivated: root.activateFocusedAction()' "$power_menu"
grep -Fq 'mokoSessionLifecycle.beginShutdown()' "$shell_main_qml"
grep -Fq 'function onShutdownBlackoutReady()' "$shell_main_qml"
grep -Fq 'mokoSessionLifecycle.notifyPowerActionRequested()' "$shell_main_qml"
grep -Fq 'mokoSystemControl.powerOff()' "$shell_main_qml"
grep -Fq 'mokoSystemControl.reboot()' "$shell_main_qml"
if sed -n '/function runPowerAction/,/function updateClock/p' "$shell_main_qml" \
    | grep -Fq 'mokoSystemControl.powerOff()'; then
  echo "PowerOff must wait for the local shutdown blackout acknowledgement." >&2
  exit 1
fi
if sed -n '/function runPowerAction/,/function updateClock/p' "$shell_main_qml" \
    | grep -Fq 'mokoSystemControl.reboot()'; then
  echo "Reboot must wait for the local shutdown blackout acknowledgement." >&2
  exit 1
fi
grep -Fq 'MOKO_POWER_MENU state=ready uid=%1' \
  "$ROOT/shell/src/windowmanager.cpp"
grep -Fq 'MOKO_POWER_MENU state=ready uid=1000' <<<"$desktop_shutdown"
power_request=$(sed -n '/^static void request_power_menu/,/^}/p' "$compositor_source")
grep -Fq 'set_shell_overlay(server, true);' <<<"$power_request"
grep -Fq 'moko_window_manager_v1_send_power_menu' <<<"$power_request"
grep -Fq 'wl_display_flush_clients(server->display);' <<<"$power_request"

shutdown_guard="$ROOT/image/live-build/config/includes.chroot/usr/local/libexec/moko-shutdown-blackout-guard"
shutdown_guard_unit="$ROOT/image/live-build/config/includes.chroot/etc/systemd/system/moko-shutdown-blackout-guard.service"
logind_shutdown_config="$ROOT/image/live-build/config/includes.chroot/etc/systemd/logind.conf.d/10-moko-shutdown-visual.conf"
grep -Fq 'all_outputs_presented_black_frame' "$compositor_source"
grep -Fq 'if (!output->shutdown_black_frame_presented)' "$compositor_source"
if sed -n '/static void output_frame/,/static void output_present/p' "$compositor_source" \
    | grep -Fq 'announce_shutdown_blackout'; then
  echo "Compositor acknowledges shutdown before the black frame is presented." >&2
  exit 1
fi
grep -Eq '^After=(.*[[:space:]])?greetd\.service([[:space:]]|$)' "$shutdown_guard_unit"
grep -Fq 'DefaultDependencies=no' "$shutdown_guard_unit"
grep -Fq 'Before=shutdown.target' "$shutdown_guard_unit"
grep -Fq 'systemd-inhibit --what=shutdown' "$shutdown_guard_unit"
grep -Fq '/usr/local/libexec/moko-shutdown-blackout-guard monitor' "$shutdown_guard_unit"
grep -Fxq '[Login]' "$logind_shutdown_config"
grep -Fxq 'InhibitDelayMaxSec=15s' "$logind_shutdown_config"
grep -Fq 'systemctl enable moko-shutdown-blackout-guard.service' \
  "$ROOT/image/live-build/config/hooks/normal/0300-greetd.hook.chroot"

guard_runtime=$(mktemp -d)
trap 'rm -rf "$guard_runtime"' EXIT
guard_events="$guard_runtime/events"
printf '%s\n' 'MOKO_SHUTDOWN_VISUAL state=ready uid=1000' > "$guard_events"
env MOKO_SHUTDOWN_GUARD_EVENTS="$guard_events" \
  MOKO_SHUTDOWN_GUARD_ATTEMPTS=30 \
  MOKO_SHUTDOWN_GUARD_SLEEP=0.02 \
  "$shutdown_guard" monitor &
shutdown_guard_pid=$!
(
  sleep 0.1
  printf '%s\n' 'MOKO_SHUTDOWN_VISUAL state=fading uid=1000' >> "$guard_events"
  printf '%s\n' 'MOKO_SHUTDOWN_VISUAL state=blackout uid=1000' >> "$guard_events"
  printf '%s\n' 'MOKO_SHUTDOWN_VISUAL state=ready uid=1000' >> "$guard_events"
) &
guard_writer_pid=$!
wait "$guard_writer_pid"
wait "$shutdown_guard_pid"

echo "MOKO boot and shutdown presentation configuration passed."
