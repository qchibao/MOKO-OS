#!/usr/bin/env bash
set -euo pipefail

BROWSER=${1:?browser executable required}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

if "$BROWSER" --no-sandbox >/dev/null 2>&1; then
  echo "Browser accepted --no-sandbox." >&2
  exit 1
fi
if "$BROWSER" --disable-web-security >/dev/null 2>&1; then
  echo "Browser accepted --disable-web-security." >&2
  exit 1
fi
if QTWEBENGINE_DISABLE_SANDBOX=1 "$BROWSER" >/dev/null 2>&1; then
  echo "Browser accepted QTWEBENGINE_DISABLE_SANDBOX." >&2
  exit 1
fi
if QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox "$BROWSER" >/dev/null 2>&1; then
  echo "Browser accepted --no-sandbox through QTWEBENGINE_CHROMIUM_FLAGS." >&2
  exit 1
fi
if QTWEBENGINE_CHROMIUM_FLAGS=--disable-web-security "$BROWSER" >/dev/null 2>&1; then
  echo "Browser accepted --disable-web-security through QTWEBENGINE_CHROMIUM_FLAGS." >&2
  exit 1
fi

mkdir -p "$TMP/home" "$TMP/runtime" "$TMP/cache" "$TMP/config" "$TMP/data"
chmod 0700 "$TMP/runtime"

if [[ $(id -u) == 0 ]]; then
  command -v runuser >/dev/null || {
    echo "runuser is required for the non-root Browser smoke test." >&2
    exit 1
  }
  chown -R 65534:65534 "$TMP"
  timeout 25 runuser -u nobody -- env \
    HOME="$TMP/home" XDG_RUNTIME_DIR="$TMP/runtime" \
    XDG_CACHE_HOME="$TMP/cache" XDG_CONFIG_HOME="$TMP/config" \
    XDG_DATA_HOME="$TMP/data" QT_QPA_PLATFORM=offscreen \
    QT_QUICK_BACKEND=software QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    "$BROWSER" --smoke-test --screenshot "$TMP/browser.png"
else
  timeout 25 env \
    HOME="$TMP/home" XDG_RUNTIME_DIR="$TMP/runtime" \
    XDG_CACHE_HOME="$TMP/cache" XDG_CONFIG_HOME="$TMP/config" \
    XDG_DATA_HOME="$TMP/data" QT_QPA_PLATFORM=offscreen \
    QT_QUICK_BACKEND=software QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    "$BROWSER" --smoke-test --screenshot "$TMP/browser.png"
fi

test -s "$TMP/browser.png"
