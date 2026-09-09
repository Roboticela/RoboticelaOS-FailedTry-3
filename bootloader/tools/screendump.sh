#!/usr/bin/env bash
# robobootloader - capture a screenshot from a running QEMU via its monitor.
# Usage: screendump.sh [output.ppm|output.png] [monitor_socket|host:port]
#
# On Windows, QEMU unix-socket monitors are unavailable; pass a TCP monitor
# address (host:port) or let the default logic pick one matching the runner.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$HERE/build/screen.ppm}"
SOCK="${2:-}"

# Default monitor address: match the runner (TCP on Windows, unix elsewhere).
if [ -z "$SOCK" ]; then
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) SOCK="${MONITOR_TCP:-127.0.0.1:4444}" ;;
        *)                    SOCK="$HERE/build/monitor.sock" ;;
    esac
fi

command -v python >/dev/null 2>&1 || true

# Windows QEMU interprets the screendump path itself: it must be an absolute
# Windows path (D:/...), not a relative or MSYS/Git-Bash one.
OUT="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) OUT="$(cygpath -m "$OUT" 2>/dev/null || echo "$OUT")" ;;
esac

python - "$OUT" "$SOCK" <<'PYEOF'
import socket, sys, time, os

out, addr = sys.argv[1], sys.argv[2]
if ":" in addr and not os.path.exists(addr):
    host, port = addr.rsplit(":", 1)
    s = socket.create_connection((host, int(port)), timeout=5)
else:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(addr)
time.sleep(0.3)
s.settimeout(5)

def read_until_prompt():
    data = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            data += chunk
            if b"(qemu)" in data:
                break
    except socket.timeout:
        pass
    return data

read_until_prompt()
# QEMU's screendump writes PPM regardless of extension; always dump to a
# .ppm path, then convert below when a different extension was requested.
dump_path = out if out.endswith(".ppm") else out[:-4] + ".ppm"
s.sendall(b"screendump " + dump_path.encode() + b"\n")
time.sleep(0.8)
read_until_prompt()
s.close()
print("wrote", dump_path)
PYEOF

# Convert to PNG when requested and Pillow is available.
case "$OUT" in
    *.png)
        python - "$OUT" <<'PYEOF'
import sys
try:
    from PIL import Image
except ImportError:
    sys.exit(0)  # keep PPM only
ppm = sys.argv[1][:-4] + ".ppm"
Image.open(ppm).save(sys.argv[1])
import os
os.remove(ppm)
print("converted to", sys.argv[1])
PYEOF
        ;;
esac
