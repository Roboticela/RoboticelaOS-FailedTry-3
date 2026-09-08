#!/usr/bin/env bash
# robobootloader - capture a screenshot from a running QEMU via its monitor.
# Usage: screendump.sh [output.ppm|output.png] [monitor_socket]
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$HERE/build/screen.ppm}"
SOCK="${2:-$HERE/build/monitor.sock}"

command -v python >/dev/null 2>&1 || true

python - "$OUT" "$SOCK" <<'PYEOF'
import socket, sys, time, os

out, sock_path = sys.argv[1], sys.argv[2]
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock_path)
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
s.sendall(b"screendump " + out.encode() + b"\n")
time.sleep(0.8)
read_until_prompt()
s.close()
print("wrote", out)
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
