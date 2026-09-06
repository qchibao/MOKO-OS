#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LB="$ROOT/image/live-build"
OUT="$ROOT/out"
ARTIFACT_STEM=${ARTIFACT_STEM:-MOKO-OS-v0.1.1-dev-amd64}
ISO_NAME="$ARTIFACT_STEM.hybrid.iso"
GIT_COMMIT=${MOKO_GIT_COMMIT:-}
SOURCE_STATE=${MOKO_SOURCE_STATE:-}
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-}

if [[ -z "$GIT_COMMIT" ]]; then
  GIT_COMMIT=$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || printf unknown)
fi
if [[ ! "$GIT_COMMIT" =~ ^[0-9a-f]{40}$ ]]; then
  GIT_COMMIT=unknown
fi
if [[ -z "$SOURCE_DATE_EPOCH" && "$GIT_COMMIT" != unknown ]]; then
  SOURCE_DATE_EPOCH=$(git -C "$ROOT" show -s --format=%ct "$GIT_COMMIT" 2>/dev/null || true)
fi
if [[ ! "$SOURCE_DATE_EPOCH" =~ ^[1-9][0-9]*$ ]]; then
  SOURCE_DATE_EPOCH=$(date +%s)
fi
export SOURCE_DATE_EPOCH
BUILD_TIMESTAMP=${MOKO_BUILD_TIMESTAMP:-$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y-%m-%dT%H:%M:%SZ)}
if [[ -z "$SOURCE_STATE" ]]; then
  if git -C "$ROOT" status --porcelain 2>/dev/null | grep -q .; then
    SOURCE_STATE=dirty
  else
    SOURCE_STATE=clean
  fi
fi
case "$SOURCE_STATE" in
  clean|dirty) ;;
  *) SOURCE_STATE=unknown ;;
esac

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
RELEASE_STAGING="$LB/config/includes.binary/MOKO"
cleanup() {
  rm -rf "$SOURCE_STAGING" "$RELEASE_STAGING"
}
trap cleanup EXIT
mkdir -p "$SOURCE_STAGING"
rsync -a --delete --exclude build --exclude out --exclude .git \
  "$ROOT/shell" "$ROOT/apps" "$ROOT/ai" "$ROOT/core" "$ROOT/compositor" "$ROOT/assets" \
  "$SOURCE_STAGING/"

mkdir -p "$RELEASE_STAGING"
install -m 0644 "$ROOT/docs/LIVE_USB_CHECKLIST.md" \
  "$RELEASE_STAGING/LIVE_USB_CHECKLIST.md"
install -m 0644 "$ROOT/release/KNOWN-ISSUES.txt" \
  "$RELEASE_STAGING/KNOWN-ISSUES.txt"
printf '%s\n' \
  "Artifact: $ISO_NAME" \
  "Build timestamp (UTC): $BUILD_TIMESTAMP" \
  "Git commit: $GIT_COMMIT" \
  "Source state: $SOURCE_STATE" \
  "Source date epoch: $SOURCE_DATE_EPOCH" \
  "Architecture: amd64 / x86_64" \
  "Session: MOKO Shell on Wayland" \
  "Installer: disabled" \
  > "$RELEASE_STAGING/BUILD-INFO.txt"

lb build

ISO=$(find . -maxdepth 1 -type f \( -name 'live-image-amd64.hybrid.iso' -o -name '*.hybrid.iso' \) | head -n1 || true)
if [[ -z "$ISO" ]]; then
  echo "Build completed but ISO was not found in $LB" >&2
  exit 2
fi
DEST="$OUT/$ISO_NAME"
cp "$ISO" "$DEST"
install -m 0644 "$LB/binary/live/filesystem.packages" \
  "$OUT/$ARTIFACT_STEM.packages.txt"
install -m 0644 "$ROOT/release/KNOWN-ISSUES.txt" \
  "$OUT/$ARTIFACT_STEM.known-issues.txt"
install -m 0644 "$ROOT/docs/LIVE_USB_CHECKLIST.md" \
  "$OUT/$ARTIFACT_STEM.live-usb-checklist.md"
install -m 0644 "$RELEASE_STAGING/BUILD-INFO.txt" \
  "$OUT/$ARTIFACT_STEM.build-info.txt"
(cd "$OUT" && sha256sum "$(basename "$DEST")" > SHA256SUMS)
echo "ISO: $DEST"
cat "$OUT/SHA256SUMS"
