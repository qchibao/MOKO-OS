#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
MONITOR=${MOKO_LIVE_LAUNCH_MONITOR:-$ROOT/image/live-build/config/includes.chroot/usr/local/libexec/moko-live-launch-monitor}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/bin" "$TMP/runtime"
cat > "$TMP/bin/tail" <<'EOF'
#!/bin/sh
for path do :; done
cat "$path"
EOF
chmod 0755 "$TMP/bin/tail"

cat > "$TMP/runtime/events" <<'EOF'
MOKO_PACKAGE_UI state=ready uid=1000
MOKO_PACKAGE state=installing detail=Waiting_for_authorization... uid=1000
MOKO_PACKAGE state=installed detail=Package_installed. uid=1000
MOKO_PACKAGEHACK state=installed uid=0
untrusted output
EOF

output=$(PATH="$TMP/bin:$PATH" \
  MOKO_LIVE_LAUNCH_EVENTS="$TMP/runtime/events" \
  MOKO_SERIAL_LOG_DEVICE="$TMP/no-serial-device" \
  "$MONITOR" 2>"$TMP/stderr")

expected=$(cat <<'EOF'
MOKO_PACKAGE_UI state=ready uid=1000
MOKO_PACKAGE state=installing detail=Waiting_for_authorization... uid=1000
MOKO_PACKAGE state=installed detail=Package_installed. uid=1000
EOF
)

[[ "$output" == "$expected" ]]
[[ ! -s "$TMP/stderr" ]]

echo "Live launch monitor package telemetry regression passed."
