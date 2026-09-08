/* robobootloader - GOP framebuffer abstraction.
 *
 * Locates the Graphics Output Protocol, keeps the firmware-chosen mode (no
 * mode switching in Stage 01: OVMF defaults are sane), and exposes a tiny,
 * format-aware pixel surface for the renderer. Supports the two 32bpp formats
 * UEFI firmware actually programs (BGRX / RGBX) plus PixelBitMask via the
 * provided masks. BltOnly is rejected early so boot can fall back safely.
 */
#ifndef ROBO_FRAMEBUFFER_H
#define ROBO_FRAMEBUFFER_H

#include "uefi/uefi.h"

#include <stdint.h>
#include <stdbool.h>

/* Our normalized pixel formats (mirrors bootinfo ROBO_FB_FORMAT_*). */
#define ROBO_PX_BGRX     0
#define ROBO_PX_RGBX     1
#define ROBO_PX_BITMASK  2
#define ROBO_PX_UNKNOWN  0xFF

typedef struct {
    uint64_t base;              /* physical == virtual here (identity map) */
    uint64_t size;              /* bytes of the reported framebuffer */
    uint32_t width;
    uint32_t height;
    uint32_t pitch;             /* bytes per scanline */
    uint32_t format;            /* ROBO_PX_* */
    uint32_t r_mask, g_mask, b_mask, r_shift, g_shift, b_shift; /* for BITMASK */
    bool     usable;            /* false = BltOnly/unsupported: refuse to draw */
} fb_t;

/* Locate GOP and fill *fb. Returns EFI_SUCCESS on success. Never writes to
 * the framebuffer itself. */
EFI_STATUS fb_init(fb_t *fb);

/* Byte length of one pixel in the supported formats (always 4). */
uint32_t fb_bytes_per_pixel(void);

/* Render helpers used by renderer.c; all bounds-checked and format-aware. */
void fb_fill_rect(fb_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                  uint8_t r, uint8_t g, uint8_t b);
void fb_blit_px(fb_t *fb, uint32_t dx, uint32_t dy,
                const uint8_t *src_bgra, bool src_has_alpha,
                uint32_t w, uint32_t h, uint32_t src_pitch_bytes);
void fb_clear_black(fb_t *fb);

#endif /* ROBO_FRAMEBUFFER_H */
