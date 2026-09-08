FROM debian:trixie-slim

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ovmf \
        qemu-system-x86 \
        python3-minimal \
        python3-pil \
        squashfs-tools \
        socat \
        xorriso \
    && rm -rf /var/lib/apt/lists/*

COPY moko-qemu /usr/local/bin/moko-qemu
RUN chmod 0755 /usr/local/bin/moko-qemu

WORKDIR /workspace
