FROM debian:trixie-slim

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        qemu-system-x86 \
        squashfs-tools \
        socat \
        xorriso \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
