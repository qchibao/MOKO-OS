#!/bin/sh
set -eu

helper=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

if "$helper" >/dev/null 2>&1; then
    echo "helper accepted missing arguments" >&2
    exit 1
fi
if PKEXEC_UID=$(id -u) "$helper" relative.deb \
    0000000000000000000000000000000000000000000000000000000000000000 \
    >/dev/null 2>&1; then
    echo "helper accepted a relative path" >&2
    exit 1
fi
printf 'not a package\n' > "$tmp/sample.deb"
if PKEXEC_UID=$(id -u) "$helper" "$tmp/sample.deb" invalid >/dev/null 2>&1; then
    echo "helper accepted an invalid digest" >&2
    exit 1
fi
ln -s "$tmp/sample.deb" "$tmp/link.deb"
digest=$(sha256sum "$tmp/sample.deb" | cut -d ' ' -f 1)
if PKEXEC_UID=$(id -u) "$helper" "$tmp/link.deb" "$digest" >/dev/null 2>&1; then
    echo "helper accepted a symbolic link" >&2
    exit 1
fi
wrong=0000000000000000000000000000000000000000000000000000000000000000
if PKEXEC_UID=$(id -u) "$helper" "$tmp/sample.deb" "$wrong" >/dev/null 2>&1; then
    echo "helper accepted a changed package" >&2
    exit 1
fi

grep -Fq '/usr/bin/apt-get --assume-yes --no-remove install "$workdir/package.deb"' "$helper"
if grep -Eq '(^|[[:space:]])(eval|sh -c|bash -c)([[:space:]]|$)' "$helper"; then
    echo "helper contains an arbitrary shell execution path" >&2
    exit 1
fi
