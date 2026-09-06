#!/bin/sh
set -eu

compositor=$1
grim=$2
runtime=$(mktemp -d)
socket=wayland-moko-screencopy
compositor_pid=

cleanup() {
  if [ -n "$compositor_pid" ]; then
    kill "$compositor_pid" 2>/dev/null || true
    wait "$compositor_pid" 2>/dev/null || true
  fi
  if [ "${MOKO_KEEP_TEST_OUTPUT:-0}" = 1 ]; then
    printf 'MOKO screencopy test output retained at %s\n' "$runtime"
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

if ! "$grim" "$runtime/screenshot.png" >"$runtime/grim.log" 2>&1; then
  cat "$runtime/grim.log" >&2
  cat "$runtime/compositor.log" >&2
  exit 1
fi
test -s "$runtime/screenshot.png"
printf 'MOKO compositor screencopy integration passed.\n'
