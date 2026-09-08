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
grep -Fq 'MOKO_SESSION_OUTPUT_ROUTED' "$ROOT/core/moko-session/moko-desktop-session"
grep -Fq 'systemd-cat --identifier=moko-session' "$ROOT/core/moko-session/moko-desktop-session"
grep -Fq 'tail -s 0.05 -n +1 -F "$events"' \
  "$ROOT/image/live-build/config/includes.chroot/usr/local/libexec/moko-live-launch-monitor"
grep -Fq 'Before=greetd.service' \
  "$ROOT/image/live-build/config/includes.chroot/etc/systemd/system/moko-live-launch-monitor.service"
if grep -Fq 'After=greetd.service' \
    "$ROOT/image/live-build/config/includes.chroot/etc/systemd/system/moko-live-launch-monitor.service"; then
  echo "Launch telemetry monitor would stop before the graphical session." >&2
  exit 1
fi

shutdown_guard="$ROOT/image/live-build/config/includes.chroot/usr/local/libexec/moko-shutdown-blackout-guard"
shutdown_guard_unit="$ROOT/image/live-build/config/includes.chroot/etc/systemd/system/moko-shutdown-blackout-guard.service"
grep -Eq '^After=(.*[[:space:]])?greetd\.service([[:space:]]|$)' "$shutdown_guard_unit"
grep -Fq 'DefaultDependencies=no' "$shutdown_guard_unit"
grep -Fq 'Before=shutdown.target' "$shutdown_guard_unit"
grep -Fq 'systemd-inhibit --what=shutdown' "$shutdown_guard_unit"
grep -Fq '/usr/local/libexec/moko-shutdown-blackout-guard monitor' "$shutdown_guard_unit"
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
