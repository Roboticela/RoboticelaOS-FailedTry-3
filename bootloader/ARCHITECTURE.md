# RoboBootloader Architecture

```
                    UEFI firmware (OVMF / real hardware)
                              │  efi_main(ImageHandle, SystemTable)
                              ▼
        ┌──────────────────────────────────────────────┐
        │ RoboBootloader                               │
        │                                              │
        │  1. mem_init           system table capture  │
        │  2. fb_init            locate GOP            │
        │  3. config parse       boot.cfg              │
        │  4. logo load/decode   BMP -> BGRA           │
        │  5. kernel locate      file on ESP           │
        │  6. ELF validate/load  PT_LOAD -> pages      │
        │  7. BootInfo build     the contract          │
        │  8. memory map         final GetMemoryMap    │
        │  9. ExitBootServices   with retry on key     │
        │ 10. kernel handoff     entry(BootInfo)       │
        └──────────────────────┬───────────────────────┘
                               │ RoboBootInfo* in RDI
                               ▼
                        ┌───────────────┐
                        │  RoboKernel   │  (future component)
                        └───────────────┘
```

## Module map

| Path | Responsibility |
|---|---|
| `src/main.c` | UEFI entry point; wires allocators into decoders |
| `src/boot.c` | Boot flow orchestration, UI staging, ExitBootServices sequence, kernel handoff |
| `src/uefi/uefi.h/.c` | Minimal hand-written UEFI spec definitions (system table, boot services, GOP, file protocols) + file access helpers |
| `src/graphics/framebuffer.h/.c` | GOP location, mode info, format normalization, clipped pixel/blit primitives |
| `src/graphics/renderer.h/.c` | Boot screen layout: logo placement, progress bar, fatal-error screen |
| `src/image/bmp.h/.c` | Hardened 32-bit BMP decoder (bounds-checked, orientation/padding aware) |
| `src/config/config.h/.c` | `boot.cfg` parser with fail-safe validation |
| `src/elf/elf_loader.h/.c` | ELF64 x86-64 loader: header/PHDR validation, segment loading, BSS zeroing |
| `src/memory/memory.h/.c` | Pool/page allocation wrappers, memory-map acquisition, `memmap_*` helpers |
| `src/bootinfo/bootinfo.h/.c` | The `RoboBootInfo` contract and builders |
| `src/debug.h/.c` | COM1 (16550) developer-only serial channel; never draws on screen |
| `src/lib.h/.c`, `src/crt.c` | Freestanding runtime: mem/str helpers, formatter, `_fltused` |
| `src/font/robofont.c` | Tiny 6x9 bitmap font (error screens only) |
| `build/generated/logo_embed.S` | Build-time embedded copy of `assets/logo.bmp` (fallback logo) |
| `tools/generate-logo.py` | SVG → 32-bit BMP logo generation (`make logo`; runs automatically when the BMP is missing/outdated) |

## Boot flow in detail

1. **Entry** (`efi_main`): capture system table, install allocators.
2. **Framebuffer**: `LocateProtocol(GOP)`; the firmware-chosen mode is used as
   is. Only 32bpp formats (BGRX/RGBX/bitmask-equivalent) are accepted;
   `PixelBltOnly` is rejected and boot falls back to a serial-only path.
3. **Configuration**: `RoboBoot/boot.cfg` parsed with strict validation.
   Missing file is fine (defaults); invalid file is fatal (error screen).
4. **Logo**: BMP decoded from disk into BGRA with alpha. Any decode failure
   falls back to the build-time embedded logo. The image is sized relative to
   the screen (height = 18% of screen height, width capped at 30% of screen
   width), centred horizontally at ~42% of screen height, and clipped to the
   framebuffer. A screen-relative layout guarantees the progress bar below
   (centred at ~57.5% height) can never overlap the logo.
5. **Kernel**: whole file read, ELF64 validated (magic, class, endianness,
   machine, type, program-header bounds, segment ranges/overflow, entry).
   Each `PT_LOAD` segment gets fresh pages allocated at its physical address,
   file bytes copied, `p_memsz > p_filesz` (BSS) zero-filled.
6. **BootInfo**: versioned structure filled with framebuffer info, ACPI RSDP
   (from the EFI configuration table), kernel base/size/entry, optional
   initfs, boot device.
7. **Memory map**: final `GetMemoryMap` (with padded buffer and retry loop on
   `EFI_BUFFER_TOO_SMALL`), then `ExitBootServices(map_key)` with retry if
   the map changed in between.
8. **Handoff**: interrupts cleared, stack abandoned, `RDI = &RoboBootInfo`,
   jump to `kernel_entry`. No boot services are used after this point.

## RoboBootInfo — the bootloader→kernel contract

Defined in `src/bootinfo/bootinfo.h` and the single output of this component.
`version` is `ROBOBOOTINFO_VERSION` (currently 1); kernels must check it.

| Field | Meaning |
|---|---|
| `version` | Contract version |
| `memory_map`, `memory_map_size`, `memory_map_descriptor_size` | Final UEFI memory map (EfiMemoryDescriptor array) valid after ExitBootServices |
| `framebuffer_address/size/width/height/pitch/format` | GOP linear framebuffer; format `ROBO_FB_FORMAT_*` (0=BGRX, 1=RGBX) |
| `acpi_rsdp` | Physical address of ACPI 2.0+ RSDP (0 if absent) |
| `kernel_base`, `kernel_size`, `kernel_entry` | Loaded kernel image and entry point |
| `initfs_address`, `initfs_size` | Optional initfs blob (0/0 if absent) |
| `boot_device` | `ROBO_BOOT_DEVICE_ESP` for stage 01 |
| `command_line` | Reserved (0); future kernel command line |

**Handoff ABI**: SysV x86-64 — first argument register `RDI` holds the
physical address of `RoboBootInfo`. All memory referenced by the structure
is EfiLoaderData/EfiConventional memory recorded in the provided memory map.

## Design invariants

- **No trust of boot files**: every byte of `boot.cfg`, the BMP, and the ELF
  is validated; integer math is overflow-checked; framebuffer writes are
  clipped.
- **Clean visual contract**: the graphical screen shows only logo + progress
  bar (and a clean error screen on failure). Diagnostics exist only on the
  serial debug channel.
- **Freestanding**: no libc, no host OS dependencies; builds to a PE/COFF
  EFI application with clang+lld.
- **Isolated**: no dependency on any other RoboticelaOS component; the test
  kernel in `testkernel/` exists solely to verify the handoff.
- **Future extension point**: kernel signature verification slots in at the
  `elf_load` validation step (see `SECURITY NOTE` in `elf_loader.h`) without
  changing any other module.
