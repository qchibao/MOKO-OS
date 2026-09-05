#!/bin/sh
set -eu

compositor=$1
client=$2
runtime=$(mktemp -d)
events=$runtime/events.log
log=$runtime/compositor.log
socket=wayland-moko-integration
compositor_pid=

cleanup() {
  if [ -n "$compositor_pid" ]; then
    kill "$compositor_pid" 2>/dev/null || true
    wait "$compositor_pid" 2>/dev/null || true
  fi
  if [ "${MOKO_KEEP_TEST_OUTPUT:-0}" = 1 ]; then
    printf 'MOKO compositor test output retained at %s\n' "$runtime"
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
export MOKO_COMPOSITOR_EVENTS=$events

"$compositor" --socket "$socket" --debug >"$log" 2>&1 &
compositor_pid=$!

attempt=0
while [ ! -S "$runtime/$socket" ]; do
  attempt=$((attempt + 1))
  if [ "$attempt" -ge 100 ] || ! kill -0 "$compositor_pid" 2>/dev/null; then
    cat "$log" >&2
    exit 1
  fi
  sleep 0.05
done

"$client"
grep -q '^MOKO_COMPOSITOR_READY ' "$events"
grep -Eq 'MOKO_COMPOSITOR_SHELL state=mapped app_id=org.moko.Shell width=[1-9][0-9]* height=[1-9][0-9]* fullscreen=1' "$events"
grep -q 'MOKO_COMPOSITOR_WINDOW state=mapped .*app_id=org.moko.TestOne' "$events"
grep -q 'MOKO_COMPOSITOR_WINDOW state=mapped .*app_id=org.moko.TestTwo' "$events"
