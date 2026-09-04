#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build/shell"
cmake -S "$ROOT/shell" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD" --parallel
printf '\nBuilt: %s\n' "$BUILD/moko-shell"
