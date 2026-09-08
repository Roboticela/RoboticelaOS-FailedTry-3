#!/usr/bin/env bash
# RoboticelaOS - run the Stage 01 bootloader in QEMU from the repository root.
#
# Usage:
#   ./run-bootloader.sh               # build (if needed) + QEMU window
#   ./run-bootloader.sh --headless    # build + headless QEMU (serial log only)
#   ./run-bootloader.sh --screenshot  # headless boot, then save boot-screen
#                                     # capture to build/screen.png and exit
#   QEMU_EXTRA=... ./run-bootloader.sh   # extra QEMU args passed through
#
# The QEMU/OVMF firmware is located automatically by the runner; override
# with OVMF_CODE=/path/to/OVMF_CODE.fd (and optionally OVMF_VARS=...).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="window"

for arg in "$@"; do
    case "$arg" in
        --headless)   MODE="headless" ;;
        --screenshot) MODE="screenshot" ;;
        *) echo "unknown option: $arg (use --headless or --screenshot)" >&2; exit 1 ;;
    esac
done

# 1) Build the disk image (idempotent; skips nothing - make decides).
"$ROOT/build-bootloader.sh" image

DISK="$ROOT/bootloader/build/robo-disk.img"

# 2) Run it.
case "$MODE" in
    window)
        HEADLESS=0 bash "$ROOT/bootloader/tools/run-qemu.sh" "$DISK"
        ;;
    headless)
        HEADLESS=1 bash "$ROOT/bootloader/tools/run-qemu.sh" "$DISK"
        echo
        echo "Headless QEMU running; serial log: bootloader/build/serial.log"
        echo "Capture the screen with: bash bootloader/tools/screendump.sh build/screen.png"
        echo "Stop QEMU with: taskkill //F //IM qemu-system-x86_64.exe   (Windows)"
        wait || true
        ;;
    screenshot)
        HEADLESS=1 bash "$ROOT/bootloader/tools/run-qemu.sh" "$DISK" &
        QEMU_PID=$!
        # Give OVMF + boot + test-mode hold time to settle, then capture.
        sleep 25
        bash "$ROOT/bootloader/tools/screendump.sh" \
            "$ROOT/bootloader/build/screen.png" \
            "$ROOT/bootloader/build/monitor.sock"
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
        echo
        echo "Boot screen saved to bootloader/build/screen.png"
        ;;
esac
