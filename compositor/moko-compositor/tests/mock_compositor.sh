#!/bin/sh
set -eu

state=${MOKO_MOCK_COMPOSITOR_STATE:?}
real_compositor=${MOKO_REAL_COMPOSITOR:?}

[ "${WAYLAND_DISPLAY+x}" != x ]
[ "${WAYLAND_SOCKET+x}" != x ]
[ "${DISPLAY+x}" != x ]

attempt=1
if [ -f "$state" ]; then
  previous=$(cat "$state")
  attempt=$((previous + 1))
fi
printf '%s\n' "$attempt" > "$state"

if [ "${MOKO_FAIL_FIRST_START:-0}" = 1 ] && [ "$attempt" -eq 1 ]; then
  exit 1
fi

exec "$real_compositor" "$@"
