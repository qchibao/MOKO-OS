#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PANEL="$ROOT/qml/components/ControlCenterPanel.qml"
MAIN="$ROOT/qml/Main.qml"
SHELL_BIN=${1:-}

grep -Fq 'function refreshState()' "$PANEL"
grep -Fq 'control.refresh()' "$PANEL"
grep -Fq 'onVisibleChanged:' "$PANEL"
grep -Fq 'function reportState()' "$PANEL"
grep -Fq 'control.reportControlCenterOpened(currentPage)' "$PANEL"
if grep -Fq 'stateReport' "$PANEL"; then
  echo "Control Center state reporting still depends on a QML timer." >&2
  exit 1
fi
grep -Fq 'function onNetworkChanged() { root.reportState() }' "$PANEL"
grep -Fq 'function onBluetoothChanged() { root.reportState() }' "$PANEL"
grep -Fq 'function onAudioChanged() { root.reportState() }' "$PANEL"
grep -Fq 'function onPowerChanged() { root.reportState() }' "$PANEL"

if sed -n '/onControlCenterRequested:/,/^        }/p' "$MAIN" \
    | grep -Fq 'reportControlCenterOpened'; then
  echo "Control Center reports stale state directly from the top-bar click." >&2
  exit 1
fi

if [ -n "$SHELL_BIN" ]; then
  runtime=$(mktemp -d)
  trap 'rm -rf "$runtime"' EXIT INT TERM
  events="$runtime/events"
  MOKO_LIVE_LAUNCH_EVENTS="$events" \
  QT_QPA_PLATFORM=offscreen \
  QT_QUICK_BACKEND=software \
    "$SHELL_BIN" --smoke-test --windowed --control-center
  grep -Eq '^MOKO_CONTROL_CENTER state=open page=0 .* uid=[0-9]+$' "$events"
fi

echo "MOKO Control Center asynchronous refresh lifecycle passed."
