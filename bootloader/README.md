# RoboBootloader — RoboticelaOS Stage 01

The UEFI bootloader for RoboticelaOS. It shows a clean, modern black boot
screen (logo + progress bar), then loads an ELF64 x86-64 kernel, hands it a
versioned `RoboBootInfo` structure, exits UEFI boot services, and jumps to the
kernel entry point.

This component is **fully standalone**: it has its own build system, its own
tests, and its own QEMU preview environment. Nothing else in RoboticelaOS is
required to build, run, or test it.

---

## What it looks like

Pure-black screen, the Roboticela logo sized relative to the screen — its
height is 18% of the screen height (Apple-style, so it keeps the same visual
weight at every resolution), centred horizontally at ~42% of screen height —
and a slim rounded progress bar below it: ~15% of the screen wide,
screen-relative thickness (0.55% of height, clamped 4–16 px), centred at
~57.5% height. Blue fill `#2DA4FE`, dark track `#313335`. The logo and the
bar can never overlap: the logo's bottom edge stops at 51% of the screen
height while the bar starts at ~57%, at every resolution. No boot text,
no logs — exactly like a modern OS boot splash. All diagnostics go to the
serial port in debug mode.

### Animated progress

The bar is driven by a small animation engine (`src/graphics/progress.c`)
with Apple-style behaviour:

- it **never blocks the boot**: the engine only computes values; real work
  (config, kernel read, ELF load) runs untouched
- after any stage jump it **glides smoothly** with an ease-out curve
  (~0.5 s per sweep), so even an instant load looks like calm motion
- the kernel read reserves the 45–60% span and **reports real byte
  progress** chunk-by-chunk (`file_read_all_progress`), so on slow disks the
  bar tracks the actual pace instead of jumping
- the bar **never rewinds** and always completes at 100% with one final
  glide (bounded to ~2.5 s) before the kernel handoff

Fast boot = still a graceful sweep; slow boot = honest tracking. Both look
smooth. The engine is host-tested (`make test`, `tests/progress_test.c`).

## Supported platform

| | |
|---|---|
| Architecture | x86-64 / AMD64 |
| Firmware | UEFI only (PE/COFF `EFI_APPLICATION`) |
| Output | `BOOTX64.EFI` |
| Kernel format | ELF64, x86-64, statically linked (PT_LOAD segments) |
| Logo format | 32-bit uncompressed BMP (bottom-up or top-down) |

BIOS/Legacy boot, ARM, and other architectures are out of scope for Stage 01.

## Prerequisites

- **clang + LLD** (any recent version; produces the UEFI PE binary directly)
- **GNU make**
- **Python 3.8+** (disk-image and asset tooling; no third-party packages needed)
- **QEMU** (system x86-64) with **EDK2/OVMF firmware** for the visual preview

On Windows/MSYS2 the Makefile auto-detects clang at
`/c/CLang/clang+llvm-18.1.8-x86_64-pc-windows-msvc/bin` and QEMU at
`C:\Program Files\qemu`. Override with `CLANG=`, `LLD_LINK=`, `QEMU=` if yours
live elsewhere.

## Build

From the repository root (recommended):

```bash
./build-bootloader.sh             # firmware + test kernel + boot disk image
./build-bootloader.sh test        # run the host unit tests
./build-bootloader.sh debug       # firmware with serial diagnostics enabled
./build-bootloader.sh clean       # remove all build outputs
```

(Windows CMD: `build-bootloader.cmd test` — same targets.)

Or work directly inside `bootloader/`:

```bash
make            # or: make release   ->  build/BOOTX64.EFI
make clean      # remove all build outputs
make debug      # build with serial diagnostics always on (see below)
```

The result is `bootloader/build/BOOTX64.EFI`, a standard UEFI application.

## Test

```bash
make test       # host unit tests for config parser, BMP decoder, ELF loader
```

43 automated host tests cover: valid/invalid/missing/oversized inputs for
`boot.cfg` parsing, BMP decoding (truncation, bad magic, absurd dimensions,
padding, orientation), and the ELF64 loader (bad magic, 32-bit, wrong arch,
no PT_LOAD, overflowing segments, bogus entry point).

## QEMU preview

From the repository root:

```bash
./run-bootloader.sh               # build if needed + open a QEMU window
./run-bootloader.sh --headless    # headless boot; watch build/serial.log
./run-bootloader.sh --screenshot  # headless boot, capture build/screen.png
```

(Windows CMD: `run-bootloader.cmd --screenshot` — same options.)

The screenshot lands in `bootloader/build/screen.png` and shows the boot
splash at its normal resolution.

Or use make inside `bootloader/`:

```bash
make image      # build/robo-disk.img: GPT disk with an ESP containing
                #   EFI/BOOT/BOOTX64.EFI, RoboBoot/{robokernel,logo.bmp,boot.cfg}
make run        # boot the image in QEMU with a visible window
make run-headless
```

The `make run` scripts locate OVMF automatically. Common locations are
probed (`C:\Program Files\qemu\share`, `/usr/share/OVMF`, etc.). If yours is
elsewhere:

```bash
OVMF_CODE=/path/to/OVMF_CODE.fd OVMF_VARS=/path/to/OVMF_VARS.fd make run
```

`boot.cfg` inside the image defaults to `test=1`, which holds the finished
boot screen for ~15 s so you can actually look at it (and screenshot it),
then hands off to the test kernel.

### Capturing the boot screen

With the root script:

```bash
./run-bootloader.sh --screenshot   # -> bootloader/build/screen.png
```

Or manually from a `run-headless` QEMU (monitor is a TCP socket on Windows,
unix socket elsewhere):

```bash
bash tools/screendump.sh build/screen.png          # default monitor address
bash tools/screendump.sh build/screen.png 127.0.0.1:4444   # explicit TCP
```

## Logo replacement

The logo's vector master is an SVG. The build generates the BMP from it
automatically:

```
assets/logo.svg   --(make / generate-logo.py)-->   assets/logo.bmp
        (rasterized 1024px, converted to 32-bit BMP)
```

`logo.bmp` is generated on every build that needs it (embedded fallback logo
and boot disk image) and is git-ignored. Regenerate explicitly with:

```bash
make logo                              # inside bootloader/
./build-bootloader.sh logo             # from the repo root
LOGO_WIDTH=2048 make logo              # higher-res rasterization
python tools/generate-logo.py --svg mylogo.svg --width 512   # one-off
```

To change the logo, replace `bootloader/assets/logo.svg` (or point the
generator at another SVG) and rebuild. The BMP decoder accepts any 32-bit
uncompressed BMP, so a hand-made BMP also works — just place it at
`assets/logo.bmp` and `touch` it so the build picks it up:

```bash
python -c "from PIL import Image; Image.open('mylogo.png').save('bootloader/assets/logo.bmp')"
```

Prerequisites for the SVG step: `pip install cairosvg` (or ImageMagick /
rsvg-convert / Inkscape — the generator picks whichever it finds).

The logo is rendered sized relative to the screen: height = 18% of screen
height (width capped at 30% of screen width), centred at 42% of screen
height, never upscaled, downscaled with a box filter, transparency respected.

Rules the bootloader enforces on any BMP (from disk or embedded):
uncompressed, 32bpp, sane dimensions, `biSizeImage` consistent — anything
else is rejected and the boot continues with the last good logo.

## Boot disk layout

```
ESP (FAT32, GPT)
└── RoboBoot/
    ├── boot.cfg      # configuration (see below)
    ├── logo.bmp      # splash logo
    └── robokernel    # ELF64 kernel image
```

`\EFI\BOOT\BOOTX64.EFI` is the firmware entry.

## Configuration

`RoboBoot/boot.cfg` — `key = value` lines, `#` comments, UTF-8/ASCII:

```
kernel  = RoboBoot/robokernel
logo    = RoboBoot/logo.bmp
initfs  =                  # empty disables the optional initfs
timeout = 3
test    = 1                # hold the boot screen for inspection
debug   = 1                # developer serial output (COM1, 115200 8N1)
```

Unknown keys, malformed values, or `..`/backslash/absolute paths in path
values are rejected — the bootloader shows its error screen rather than
guessing. Missing file = defaults.

## Debug mode

Three independent ways to get developer diagnostics on the COM1 serial port
(QEMU shows it with `-serial file:...` or displays it):

1. `boot.cfg`: `debug = 1`
2. `make debug` (compile-time; serial starts at the very first instruction)
3. Both together for the earliest possible output

Production boot (`debug=0`, `make release`) never touches the serial port or
shows any text on screen.

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| QEMU drops to the UEFI shell | The image lacks a GPT/ESP — use `make image` (not a superfloppy) |
| Screen stays black | In test mode the bar finishes and holds; wait or check serial |
| `FATAL: invalid kernel` | `robokernel` isn't a valid ELF64 x86-64 image |
| No serial output | Use `make debug` (not just `debug=1` in boot.cfg) or check `-serial` is set |
| Wrong colors | Only 32bpp BMPs are supported; convert the logo |
| OVMF not found | Set `OVMF_CODE`/`OVMF_VARS` explicitly |

## What this component is NOT

No kernel, no filesystem implementation, no drivers, no compositor, no
package manager, no network, no secure-boot signing (only an isolated
extension point), no dependency on any other RoboticelaOS component. The
only output contract is the documented `RoboBootInfo` handoff — see
`ARCHITECTURE.md`.
