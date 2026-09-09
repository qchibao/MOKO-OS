#!/bin/sh
set -eu

desktop_session=$1
compositor=$2
client=$3
mock_compositor=$4
runtime=$(mktemp -d)
events=$runtime/events.log
compositor_state=$runtime/compositor-attempts
consumer_tty=$runtime/consumer-tty
uptime_file=$runtime/uptime
socket=wayland-moko-0

cleanup() {
  if [ "${MOKO_KEEP_TEST_OUTPUT:-0}" = 1 ]; then
    printf 'MOKO desktop session test output retained at %s\n' "$runtime"
  else
    rm -rf "$runtime"
  fi
}
trap cleanup EXIT INT TERM

chmod 700 "$runtime"
export XDG_RUNTIME_DIR=$runtime
export WAYLAND_DISPLAY=missing-parent-display
export WAYLAND_SOCKET=99
export DISPLAY=:99
export WLR_BACKENDS=headless
export WLR_HEADLESS_OUTPUTS=1
export WLR_RENDERER=pixman
export WLR_LIBINPUT_NO_DEVICES=1
export MOKO_COMPOSITOR=moko
export MOKO_COMPOSITOR_BIN=$mock_compositor
export MOKO_SESSION_BIN=$client
export MOKO_EXPECT_WAYLAND_DISPLAY=$socket
export MOKO_DESKTOP_SESSION_TEST_EVENTS=$events
export MOKO_COMPOSITOR_EVENTS=$events
export MOKO_LIVE_LAUNCH_EVENTS=$events
export MOKO_MOCK_COMPOSITOR_STATE=$compositor_state
export MOKO_REAL_COMPOSITOR=$compositor
export MOKO_FAIL_FIRST_AFTER_SOCKET=1
export MOKO_CONSUMER_TTY=$consumer_tty
export MOKO_BOOT_UPTIME_FILE=$uptime_file
: > "$consumer_tty"
printf '12.34 4.56\n' > "$uptime_file"

"$desktop_session"

[ "$(cat "$compositor_state")" = 2 ]
grep -q "^MOKO_COMPOSITOR_READY socket=$socket " "$events"
grep -q '^MOKO_BOOT_TIMING stage=session-start uptime_ms=12340 uid=' "$events"
grep -q '^MOKO_BOOT_TIMING stage=compositor-ready uptime_ms=12340 uid=' "$events"
grep -q "^MOKO_DESKTOP_SESSION_TEST state=connected socket=$socket$" "$events"
[ "$(LC_ALL=C grep -ao "$(printf '\033\[2J')" "$consumer_tty" | wc -l)" -eq 2 ]
