#!/bin/sh
set -eu

compositor=$1
shell=$2
files=$3
settings=$4
runtime=$(mktemp -d)
events=$runtime/events.log
socket=wayland-moko-qt-session
compositor_pid=
shell_pid=
files_pid=
settings_pid=

stop_process() {
  pid=$1
  if [ -n "$pid" ]; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
}

cleanup() {
  stop_process "$settings_pid"
  stop_process "$files_pid"
  stop_process "$shell_pid"
  stop_process "$compositor_pid"
  if [ "${MOKO_KEEP_TEST_OUTPUT:-0}" = 1 ]; then
    printf 'MOKO Qt session output retained at %s\n' "$runtime"
  else
    rm -rf "$runtime"
  fi
}
trap cleanup EXIT INT TERM

chmod 700 "$runtime"
export XDG_RUNTIME_DIR=$runtime
export WAYLAND_DISPLAY=$socket
export WLR_BACKENDS=headless
export WLR_HEADLESS_OUTPUTS=1
export WLR_RENDERER=pixman
export WLR_LIBINPUT_NO_DEVICES=1
export QT_QPA_PLATFORM=wayland
export QT_QUICK_BACKEND=software
export MOKO_COMPOSITOR_EVENTS=$events
export MOKO_LIVE_LAUNCH_EVENTS=$events

"$compositor" --socket "$socket" --debug >"$runtime/compositor.log" 2>&1 &
compositor_pid=$!

attempt=0
while [ ! -S "$runtime/$socket" ]; do
  attempt=$((attempt + 1))
  if [ "$attempt" -ge 100 ] || ! kill -0 "$compositor_pid" 2>/dev/null; then
    cat "$runtime/compositor.log" >&2
    exit 1
  fi
  sleep 0.05
done

"$shell" >"$runtime/shell.log" 2>&1 &
shell_pid=$!
sleep 2
kill -0 "$shell_pid"

"$files" --smoke-test >"$runtime/files.log" 2>&1 &
files_pid=$!
"$settings" --smoke-test >"$runtime/settings.log" 2>&1 &
settings_pid=$!

wait "$files_pid"
files_pid=
wait "$settings_pid"
settings_pid=
sleep 1
kill -0 "$shell_pid"
kill -0 "$compositor_pid"

grep -Eq 'MOKO_COMPOSITOR_SHELL state=mapped app_id=org.moko.Shell width=[1-9][0-9]* height=[1-9][0-9]* fullscreen=1' "$events"
grep -q 'MOKO_COMPOSITOR_WINDOW state=mapped .*app_id=org.moko.Files' "$events"
grep -q 'MOKO_COMPOSITOR_WINDOW state=mapped .*app_id=org.moko.Settings' "$events"
if grep -q 'MOKO_SHELL_SURFACE state=hidden' "$events"; then
  printf 'Shell used the Cage hide workaround under moko-compositor.\n' >&2
  exit 1
fi
if grep -Eiq 'assert|segmentation fault' "$runtime/compositor.log"; then
  cat "$runtime/compositor.log" >&2
  exit 1
fi

printf 'MOKO compositor Qt multi-window session passed.\n'
