#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/out"
IMAGE=${MOKO_LIVE_IMAGE:-moko-os-debian13-live-builder}
CACHE_VOLUME=${MOKO_LIVE_CACHE_VOLUME:-moko-os-live-build-cache}
ARTIFACT_STEM=MOKO-OS-v0.1.1-dev-amd64
GIT_COMMIT=$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || printf unknown)
SOURCE_DATE_EPOCH=$(git -C "$ROOT" show -s --format=%ct HEAD 2>/dev/null || date +%s)
if [[ -n $(git -C "$ROOT" status --porcelain 2>/dev/null) ]]; then
  SOURCE_STATE=dirty
else
  SOURCE_STATE=clean
fi

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
  --env "MOKO_GIT_COMMIT=$GIT_COMMIT" \
  --env "MOKO_SOURCE_STATE=$SOURCE_STATE" \
  --env "SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH" \
  --env "ARTIFACT_STEM=$ARTIFACT_STEM" \
  "$IMAGE" \
  bash -lc '
    set -euo pipefail
    rsync -a --exclude .git --exclude build --exclude out /source/ /workspace/
    cd /workspace
    ./scripts/build-iso.sh
    for artifact in \
      "$ARTIFACT_STEM.hybrid.iso" \
      "$ARTIFACT_STEM.build-info.txt" \
      "$ARTIFACT_STEM.packages.txt" \
      "$ARTIFACT_STEM.known-issues.txt" \
      "$ARTIFACT_STEM.live-usb-checklist.md" \
      SHA256SUMS
    do
      install -m 0644 "out/$artifact" /artifacts/
    done
  '
