#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/out"
IMAGE=${MOKO_LIVE_IMAGE:-moko-os-debian13-live-builder}
CACHE_VOLUME=${MOKO_LIVE_CACHE_VOLUME:-moko-os-live-build-cache}

command -v docker >/dev/null || {
  echo "Docker is required for the Debian 13 live-image builder." >&2
  exit 1
}

mkdir -p "$OUT"
docker build --platform linux/amd64 -t "$IMAGE" "$ROOT/tools/debian-live"
docker volume create "$CACHE_VOLUME" >/dev/null
docker run --rm --privileged --platform linux/amd64 \
  --mount "type=bind,source=$ROOT,target=/source,readonly" \
  --mount "type=bind,source=$OUT,target=/artifacts" \
  --mount "type=volume,source=$CACHE_VOLUME,target=/workspace/image/live-build/cache" \
  "$IMAGE" \
  bash -lc '
    set -euo pipefail
    rsync -a --exclude .git --exclude build --exclude out /source/ /workspace/
    cd /workspace
    ./scripts/build-iso.sh
    install -m 0644 out/MOKO-OS-v0.1-dev-amd64.hybrid.iso /artifacts/
    install -m 0644 out/SHA256SUMS /artifacts/
  '
