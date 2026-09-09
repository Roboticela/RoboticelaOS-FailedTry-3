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

# Windows QEMU builds need Windows-style paths (C:/...), not MSYS /d/... ones.
UNAME_S="$(uname -s)"
case "$UNAME_S" in
    MINGW*|MSYS*|CYGWIN*)
        winpath() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
        ;;
    *)
        winpath() { echo "$1"; }
        ;;
esac

# Optional second disk (boot-picker testing): SECOND_DISK=<path> attaches it
# as a second IDE drive so the firmware exposes two ESPs to enumerate.
SECOND_DISK_ARG=()
if [ -n "${SECOND_DISK:-}" ] && [ -f "$SECOND_DISK" ]; then
    SECOND_DISK_ARG=(-drive "file=$(winpath "$SECOND_DISK"),format=raw,if=ide,index=1")
fi

QEMU_BIN="${QEMU:-qemu-system-x86_64}"
command -v "$QEMU_BIN" >/dev/null 2>&1 || QEMU_BIN="/c/Program Files/qemu/qemu-system-x86_64.exe"

DISK="$(winpath "$DISK")"
SERIAL_LOG="$(winpath "$SERIAL_LOG")"

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
# (Scratch copy path is converted to Windows form for QEMU.)
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
    VARS_ARGS=(-drive "if=pflash,format=raw,readonly=on,file=$(winpath "$FW")" \
               -drive "if=pflash,format=raw,file=$(winpath "$HERE/build/vars.fd")")
else
    echo "NOTE: no vars image found; booting without persistent UEFI vars." >&2
    VARS_ARGS=(-bios "$(winpath "$FW")")
fi

rm -f "$MONITOR_SOCK"
DISPLAY_ARGS=(-display default)
if [ "${HEADLESS:-0}" = "1" ]; then
    DISPLAY_ARGS=(-display none)
fi

# Windows QEMU builds do not support unix-socket monitors (and Python there
# has no AF_UNIX); use a loopback TCP monitor instead. MONITOR_TCP can be
# overridden, e.g. MONITOR_TCP=127.0.0.1:4555.
MONITOR_TCP="${MONITOR_TCP:-127.0.0.1:4444}"
case "$UNAME_S" in
    MINGW*|MSYS*|CYGWIN*)
        MONITOR_ARGS=(-monitor "tcp:$MONITOR_TCP,server,nowait")
        ;;
    *)
        MONITOR_ARGS=(-monitor "unix:$MONITOR_SOCK,server,nowait")
        ;;
esac

exec "$QEMU_BIN" \
    -m 256M \
    "${VARS_ARGS[@]}" \
    -drive "file=$DISK,format=raw,if=ide" \
    "${SECOND_DISK_ARG[@]}" \
    -serial "file:$SERIAL_LOG" \
    "${MONITOR_ARGS[@]}" \
    -no-reboot \
    "${DISPLAY_ARGS[@]}" \
    ${QEMU_EXTRA:-}
