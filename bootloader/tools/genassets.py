#!/usr/bin/env python3
"""Generate build-time assets for RoboBootloader (Stage 01).

Inputs
------
assets/reference.png : the visual design reference supplied by the project
                       owner (override with --ref PATH).

Outputs
-------
assets/logo.bmp      : 32-bit BGRA BMP of the logo, cropped from the reference
                       image. Written bottom-up with a 40-byte BITMAPINFOHEADER
                       and no compression -- exactly the subset of BMP that the
                       bootloader's own decoder supports.
src/font/robofont.h  : compact variable-width bitmap font (printable ASCII
src/font/robofont.c    32..126) used ONLY for the fatal-error screen and debug
                       output. The normal boot screen contains no text at all.

The bootloader embeds assets/logo.bmp into BOOTX64.EFI at link time (see the
Makefile) and can also load a replacement logo.bmp from the EFI System
Partition at runtime.

Usage
-----
  python tools/genassets.py [--ref PATH] [--logo PATH]

Only dependency: Pillow (pip install pillow).
"""

import argparse
import os
import struct
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("Pillow is required: pip install pillow")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def find_logo_bbox(im):
    """Bounding box of non-black pixels in the upper 58% of the reference.

    Reference layout: pure black background, logo centred around 46% height,
    progress bar around 61% height. Scanning only above 58% isolates the logo.
    """
    w, h = im.size
    limit = int(h * 0.58)
    px = im.load()
    minx, miny, maxx, maxy = w, h, -1, -1
    for y in range(limit):
        for x in range(w):
            r, g, b = px[x, y][:3]
            if r + g + b > 24:
                if x < minx:
                    minx = x
                if x > maxx:
                    maxx = x
                if y < miny:
                    miny = y
                if y > maxy:
                    maxy = y
    if maxx < 0:
        sys.exit("reference image appears to be entirely black")
    return minx, miny, maxx, maxy


def write_bmp32(path, im):
    """Write a 32-bit uncompressed bottom-up BMP (BITMAPINFOHEADER, 40 bytes).

    This is written by hand (rather than PIL's BMP encoder) so the produced
    file is exactly the BMP subset robobootloader's decoder supports and so it
    doubles as a known-good test fixture.
    """
    w, h = im.size
    rgba = im.convert("RGBA")
    try:
        bgra = rgba.tobytes("raw", "BGRA")  # 4 bytes/px: B, G, R, A
    except ValueError:  # older/newer Pillow without the BGRA packer
        bgra = bytes(
            b for px in rgba.getdata() for b in (px[2], px[1], px[0], 255))
    row = w * 4
    image_size = row * h
    fh = struct.pack("<2sIHHI", b"BM", 14 + 40 + image_size, 0, 0, 14 + 40)
    ih = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 32, 0, image_size, 0, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(fh)
        f.write(ih)
        for y in range(h - 1, -1, -1):  # bottom-up
            f.write(bgra[y * row:(y + 1) * row])
    print("wrote %s (%dx%d, 32-bit BGRA BMP, %d bytes)"
          % (path, w, h, 14 + 40 + image_size))


def gen_logo(args):
    if args.logo:
        im = Image.open(args.logo).convert("RGB")
        bbox = (0, 0) + im.size
        src = "logo file %s" % args.logo
    else:
        ref = Image.open(args.ref).convert("RGB")
        bbox = find_logo_bbox(ref)
        margin = 2
        bbox = (max(0, bbox[0] - margin), max(0, bbox[1] - margin),
                min(ref.size[0], bbox[2] + 1 + margin),
                min(ref.size[1], bbox[3] + 1 + margin))
        im = ref
        src = "reference %s" % args.ref
    im = im.crop(bbox)
    out = os.path.join(ROOT, "assets", "logo.bmp")
    write_bmp32(out, im)
    print("  logo source: %s, crop bbox=%s" % (src, bbox))
    return bbox


def gen_font():
    """Emit src/font/robofont.{h,c} from Pillow's built-in bitmap font."""
    font = ImageFont.load_default()
    ascent, descent = font.getmetrics()
    chars = [chr(c) for c in range(32, 127)]
    width, yoff, advance, offset = [], [], [], []
    bits = bytearray()
    for ch in chars:
        bbox = font.getbbox(ch)
        if bbox is None or bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
            width.append(0)
            yoff.append(0)
            advance.append(max(1, int(round(font.getlength(ch)))))
            offset.append(len(bits))
            continue
        gw, gh = bbox[2] - bbox[0], bbox[3] - bbox[1]
        img = Image.new("1", (gw, gh), 0)
        d = ImageDraw.Draw(img)
        d.text((-bbox[0], -bbox[1]), ch, font=font, fill=1)
        data = img.tobytes()  # rows top-down, each row padded to a byte, MSB first
        width.append(gw)
        yoff.append(max(0, bbox[1]))
        advance.append(max(gw + 1, int(round(font.getlength(ch)))))
        offset.append(len(bits))
        bits.extend(data)

    count = len(chars)
    h_path = os.path.join(ROOT, "src", "font", "robofont.h")
    c_path = os.path.join(ROOT, "src", "font", "robofont.c")
    os.makedirs(os.path.dirname(h_path), exist_ok=True)

    with open(h_path, "w") as f:
        f.write("/* Auto-generated by tools/genassets.py -- DO NOT EDIT.\n"
                " *\n"
                " * Variable-width bitmap font covering printable ASCII 32..126,\n"
                " * used only for the fatal-error screen and debug output.\n"
                " * Each glyph's rows are packed MSB-first, top-down, each row\n"
                " * padded to a whole byte.\n"
                " */\n"
                "#ifndef ROBO_ROBOFONT_H\n"
                "#define ROBO_ROBOFONT_H\n\n"
                "#include <stdint.h>\n\n"
                "#define ROBOFONT_FIRST   %d\n"
                "#define ROBOFONT_COUNT   %d\n"
                "#define ROBOFONT_ASCENT  %d\n"
                "#define ROBOFONT_DESCENT %d\n"
                "#define ROBOFONT_HEIGHT  %d\n"
                "#define ROBOFONT_LINE    (ROBOFONT_ASCENT + ROBOFONT_DESCENT)\n\n"
                "extern const uint8_t  robofont_width[ROBOFONT_COUNT];\n"
                "extern const uint8_t  robofont_yoff[ROBOFONT_COUNT];\n"
                "extern const uint8_t  robofont_advance[ROBOFONT_COUNT];\n"
                "extern const uint32_t robofont_offset[ROBOFONT_COUNT];\n"
                "extern const uint8_t  robofont_bits[%d];\n\n"
                "#endif /* ROBO_ROBOFONT_H */\n"
                % (32, count, ascent, descent, ascent + descent, len(bits)))

    with open(c_path, "w") as f:
        f.write("/* Auto-generated by tools/genassets.py -- DO NOT EDIT. */\n"
                '#include "robofont.h"\n\n')
        for name, vals, fmt in (
                ("robofont_width", width, "%d"),
                ("robofont_yoff", yoff, "%d"),
                ("robofont_advance", advance, "%d")):
            f.write("const uint8_t %s[ROBOFONT_COUNT] = {" % name)
            f.write(",".join(fmt % v for v in vals))
            f.write("};\n\n")
        f.write("const uint32_t robofont_offset[ROBOFONT_COUNT] = {")
        f.write(",".join("%d" % v for v in offset))
        f.write("};\n\n")
        f.write("const uint8_t robofont_bits[%d] = {" % len(bits))
        for i in range(0, len(bits), 16):
            f.write("\n    " + ",".join("0x%02x" % b for b in bits[i:i + 16]) + ",")
        f.write("\n};\n")
    print("wrote %s and %s (%d glyphs, %d font bytes, ascent=%d descent=%d)"
          % (h_path, c_path, count, len(bits), ascent, descent))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", default=os.path.join(ROOT, "assets", "reference.png"))
    ap.add_argument("--logo", default=None,
                    help="use an explicit logo image instead of cropping the reference")
    args = ap.parse_args()
    if not args.logo and not os.path.exists(args.ref):
        sys.exit("reference image not found: %s (pass --ref PATH)" % args.ref)
    gen_logo(args)
    gen_font()


if __name__ == "__main__":
    main()
