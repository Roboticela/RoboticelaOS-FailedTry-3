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
        # QEMU's output goes to a log so it cannot hold the caller's pipe
        # open after we exit (Windows keeps it alive past `kill`).
        HEADLESS=1 bash "$ROOT/bootloader/tools/run-qemu.sh" "$DISK" \
            >"$ROOT/bootloader/build/qemu-stdout.log" 2>&1 &
        QEMU_PID=$!
        # Give OVMF + boot + test-mode hold time to settle, then capture.
        sleep 25
        # Match the runner's monitor (TCP on Windows, unix socket elsewhere).
        case "$(uname -s)" in
            MINGW*|MSYS*|CYGWIN*) SOCK="${MONITOR_TCP:-127.0.0.1:4444}" ;;
            *)                    SOCK="$ROOT/bootloader/build/monitor.sock" ;;
        esac
        bash "$ROOT/bootloader/tools/screendump.sh" \
            "$ROOT/bootloader/build/screen.png" "$SOCK"
        # Stop QEMU. Windows ignores POSIX signals from Git Bash reliably,
        # so fall back to taskkill there.
        kill "$QEMU_PID" 2>/dev/null || true
        case "$(uname -s)" in
            MINGW*|MSYS*|CYGWIN*)
                taskkill //F //IM qemu-system-x86_64.exe >/dev/null 2>&1 || true
                ;;
        esac
        echo
        echo "Boot screen saved to bootloader/build/screen.png"
        ;;
esac
