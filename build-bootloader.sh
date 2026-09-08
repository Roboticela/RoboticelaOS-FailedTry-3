#!/usr/bin/env bash
# RoboticelaOS - build the Stage 01 bootloader from the repository root.
#
# Usage:
#   ./build-bootloader.sh              # default: build BOOTX64.EFI + disk image
#   ./build-bootloader.sh debug        # build with serial diagnostics enabled
#   ./build-bootloader.sh test         # run the host unit tests
#   ./build-bootloader.sh release      # release firmware only
#   ./build-bootloader.sh clean        # remove all build outputs
#   ./build-bootloader.sh <any make target>
#
# Everything is delegated to bootloader/Makefile; this script only locates
# the toolchain and forwards targets.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT/bootloader"

# Locate GNU make: PATH first, then the common MSYS2 location on Windows.
if command -v make >/dev/null 2>&1; then
    MAKE=make
elif [ -x /c/msys64/usr/bin/make.exe ]; then
    MAKE=/c/msys64/usr/bin/make.exe
else
    echo "ERROR: GNU make not found." >&2
    echo "Install it (MSYS2: pacman -S make) or add it to PATH." >&2
    exit 1
fi

TARGETS=("$@")
if [ ${#TARGETS[@]} -eq 0 ]; then
    TARGETS=(image)   # firmware + test kernel + boot config + disk image
fi

exec "$MAKE" "${TARGETS[@]}"
