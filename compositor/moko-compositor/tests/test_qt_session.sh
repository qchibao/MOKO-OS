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
export QSG_RENDER_LOOP=threaded
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

MOKO_FORCE_BOOT_READY_FALLBACK=1 "$shell" --power-menu >"$runtime/shell.log" 2>&1 &
shell_pid=$!

attempt=0
until grep -q '^MOKO_BOOT_TIMING stage=shell-ready ' "$events" 2>/dev/null; do
  attempt=$((attempt + 1))
  if [ "$attempt" -ge 100 ] || ! kill -0 "$shell_pid" 2>/dev/null; then
    cat "$runtime/shell.log" >&2
    printf 'MOKO Shell did not report readiness under moko-compositor.\n' >&2
    exit 1
  fi
  sleep 0.05
done

attempt=0
until grep -q '^MOKO_POWER_MENU state=ready uid=' "$events" 2>/dev/null; do
  attempt=$((attempt + 1))
  if [ "$attempt" -ge 200 ] || ! kill -0 "$shell_pid" 2>/dev/null; then
    cat "$runtime/shell.log" >&2
    cat "$runtime/compositor.log" >&2
    cat "$events" >&2
    printf 'MOKO Power menu did not complete tracked output presentation.\n' >&2
    exit 1
  fi
  sleep 0.05
done

for marker in \
  'MOKO_SHELL_OVERLAY state=rendered' \
  'MOKO_SHELL_OVERLAY state=qt-synchronized' \
  'MOKO_SHELL_OVERLAY state=presentation-requested' \
  'MOKO_SHELL_OVERLAY state=shown' \
  'MOKO_SHELL_OVERLAY state=presented' \
  'MOKO_SHELL_OVERLAY state=acknowledged'
do
  grep -q "^$marker" "$events"
done

rendered_line=$(grep -nFm1 'MOKO_SHELL_OVERLAY state=rendered' "$events" | cut -d: -f1)
synchronized_line=$(grep -nFm1 'MOKO_SHELL_OVERLAY state=qt-synchronized' "$events" | cut -d: -f1)
requested_line=$(grep -nFm1 'MOKO_SHELL_OVERLAY state=presentation-requested' "$events" | cut -d: -f1)
presented_line=$(grep -nFm1 'MOKO_SHELL_OVERLAY state=presented' "$events" | cut -d: -f1)
ready_line=$(grep -nFm1 'MOKO_POWER_MENU state=ready' "$events" | cut -d: -f1)
if [ "$rendered_line" -ge "$synchronized_line" ] \
  || [ "$synchronized_line" -ge "$requested_line" ] \
  || [ "$requested_line" -ge "$presented_line" ] \
  || [ "$presented_line" -ge "$ready_line" ]; then
  cat "$events" >&2
  printf 'MOKO Power menu presentation barriers completed out of order.\n' >&2
  exit 1
fi

if [ "${MOKO_PRINT_TEST_EVENTS:-0}" = 1 ]; then
  cat "$events"
fi

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
