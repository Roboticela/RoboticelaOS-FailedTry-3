#!/usr/bin/env python3
"""Generate the RoboBootloader logo (32-bit BMP) from the SVG source.

Pipeline:
  assets/logo.svg  (vector master, keep at the repository root or bootloader/assets)
        |  cairosvg (or ImageMagick fallback) rasterizes with transparency
        |  PIL converts to an uncompressed 32-bit BGRA BMP, bottom-up
        v
  bootloader/assets/logo.bmp   -> consumed by the build (disk + embedded fallback)

Usage:
  python tools/generate-logo.py [--svg PATH] [--out PATH] [--width N]

The default rasterization width (1024) keeps the logo crisp on 1080p+
boot screens; the bootloader scales it to the actual GOP mode at runtime.
"""
import argparse
import io
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))          # bootloader/tools
BOOT = os.path.dirname(HERE)                               # bootloader/
DEFAULT_SVG = os.path.join(BOOT, "assets", "logo.svg")
DEFAULT_OUT = os.path.join(BOOT, "assets", "logo.bmp")
DEFAULT_WIDTH = 1024


def find_svg():
    """SVG source: --svg wins, else bootloader/assets/logo.svg, else repo root."""
    if ARGS.svg:
        return ARGS.svg
    for cand in (DEFAULT_SVG, os.path.join(os.path.dirname(BOOT), "assets", "logo.svg")):
        if os.path.isfile(cand):
            return cand
    raise SystemExit("ERROR: logo.svg not found (looked in bootloader/assets/ and <repo>/assets/)")


def rasterize_svg(svg_path, width):
    """Return (png_bytes, tool_name) using cairosvg, else ImageMagick."""
    try:
        import cairosvg
        return cairosvg.svg2png(url=svg_path, output_width=width), "cairosvg"
    except ImportError:
        pass
    for tool in ("magick", "convert", "rsvg-convert", "inkscape"):
        if shutil.which(tool):
            break
    else:
        raise SystemExit(
            "ERROR: no SVG rasterizer found.\n"
            "Install one of: pip install cairosvg  |  ImageMagick  |  rsvg-convert  |  Inkscape"
        )
    if tool == "rsvg-convert":
        out = subprocess.run([tool, "-w", str(width), svg_path],
                             check=True, capture_output=True)
        return out.stdout, tool
    density = max(1, width * 96 // 512)  # SVG viewBox is ~512 units wide
    return subprocess.run([tool, "-background", "none", "-density", str(density),
                           svg_path, "PNG:-"], check=True, capture_output=True).stdout, tool


def png_to_bmp32(png_bytes, out_path):
    """RGBA PNG -> uncompressed 32-bit bottom-up BGRA BMP."""
    from PIL import Image
    im = Image.open(io.BytesIO(png_bytes)).convert("RGBA")
    im.save(out_path, format="BMP")  # PIL writes 32-bit BMPs from RGBA
    return im.size


def main():
    global ARGS
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--svg", help="SVG source (default: bootloader/assets/logo.svg)")
    ap.add_argument("--out", default=DEFAULT_OUT, help="BMP output path")
    ap.add_argument("--width", type=int, default=DEFAULT_WIDTH,
                    help="rasterization width in px (default 1024)")
    ARGS = ap.parse_args()

    svg = find_svg()
    png, tool = rasterize_svg(svg, ARGS.width)
    w, h = png_to_bmp32(png, ARGS.out)
    print("logo: %s --(%s @ %dpx)--> %s (%dx%d 32-bit BMP, %d bytes)"
          % (os.path.relpath(svg), tool, ARGS.width,
             os.path.relpath(ARGS.out), w, h, os.path.getsize(ARGS.out)))


if __name__ == "__main__":
    main()
