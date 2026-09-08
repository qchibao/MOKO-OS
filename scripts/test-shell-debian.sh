#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
IMAGE=${MOKO_DEV_IMAGE:-moko-os-debian13-dev}

command -v docker >/dev/null || {
  echo "Docker is required for the Debian 13 validation container." >&2
  exit 1
}

docker build --platform linux/amd64 -t "$IMAGE" "$ROOT/tools/debian-dev"
docker run --rm --platform linux/amd64 \
  --mount "type=bind,source=$ROOT,target=/workspace,readonly" \
  --mount "type=bind,source=$ROOT/out,target=/artifacts" \
  "$IMAGE" \
  bash -lc '
    set -euo pipefail
    bash tests/test-boot-presentation.sh
    bash tests/test-live-disk-safety.sh
    bash tests/test-live-launch-monitor.sh

    cmake -S compositor/moko-compositor -B /tmp/moko-compositor-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build /tmp/moko-compositor-build --parallel
    ctest --test-dir /tmp/moko-compositor-build --output-on-failure

    cmake -S shell -B /tmp/moko-shell-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build /tmp/moko-shell-build --parallel
    ctest --test-dir /tmp/moko-shell-build --output-on-failure

    cmake -S ai -B /tmp/moko-ai-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build /tmp/moko-ai-build --parallel
    ctest --test-dir /tmp/moko-ai-build --output-on-failure

    cmake -S apps -B /tmp/moko-apps-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build /tmp/moko-apps-build --parallel
    ctest --test-dir /tmp/moko-apps-build --output-on-failure
    apps/browser/tests/test_browser_network.sh \
      /tmp/moko-apps-build/browser/moko-browser \
      /artifacts/moko-browser-network-preview.png

    compositor/moko-compositor/tests/test_qt_session.sh \
      /tmp/moko-compositor-build/moko-compositor \
      /tmp/moko-shell-build/moko-shell \
      /tmp/moko-apps-build/files/moko-files \
      /tmp/moko-apps-build/settings/moko-settings

    export XDG_RUNTIME_DIR=/tmp/moko-runtime
    install -d -m 700 "$XDG_RUNTIME_DIR"
    weston --backend=headless-backend.so --renderer=pixman --socket=wayland-moko --idle-time=0 >/tmp/weston.log 2>&1 &
    weston_pid=$!
    trap '\''kill "$weston_pid" 2>/dev/null || true'\'' EXIT

    for _ in $(seq 1 50); do
      [[ -S "$XDG_RUNTIME_DIR/wayland-moko" ]] && break
      sleep 0.1
    done
    [[ -S "$XDG_RUNTIME_DIR/wayland-moko" ]] || { cat /tmp/weston.log; exit 1; }

    WAYLAND_DISPLAY=wayland-moko QT_QPA_PLATFORM=wayland \
      /tmp/moko-shell-build/moko-shell --smoke-test --windowed --size 1280x720 \
      --screenshot /tmp/moko-shell-1280x720.png
    test -s /tmp/moko-shell-1280x720.png

    WAYLAND_DISPLAY=wayland-moko QT_QPA_PLATFORM=wayland \
      /tmp/moko-shell-build/moko-shell --smoke-test --windowed --shutdown-preview \
      --size 1280x720 --screenshot /artifacts/moko-shell-shutdown-preview.png
    python3 - <<'PY'
from PIL import Image

image = Image.open("/artifacts/moko-shell-shutdown-preview.png").convert("RGB")
if any(max(pixel) > 8 for pixel in image.getdata()):
    raise SystemExit("MOKO shutdown render is not fully black")
PY

    WAYLAND_DISPLAY=wayland-moko QT_QPA_PLATFORM=wayland \
      /tmp/moko-shell-build/moko-shell --smoke-test --windowed --control-center \
      --size 1280x720 --screenshot /artifacts/moko-control-center-preview.png
    test -s /artifacts/moko-control-center-preview.png

    WAYLAND_DISPLAY=wayland-moko QT_QPA_PLATFORM=wayland \
      /tmp/moko-shell-build/moko-shell --smoke-test --windowed --control-center \
      --control-center-page 3 --size 1280x720 \
      --screenshot /artifacts/moko-control-center-power-preview.png
    test -s /artifacts/moko-control-center-power-preview.png

    WAYLAND_DISPLAY=wayland-moko QT_QPA_PLATFORM=wayland \
      /tmp/moko-shell-build/moko-shell --smoke-test --windowed --notification-center \
      --size 1280x720 --screenshot /artifacts/moko-notification-center-preview.png
    test -s /artifacts/moko-notification-center-preview.png

    for app in files settings terminal package-installer; do
      WAYLAND_DISPLAY=wayland-moko QT_QPA_PLATFORM=wayland \
        "/tmp/moko-apps-build/$app/moko-$app" \
        --screenshot "/artifacts/moko-$app-preview.png"
      test -s "/artifacts/moko-$app-preview.png"
    done
    WAYLAND_DISPLAY=wayland-moko QT_QPA_PLATFORM=wayland \
      /tmp/moko-apps-build/hardware/moko-hardware-diagnostics \
      --screenshot /artifacts/moko-hardware-diagnostics-preview.png \
      --export-directory /artifacts
    test -s /artifacts/moko-hardware-diagnostics-preview.png
  '
