#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LB="$ROOT/image/live-build"
OUT="$ROOT/out"

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "ISO build needs root for live-build chroots. Run: sudo $0" >&2
  exit 1
fi
for cmd in lb rsync sha256sum; do command -v "$cmd" >/dev/null || { echo "Missing $cmd. Run scripts/bootstrap-debian.sh" >&2; exit 1; }; done

mkdir -p "$OUT"
cd "$LB"
lb clean --all || true
./auto-config.sh

SOURCE_STAGING="$LB/config/includes.chroot/opt/moko-src"
trap 'rm -rf "$SOURCE_STAGING"' EXIT
mkdir -p "$SOURCE_STAGING"
rsync -a --delete --exclude build --exclude out --exclude .git \
  "$ROOT/shell" "$ROOT/apps" "$ROOT/ai" "$ROOT/core" "$ROOT/assets" \
  "$SOURCE_STAGING/"

lb build

ISO=$(find . -maxdepth 1 -type f \( -name 'live-image-amd64.hybrid.iso' -o -name '*.hybrid.iso' \) | head -n1 || true)
if [[ -z "$ISO" ]]; then
  echo "Build completed but ISO was not found in $LB" >&2
  exit 2
fi
DEST="$OUT/MOKO-OS-v0.1-dev-amd64.hybrid.iso"
cp "$ISO" "$DEST"
(cd "$OUT" && sha256sum "$(basename "$DEST")" > SHA256SUMS)
echo "ISO: $DEST"
cat "$OUT/SHA256SUMS"
