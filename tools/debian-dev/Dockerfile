FROM debian:trixie-slim

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cage \
        cmake \
        dbus \
        libgl1-mesa-dri \
        libvterm-dev \
        libxkbcommon-dev \
        ninja-build \
        pkg-config \
        qml6-module-qtquick \
        qml6-module-qtquick-controls \
        qml6-module-qtquick-layouts \
        qt6-base-dev \
        qt6-declarative-dev \
        qt6-declarative-dev-tools \
        qt6-wayland \
        weston \
    && rm -rf /var/lib/apt/lists/*

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

WORKDIR /workspace
