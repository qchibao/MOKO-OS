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

if [ "${MOKO_FAIL_FIRST_AFTER_SOCKET:-0}" = 1 ] && [ "$attempt" -eq 1 ]; then
  exec python3 - "${XDG_RUNTIME_DIR:?}/${MOKO_EXPECT_WAYLAND_DISPLAY:?}" <<'PY'
import socket
import sys
import time

server = socket.socket(socket.AF_UNIX)
server.bind(sys.argv[1])
server.listen(1)
time.sleep(0.2)
PY
fi

exec "$real_compositor" "$@"
