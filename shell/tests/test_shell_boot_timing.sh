#!/bin/sh
set -eu

shell_bin=${1:?moko-shell path is required}
runtime=$(mktemp -d)
trap 'rm -rf "$runtime"' EXIT INT TERM
events="$runtime/events"

MOKO_LIVE_LAUNCH_EVENTS="$events" \
QT_QPA_PLATFORM=offscreen \
QT_QUICK_BACKEND=software \
    "$shell_bin" --smoke-test --windowed

grep -Eq '^MOKO_BOOT_TIMING stage=shell-ready uptime_ms=[0-9]+ uid=[0-9]+$' "$events"
