# Building MOKO OS v0.1

## Supported build host for the first milestone
Debian 13 amd64 is the reference host. A Debian 13 VM is recommended when developing from macOS.

## Shell dependencies
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts
```

Build:
```bash
./scripts/build-shell.sh
```

## ISO dependencies
```bash
sudo apt install -y live-build debootstrap squashfs-tools xorriso rsync
```

Build:
```bash
sudo ./scripts/build-iso.sh
```

The wrapper copies only the source/assets required by the live image into the live-build chroot include tree, then lets a chroot hook compile/install `moko-shell`.

## macOS Intel
Do not try to use Debian `live-build` directly against macOS as the reference workflow. Build the ISO inside Debian 13 (VM, dedicated Linux machine, or suitable Linux CI runner), then test the resulting ISO with QEMU on macOS using HVF acceleration.
