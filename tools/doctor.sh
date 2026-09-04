#!/usr/bin/env bash
set -u
printf 'MOKO OS build doctor\n\n'
printf 'OS: '; uname -a
printf '\n'
for c in cmake ninja c++ qemu-system-x86_64 lb rsync; do
  if command -v "$c" >/dev/null 2>&1; then printf '[OK]   %-22s %s\n' "$c" "$(command -v "$c")"; else printf '[MISS] %-22s\n' "$c"; fi
done
if command -v cmake >/dev/null 2>&1; then
  if pkg-config --exists Qt6Quick 2>/dev/null; then echo '[OK]   Qt6Quick'; else echo '[MISS] Qt6Quick/pkg-config metadata'; fi
fi
