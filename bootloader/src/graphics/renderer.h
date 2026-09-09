/* robobootloader - boot screen renderer.
 *
 * Apple-style splash, exactly like the macOS boot screen:
 *   - pure black background
 *   - logo sized RELATIVE TO THE SCREEN: height = ~18% of screen height
 *     (width capped at 30% of screen width for extreme aspect ratios),
 *     centred horizontally, centre at ~42% of screen height
 *   - progress bar centred, centre at ~57.5% of screen height,
 *     ~15% of screen wide, ~0.55% of screen tall (clamped 4..16 px),
 *     fill #2DA4FE, track #313335, fully rounded ends
 *   - NO text on the normal boot screen
 *
 * No-overlap invariant: the logo bottom can never reach the bar.
 *   logo bottom <= (0.42 + 0.18/2) * H = 0.51 * H
 *   bar top     >= (0.575 * H) - ROBO_LAYOUT_BAR_MAX_H/2
 * so the gap is >= ~6% of screen height at every resolution and aspect.
 *
 * Text exists only for the fatal-error screen and debug builds.
 */
#ifndef ROBO_RENDERER_H
#define ROBO_RENDERER_H

#include "framebuffer.h"

#include <stdint.h>
#include <stdbool.h>

/* Apple-style relative layout constants (fractions of screen size). */
#define ROBO_LAYOUT_LOGO_H          0.18   /* logo height, fraction of screen height */
#define ROBO_LAYOUT_LOGO_MAX_W      0.30   /* safety cap: logo width, fraction of screen width */
#define ROBO_LAYOUT_LOGO_CY         0.42   /* logo centre, fraction of screen height */
#define ROBO_LAYOUT_BAR_W           0.15   /* bar width, fraction of screen width */
#define ROBO_LAYOUT_BAR_H           0.0055 /* bar thickness, fraction of screen height */
#define ROBO_LAYOUT_BAR_MIN_H       4      /* px floor: stays visible on tiny panels */
#define ROBO_LAYOUT_BAR_MAX_H       16     /* px ceiling: stays slim on 4K/8K panels */
#define ROBO_LAYOUT_BAR_CY          0.575  /* bar centre, fraction of screen height */

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

/* Fit-to-screen helper: given image and screen dims, output drawn size.
 * Targets ROBO_LAYOUT_LOGO_H of the screen height (width capped at
 * ROBO_LAYOUT_LOGO_MAX_W of screen width), preserving aspect ratio;
 * never upscales (keeps the logo crisp). */
void render_fit_logo(uint32_t img_w, uint32_t img_h, uint32_t scr_w, uint32_t scr_h,
                     uint32_t *out_w, uint32_t *out_h);

#endif /* ROBO_RENDERER_H */
