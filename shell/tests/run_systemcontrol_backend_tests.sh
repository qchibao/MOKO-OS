#!/bin/sh
set -eu

network_helper=$1
logind_helper=$2
test_binary=$3
export DBUS_SYSTEM_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS

"$network_helper" &
network_pid=$!
"$logind_helper" &
logind_pid=$!
trap 'kill "$network_pid" "$logind_pid" 2>/dev/null || true; wait "$network_pid" "$logind_pid" 2>/dev/null || true' EXIT INT TERM

for _ in $(seq 1 50); do
    if dbus-send --system --dest=org.freedesktop.NetworkManager \
        --type=method_call --print-reply \
        /org/freedesktop/NetworkManager \
        org.freedesktop.DBus.Peer.Ping >/dev/null 2>&1 \
        && dbus-send --system --dest=org.freedesktop.login1 \
        --type=method_call --print-reply \
        /org/freedesktop/login1 \
        org.freedesktop.DBus.Peer.Ping >/dev/null 2>&1; then
        "$test_binary"
        status=$?
        kill "$network_pid" "$logind_pid" 2>/dev/null || true
        wait "$network_pid" "$logind_pid" 2>/dev/null || true
        trap - EXIT INT TERM
        exit "$status"
    fi
    sleep 0.02
done

echo "fake NetworkManager/logind services did not register" >&2
exit 1
