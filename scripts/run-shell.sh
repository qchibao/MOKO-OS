#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/build/shell/moko-shell"
if [[ ! -x "$BIN" ]]; then
  "$ROOT/scripts/build-shell.sh"
fi
export QT_QUICK_CONTROLS_STYLE=Basic
exec "$BIN"
