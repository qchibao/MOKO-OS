#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 first.iso second.iso" >&2
  exit 2
fi

FIRST=$1
SECOND=$2
for iso in "$FIRST" "$SECOND"; do
  [[ -f "$iso" ]] || {
    echo "ISO not found: $iso" >&2
    exit 1
  }
done

sha256_file() {
  if command -v sha256sum >/dev/null; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

FIRST_SHA=$(sha256_file "$FIRST")
SECOND_SHA=$(sha256_file "$SECOND")

if ! cmp -s "$FIRST" "$SECOND"; then
  echo "ISO reproducibility check failed." >&2
  echo "First:  $FIRST_SHA  $FIRST" >&2
  echo "Second: $SECOND_SHA  $SECOND" >&2
  exit 1
fi

echo "ISO reproducibility check passed: $FIRST_SHA"
