#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CHECK=${MOKO_DISK_SAFETY_CHECK:-$ROOT/image/live-build/config/includes.chroot/usr/local/libexec/moko-live-disk-safety-check}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/bin"
printf '%s\n' \
  '#!/bin/sh' \
  'printf "%s\n" "${MOKO_TEST_FINDMNT_OUTPUT:-}"' \
  > "$TMP/bin/findmnt"
chmod 0755 "$TMP/bin/findmnt"

run_check() {
  local mounts=$1
  local serial_device=$2
  local stderr_path=$3

  PATH="$TMP/bin" \
    MOKO_TEST_FINDMNT_OUTPUT="$mounts" \
    MOKO_SERIAL_LOG_DEVICE="$serial_device" \
    "$CHECK" 2>"$stderr_path"
}

safe_mount='/dev/loop0 /run/live/rootfs/filesystem.squashfs'
expected_pass='MOKO_DISK_SAFETY result=pass unexpected_block_mounts=0 automounter=absent'

missing_output=$(run_check "$safe_mount" "$TMP/no-serial-device" "$TMP/missing.stderr")
[[ "$missing_output" == "$expected_pass" ]]
[[ ! -s "$TMP/missing.stderr" ]]

if [[ -c /dev/full && -w /dev/full ]]; then
  failed_write_output=$(run_check "$safe_mount" /dev/full "$TMP/full.stderr")
  [[ "$failed_write_output" == "$expected_pass" ]]
  [[ ! -s "$TMP/full.stderr" ]]

  if ! PATH="$TMP/bin" \
      MOKO_TEST_FINDMNT_OUTPUT="$safe_mount" \
      MOKO_SERIAL_LOG_DEVICE="$TMP/no-serial-device" \
      "$CHECK" >/dev/full 2>"$TMP/stdout-full.stderr"; then
    echo "A failed primary logging write changed a passing audit exit code." >&2
    exit 1
  fi
  [[ ! -s "$TMP/stdout-full.stderr" ]]
else
  echo "Skipping /dev/full write-failure cases; this host does not provide it."
fi

set +e
unsafe_output=$(run_check '/dev/sda1 /mnt/internal' "$TMP/no-serial-device" "$TMP/unsafe.stderr")
unsafe_status=$?
set -e
[[ "$unsafe_status" -eq 1 ]]
[[ "$unsafe_output" == 'MOKO_DISK_SAFETY result=fail unexpected_block_mounts=1 automounter=absent' ]]
[[ ! -s "$TMP/unsafe.stderr" ]]

echo "Live disk safety logging regression tests passed."
