#!/bin/sh
set -eu

helper=$1
test_binary=$2
export DBUS_SYSTEM_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS

"$helper" &
helper_pid=$!
trap 'kill "$helper_pid" 2>/dev/null || true; wait "$helper_pid" 2>/dev/null || true' EXIT INT TERM

for _ in $(seq 1 50); do
    if dbus-send --system --dest=org.freedesktop.NetworkManager \
        --type=method_call --print-reply \
        /org/freedesktop/NetworkManager \
        org.freedesktop.DBus.Peer.Ping >/dev/null 2>&1; then
        "$test_binary"
        status=$?
        kill "$helper_pid" 2>/dev/null || true
        wait "$helper_pid" 2>/dev/null || true
        trap - EXIT INT TERM
        exit "$status"
    fi
    sleep 0.02
done

echo "fake NetworkManager did not register" >&2
exit 1
