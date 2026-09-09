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

# Locate GNU make: PATH first, then common MSYS2 and Linux locations.
if command -v make >/dev/null 2>&1; then
    MAKE=make
elif [ -x /c/msys64/usr/bin/make.exe ]; then
    MAKE=/c/msys64/usr/bin/make.exe
elif [ -x /usr/bin/make ]; then
    MAKE=/usr/bin/make
else
    echo "ERROR: GNU make not found." >&2
    echo "Install it (MSYS2: pacman -S make; Ubuntu/Debian: sudo apt install make) or add it to PATH." >&2
    exit 1
fi

# Ensure common LLVM installs are visible on Linux, mirroring the Windows fallback
# logic used by the bootloader Makefile.
for llvm_dir in \
    /usr/lib/llvm-*/bin \
    /usr/local/opt/llvm/bin \
    /usr/local/bin \
    /usr/bin \
    "$HOME/.local/bin"
 do
    if [ -d "$llvm_dir" ] && { \
        ls "$llvm_dir"/clang* >/dev/null 2>&1 || \
        ls "$llvm_dir"/ld.lld* >/dev/null 2>&1 || \
        ls "$llvm_dir"/lld-link* >/dev/null 2>&1; \
    }; then
        export PATH="$(dirname "$llvm_dir"):$PATH"
        break
    fi
done

TARGETS=("$@")
if [ ${#TARGETS[@]} -eq 0 ]; then
    TARGETS=(image)   # firmware + test kernel + boot config + disk image
fi

if ! command -v clang >/dev/null 2>&1 || ! command -v ld.lld >/dev/null 2>&1; then
    echo "ERROR: clang/ld.lld not found in PATH." >&2
    echo "Install LLVM (Ubuntu/Debian: sudo apt install clang lld llvm) or set CLANG=/path/to/clang and LD=/path/to/ld.lld." >&2
    exit 1
fi

if ! command -v python >/dev/null 2>&1 && ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: Python not found in PATH." >&2
    echo "Install Python 3 (Ubuntu/Debian: sudo apt install python3) or set PYTHON=/path/to/python3." >&2
    exit 1
fi

exec "$MAKE" "${TARGETS[@]}"
