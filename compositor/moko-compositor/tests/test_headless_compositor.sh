#!/bin/sh
set -eu

compositor=$1
client=$2
grim=${3:-}
python=${4:-}
runtime=$(mktemp -d)
events=$runtime/events.log
log=$runtime/compositor.log
socket=wayland-moko-integration
compositor_pid=
client_pid=

cleanup() {
  if [ -n "$client_pid" ]; then
    kill "$client_pid" 2>/dev/null || true
    wait "$client_pid" 2>/dev/null || true
  fi
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

MOKO_SHUTDOWN_TEST_HOLD_MS=3000 "$client" >"$runtime/client.log" 2>&1 &
client_pid=$!

attempt=0
while ! grep -q 'MOKO_COMPOSITOR_SHUTDOWN state=blackout' "$events" 2>/dev/null; do
  attempt=$((attempt + 1))
  if [ "$attempt" -ge 200 ] || ! kill -0 "$client_pid" 2>/dev/null; then
    cat "$runtime/client.log" >&2
    cat "$log" >&2
    exit 1
  fi
  sleep 0.01
done

if [ -n "$grim" ] && [ -n "$python" ]; then
  "$grim" -t ppm "$runtime/shutdown.ppm"
  "$python" - "$runtime/shutdown.ppm" <<'PY'
import sys

with open(sys.argv[1], "rb") as source:
    if source.readline().strip() != b"P6":
        raise SystemExit("Unexpected shutdown capture format")

    tokens = []
    while len(tokens) < 3:
        line = source.readline()
        if not line:
            raise SystemExit("Truncated shutdown capture header")
        line = line.split(b"#", 1)[0]
        tokens.extend(line.split())
    width, height, maximum = map(int, tokens[:3])
    pixels = source.read()

expected = width * height * 3
if maximum != 255 or len(pixels) != expected:
    raise SystemExit("Invalid shutdown capture payload")
bright = sum(component > 8 for component in pixels)
print(f"MOKO headless shutdown frame: bright_components={bright} total={expected}")
if bright:
    raise SystemExit("Shutdown acknowledgement preceded the opaque black frame")
PY
fi

wait "$client_pid"
client_pid=
cat "$runtime/client.log"
grep -q '^MOKO_COMPOSITOR_READY ' "$events"
grep -Eq 'MOKO_COMPOSITOR_SHELL state=mapped app_id=org.moko.Shell width=[1-9][0-9]* height=[1-9][0-9]* fullscreen=1' "$events"
grep -q 'MOKO_DESKTOP_CONFIG action=set-scale value=150 ok=0' "$events"
grep -q 'MOKO_DESKTOP_CONFIG action=set-scale value=200 ok=0' "$events"
grep -q 'MOKO_DESKTOP_CONFIG action=set-keyboard-layout value=1 ok=1' "$events"
grep -q 'MOKO_SHELL_OVERLAY state=shown' "$events"
grep -q 'MOKO_SHELL_OVERLAY state=hidden' "$events"
grep -q 'MOKO_COMPOSITOR_SHUTDOWN state=fading' "$events"
grep -q 'MOKO_COMPOSITOR_SHUTDOWN state=blackout' "$events"
grep -q 'MOKO_COMPOSITOR_WINDOW state=mapped .*app_id=org.moko.TestOne' "$events"
grep -q 'MOKO_COMPOSITOR_WINDOW state=mapped .*app_id=org.moko.TestTwo' "$events"
