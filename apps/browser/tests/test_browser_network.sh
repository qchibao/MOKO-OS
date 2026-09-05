#!/usr/bin/env bash
set -euo pipefail

BROWSER=${1:?browser executable required}
ARTIFACT=${2:-}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/home/Downloads" "$TMP/runtime" "$TMP/cache" "$TMP/config" "$TMP/data"
chmod 0700 "$TMP/runtime"
EVENTS="$TMP/events.log"
SCREENSHOT="$TMP/browser-network.png"
touch "$EVENTS"

run_args=(
  env
  HOME="$TMP/home"
  XDG_RUNTIME_DIR="$TMP/runtime"
  XDG_CACHE_HOME="$TMP/cache"
  XDG_CONFIG_HOME="$TMP/config"
  XDG_DATA_HOME="$TMP/data"
  MOKO_LIVE_LAUNCH_EVENTS="$EVENTS"
  QT_QPA_PLATFORM=offscreen
  QT_QUICK_BACKEND=software
  QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu
  "$BROWSER"
  --url https://example.com/
  --validation-download https://deb.debian.org/debian/pool/main/h/hello/hello_2.10-5_amd64.deb
  --screenshot "$SCREENSHOT"
)

if [[ $(id -u) == 0 ]]; then
  chown -R 65534:65534 "$TMP"
  timeout 130 runuser -u nobody -- "${run_args[@]}"
else
  timeout 130 "${run_args[@]}"
fi

grep -Fq "MOKO_BROWSER_READY sandbox=enabled web_security=enabled" "$EVENTS"
grep -Fq "MOKO_BROWSER_PAGE state=loaded scheme=https host=example.com" "$EVENTS"
grep -Fq "MOKO_BROWSER_JAVASCRIPT state=pass scheme=https host=example.com" "$EVENTS"
grep -Eq "MOKO_BROWSER_DOWNLOAD state=completed file=hello_2.10-5_amd64.deb bytes=[1-9][0-9]+" "$EVENTS"
test -s "$TMP/home/Downloads/hello_2.10-5_amd64.deb"
test -s "$SCREENSHOT"

if [[ -n "$ARTIFACT" ]]; then
  cp "$SCREENSHOT" "$ARTIFACT"
fi
