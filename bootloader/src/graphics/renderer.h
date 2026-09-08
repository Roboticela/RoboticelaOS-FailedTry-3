/* robobootloader - boot screen renderer.
 *
 * Implements the visual design from assets/reference.png:
 *   - pure black background
 *   - logo centred horizontally, its centre at ~46.4% of screen height
 *   - progress bar centred, its centre at ~60.9% of screen height,
 *     ~31% of screen width, ~10px tall at 1024-wide reference (scaled),
 *     fill #2DA4FE, track #313335, fully rounded ends
 *   - NO text on the normal boot screen
 *
 * Text exists only for the fatal-error screen and debug builds.
 */
#ifndef ROBO_RENDERER_H
#define ROBO_RENDERER_H

#include "framebuffer.h"

#include <stdint.h>
#include <stdbool.h>

/* Reference-derived layout constants (fractions of screen size). */
#define ROBO_LAYOUT_LOGO_MAX_W       0.24   /* max logo width as fraction of screen width */
#define ROBO_LAYOUT_LOGO_MAX_H       0.14   /* max logo height as fraction of screen height */
#define ROBO_LAYOUT_LOGO_CY          0.464  /* logo centre, fraction of height */
#define ROBO_LAYOUT_BAR_W            0.307  /* bar width, fraction of width */
#define ROBO_LAYOUT_BAR_H            10     /* bar thickness at 1024-wide; scales mildly */
#define ROBO_LAYOUT_BAR_CY           0.609  /* bar centre, fraction of height */

/* Reference colours. */
#define ROBO_COLOR_BAR_FILL_R  0x2D
#define ROBO_COLOR_BAR_FILL_G  0xA4
#define ROBO_COLOR_BAR_FILL_B  0xFE
#define ROBO_COLOR_TRACK_R     0x31
#define ROBO_COLOR_TRACK_G     0x33
#define ROBO_COLOR_TRACK_B     0x35

/* A decoded 32-bit BGRA image (see image/bmp.h). */
typedef struct {
    uint32_t width;
    uint32_t height;
    const uint8_t *pixels;      /* bottom-up-decoded: rows top-down, BGRA */
    uint32_t pitch;             /* bytes per row (width*4) */
} robo_image_t;

/* Draw the full boot frame: black background + centred logo at the reference
 * position. Logo may be NULL (falls back to a clean black screen). */
void render_boot_frame(fb_t *fb, const robo_image_t *logo);

/* Draw/update only the progress bar (0..1000 per-mille). */
void render_progress(fb_t *fb, uint32_t per_mille);

/* Fatal error screen: clean RoboticelaOS visual, no technical logs. */
void render_fatal_error(fb_t *fb, const robo_image_t *logo);

/* Debug-only text overlay (top-left), used exclusively by debug builds. */
void render_debug_text(fb_t *fb, uint32_t x, uint32_t y, const char *text,
                       uint8_t r, uint8_t g, uint8_t b);

/* Scale-to-fit helper: given image and screen dims, output drawn size.
 * Never upscales beyond 2x to avoid blur; never distorts. */
void render_fit_logo(uint32_t img_w, uint32_t img_h, uint32_t scr_w, uint32_t scr_h,
                     uint32_t *out_w, uint32_t *out_h);

#endif /* ROBO_RENDERER_H */
