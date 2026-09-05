#!/bin/sh
set -eu

expected_socket=${MOKO_EXPECT_WAYLAND_DISPLAY:?}
events=${MOKO_DESKTOP_SESSION_TEST_EVENTS:?}

[ "${WAYLAND_DISPLAY:-}" = "$expected_socket" ]
[ -S "${XDG_RUNTIME_DIR:?}/$WAYLAND_DISPLAY" ]
printf 'MOKO_DESKTOP_SESSION_TEST state=connected socket=%s\n' "$WAYLAND_DISPLAY" \
  >> "$events"
