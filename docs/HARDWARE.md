# MOKO OS v0.1 hardware target

## First architecture
`x86_64 / amd64`, UEFI-oriented live image.

## CPU target
- Intel Core i3/i5/i7/i9 x86-64
- Intel Core Ultra x86-64 models
- Intel Xeon x86-64
- AMD Ryzen 3/5/7/9
- AMD Threadripper
- AMD EPYC

CPU instruction compatibility is usually not the hard part; graphics, Wi-Fi, audio, Bluetooth, suspend, camera, trackpad and firmware are the main certification work.

## Minimum developer-preview target
- 64-bit x86 CPU, 2 cores
- 4 GiB RAM
- 32 GiB storage if installed
- 1280×720 display
- UEFI or legacy BIOS supported by the live image

## Recommended
- 4+ cores
- 8–16 GiB RAM
- SSD 64 GiB+
- 1920×1080+
- Intel or AMD graphics for early certification

## Intel Mac policy
1. VM/QEMU first.
2. Live USB second.
3. External SSD install third.
4. Internal dual boot only after hardware diagnostics and backups.

T2-equipped Intel Macs require a separate compatibility track. Do not claim support until storage, input, Wi-Fi, audio, suspend and boot have been tested on the exact model.
