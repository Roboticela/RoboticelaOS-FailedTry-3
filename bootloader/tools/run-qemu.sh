#!/usr/bin/env bash
# robobootloader - QEMU runner.
#
# Locates EDK2/OVMF firmware across common locations (override with
# OVMF_CODE / OVMF_VARS env vars, or OVMF_DIR), then boots build/robo disk.img.
#
# Env knobs:
#   OVMF_CODE, OVMF_VARS : explicit firmware paths
#   QEMU                 : qemu binary (default: qemu-system-x86_64)
#   QEMU_EXTRA           : extra args passed through
#   SERIAL_LOG           : file for COM1 output (default: build/serial.log)
#   HEADLESS=1           : run without a window (uses -display none + VNC off)
#   MONITOR_SOCK         : unix socket path for monitor (screendumps)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # bootloader/
DISK="${1:-$HERE/build/robo-disk.img}"
SERIAL_LOG="${SERIAL_LOG:-$HERE/build/serial.log}"
MONITOR_SOCK="${MONITOR_SOCK:-$HERE/build/monitor.sock}"
mkdir -p "$(dirname "$SERIAL_LOG")"

QEMU_BIN="${QEMU:-qemu-system-x86_64}"
command -v "$QEMU_BIN" >/dev/null 2>&1 || QEMU_BIN="/c/Program Files/qemu/qemu-system-x86_64.exe"

find_ovmf() {
    # explicit wins
    if [ -n "${OVMF_CODE:-}" ] && [ -f "${OVMF_CODE:-}" ]; then
        echo "${OVMF_CODE}"; return
    fi
    if [ -n "${OVMF_DIR:-}" ] && [ -f "$OVMF_DIR/edk2-x86_64-code.fd" ]; then
        echo "$OVMF_DIR/edk2-x86_64-code.fd"; return
    fi
    local cands=(
        "/c/Program Files/qemu/share/edk2-x86_64-code.fd"
        "/c/msys64/mingw64/share/qemu/edk2-x86_64-code.fd"
        "/usr/share/OVMF/OVMF_CODE.fd"
        "/usr/share/OVMF/OVMF_CODE_4M.fd"
        "/usr/share/ovmf/OVMF.fd"
        "/usr/share/qemu/OVMF_CODE.fd"
        "/usr/local/share/qemu/OVMF_CODE.fd"
    )
    for c in "${cands[@]}"; do
        [ -f "$c" ] && { echo "$c"; return; }
    done
    echo ""
}

FW="$(find_ovmf || true)"
if [ -z "$FW" ]; then
    echo "ERROR: OVMF/EDK2 firmware not found." >&2
    echo "Set OVMF_CODE=/path/to/OVMF_CODE.fd (and OVMF_VARS=/path/to/OVMF_VARS.fd) and retry." >&2
    exit 1
fi
echo "Using firmware: $FW"

# Vars image: prefer matching -vars file next to code; else a scratch copy.
VARS="${OVMF_VARS:-}"
if [ -z "$VARS" ]; then
    case "$FW" in
        *edk2-x86_64-code.fd)
            for v in "/c/Program Files/qemu/share/edk2-i386-vars.fd" \
                     "/c/msys64/mingw64/share/qemu/edk2-i386-vars.fd" \
                     "$(dirname "$FW")/edk2-x86_64-vars.fd"; do
                [ -f "$v" ] && { VARS="$v"; break; }
            done
            ;;
        *OVMF_CODE_4M.fd) VARS="/usr/share/OVMF/OVMF_VARS_4M.fd" ;;
        *OVMF_CODE.fd)    VARS="/usr/share/OVMF/OVMF_VARS.fd" ;;
        *)                VARS="" ;;
    esac
fi
rm -f "$HERE/build/vars.fd"
if [ -n "$VARS" ] && [ -f "$VARS" ]; then
    cp "$VARS" "$HERE/build/vars.fd"
    VARS_ARGS=(-drive "if=pflash,format=raw,readonly=on,file=$FW" \
               -drive "if=pflash,format=raw,file=$HERE/build/vars.fd")
else
    echo "NOTE: no vars image found; booting without persistent UEFI vars." >&2
    VARS_ARGS=(-bios "$FW")
fi

rm -f "$MONITOR_SOCK"
DISPLAY_ARGS=(-display default)
if [ "${HEADLESS:-0}" = "1" ]; then
    DISPLAY_ARGS=(-display none)
fi

exec "$QEMU_BIN" \
    -m 256M \
    "${VARS_ARGS[@]}" \
    -drive "file=$DISK,format=raw,if=ide" \
    -serial "file:$SERIAL_LOG" \
    -monitor "unix:$MONITOR_SOCK,server,nowait" \
    -no-reboot \
    "${DISPLAY_ARGS[@]}" \
    ${QEMU_EXTRA:-}
