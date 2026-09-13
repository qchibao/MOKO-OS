#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PANEL="$ROOT/qml/components/ControlCenterPanel.qml"
MAIN="$ROOT/qml/Main.qml"

grep -Fq 'function refreshState()' "$PANEL"
grep -Fq 'control.refresh()' "$PANEL"
grep -Fq 'onVisibleChanged:' "$PANEL"
grep -Fq 'function onNetworkChanged() { root.queueStateReport() }' "$PANEL"
grep -Fq 'function onBluetoothChanged() { root.queueStateReport() }' "$PANEL"
grep -Fq 'function onAudioChanged() { root.queueStateReport() }' "$PANEL"
grep -Fq 'function onPowerChanged() { root.queueStateReport() }' "$PANEL"
grep -Fq 'root.control.reportControlCenterOpened(root.currentPage)' "$PANEL"

if sed -n '/onControlCenterRequested:/,/^        }/p' "$MAIN" \
    | grep -Fq 'reportControlCenterOpened'; then
  echo "Control Center reports stale state directly from the top-bar click." >&2
  exit 1
fi

echo "MOKO Control Center asynchronous refresh lifecycle passed."
