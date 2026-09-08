/* robobootloader - boot screen renderer implementation. */
#include "renderer.h"
#include "font/robofont.h"
#include "memory/memory.h"
#include "lib.h"
#include "debug.h"

/* ------------------------------------------------------------- primitives */

/* integer sqrt (no libm in freestanding) */
static uint32_t sqrt_isq(uint32_t v)
{
    uint32_t res = 0, bit = 1u << 30;
    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= res + bit) {
            v -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/* Filled circle used for the rounded bar ends. */
static void draw_circle_fill(fb_t *fb, int32_t cx, int32_t cy, int32_t rad,
                             uint8_t r, uint8_t g, uint8_t b)
{
    int32_t y;
    if (rad <= 0)
        return;
    for (y = -rad; y <= rad; y++) {
        int32_t dx = rad * rad - y * y;
        int32_t half;
        if (dx < 0)
            continue;
        half = (int32_t)sqrt_isq((uint32_t)dx);
        fb_fill_rect(fb, (uint32_t)(cx - half), (uint32_t)(cy + y),
                     (uint32_t)(2 * half + 1), 1, r, g, b);
    }
}

/* ------------------------------------------------------------- logo image */

void render_fit_logo(uint32_t img_w, uint32_t img_h, uint32_t scr_w, uint32_t scr_h,
                     uint32_t *out_w, uint32_t *out_h)
{
    uint64_t max_w = (uint64_t)((double)scr_w * ROBO_LAYOUT_LOGO_MAX_W);
    uint64_t max_h = (uint64_t)((double)scr_h * ROBO_LAYOUT_LOGO_MAX_H);
    uint64_t w = img_w, h = img_h;
    uint64_t scale_num = 1, scale_den = 1; /* drawn = img * num/den */

    if (img_w == 0 || img_h == 0) {
        *out_w = *out_h = 0;
        return;
    }

    /* Fit within (max_w, max_h) preserving aspect ratio. Allow modest
     * downscale only - never upscale (keeps the logo crisp). */
    if (w > max_w) {
        scale_num = max_w;
        scale_den = w;
    }
    if (h * scale_num > max_h * scale_den) {
        scale_num = max_h;
        scale_den = h;
    }
    if (scale_num >= scale_den) {
        /* no downscale needed */
        w = img_w;
        h = img_h;
    } else {
        w = img_w * scale_num / scale_den;
        h = img_h * scale_num / scale_den;
    }
    if (w == 0) w = 1;
    if (h == 0) h = 1;
    *out_w = (uint32_t)w;
    *out_h = (uint32_t)h;
}

/* Nearest-neighbour scaled blit, centred at (cx_frac * screen_w, cy_frac * screen_h).
 * Clipping is handled inside fb_blit_px. */
static void draw_logo(fb_t *fb, const robo_image_t *img)
{
    uint32_t dw, dh, dx, dy;
    if (!img || !img->pixels || img->width == 0 || img->height == 0)
        return;

    render_fit_logo(img->width, img->height, fb->width, fb->height, &dw, &dh);

    if (dw == img->width && dh == img->height) {
        /* 1:1 fast path */
        dx = (fb->width >= dw) ? (fb->width - dw) / 2 : 0;
        dy = (uint32_t)((double)fb->height * ROBO_LAYOUT_LOGO_CY) - dh / 2;
        fb_blit_px(fb, dx, dy, img->pixels, true, img->width, img->height, img->pitch);
        return;
    }

    /* Nearest-neighbour resample (downscale only, keeps logo crisp enough) */
    {
        uint64_t x_ratio = ((uint64_t)img->width << 16) / dw;
        uint64_t y_ratio = ((uint64_t)img->height << 16) / dh;
        uint8_t *dst = mem_alloc((uint64_t)dw * dh * 4);
        if (!dst) {
            /* fall back to 1:1 if we cannot allocate a scaled buffer */
            dx = (fb->width >= img->width) ? (fb->width - img->width) / 2 : 0;
            dy = (uint32_t)((double)fb->height * ROBO_LAYOUT_LOGO_CY) - img->height / 2;
            fb_blit_px(fb, dx, dy, img->pixels, true, img->width, img->height, img->pitch);
            return;
        }
        for (uint32_t y = 0; y < dh; y++) {
            uint32_t sy = (uint32_t)(((uint64_t)y * y_ratio) >> 16);
            const uint8_t *srow = img->pixels + (uint64_t)sy * img->pitch;
            uint8_t *drow = dst + (uint64_t)y * dw * 4;
            for (uint32_t x = 0; x < dw; x++) {
                uint32_t sx = (uint32_t)(((uint64_t)x * x_ratio) >> 16);
                const uint8_t *s = srow + (uint64_t)sx * 4;
                uint8_t *d = drow + (uint64_t)x * 4;
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
            }
        }
        dx = (fb->width >= dw) ? (fb->width - dw) / 2 : 0;
        dy = (uint32_t)((double)fb->height * ROBO_LAYOUT_LOGO_CY) - dh / 2;
        fb_blit_px(fb, dx, dy, dst, true, dw, dh, dw * 4);
        mem_free(dst);
    }
}

/* ------------------------------------------------------------- progress bar */

void render_progress(fb_t *fb, uint32_t per_mille)
{
    uint32_t bar_w = (uint32_t)((double)fb->width * ROBO_LAYOUT_BAR_W);
    uint32_t bar_h = ROBO_LAYOUT_BAR_H;
    uint32_t bar_x, bar_y;
    uint32_t fill_w;
    uint32_t rad;

    if (bar_w < 8)
        bar_w = 8;
    if (bar_h < 4)
        bar_h = 4;

    bar_x = (fb->width >= bar_w) ? (fb->width - bar_w) / 2 : 0;
    bar_y = (uint32_t)((double)fb->height * ROBO_LAYOUT_BAR_CY) - bar_h / 2;

    if (per_mille > 1000)
        per_mille = 1000;

    /* Track */
    fb_fill_rect(fb, bar_x + bar_h / 2, bar_y, bar_w - bar_h, bar_h,
                 ROBO_COLOR_TRACK_R, ROBO_COLOR_TRACK_G, ROBO_COLOR_TRACK_B);
    rad = bar_h / 2;
    draw_circle_fill(fb, (int32_t)(bar_x + rad), (int32_t)(bar_y + rad), (int32_t)rad,
                     ROBO_COLOR_TRACK_R, ROBO_COLOR_TRACK_G, ROBO_COLOR_TRACK_B);
    draw_circle_fill(fb, (int32_t)(bar_x + bar_w - rad), (int32_t)(bar_y + rad), (int32_t)rad,
                     ROBO_COLOR_TRACK_R, ROBO_COLOR_TRACK_G, ROBO_COLOR_TRACK_B);

    /* Fill */
    fill_w = (uint32_t)(((uint64_t)bar_w * per_mille) / 1000u);
    if (fill_w > 0) {
        uint32_t fw = fill_w;
        uint32_t frad = (fw < bar_h) ? fw / 2 : rad;
        if (fw >= bar_h) {
            fb_fill_rect(fb, bar_x + rad, bar_y, fw - bar_h, bar_h,
                         ROBO_COLOR_BAR_FILL_R, ROBO_COLOR_BAR_FILL_G, ROBO_COLOR_BAR_FILL_B);
            draw_circle_fill(fb, (int32_t)(bar_x + rad), (int32_t)(bar_y + rad), (int32_t)rad,
                             ROBO_COLOR_BAR_FILL_R, ROBO_COLOR_BAR_FILL_G, ROBO_COLOR_BAR_FILL_B);
            draw_circle_fill(fb, (int32_t)(bar_x + fw - rad), (int32_t)(bar_y + rad), (int32_t)rad,
                             ROBO_COLOR_BAR_FILL_R, ROBO_COLOR_BAR_FILL_G, ROBO_COLOR_BAR_FILL_B);
        } else {
            draw_circle_fill(fb, (int32_t)(bar_x + fw / 2), (int32_t)(bar_y + rad),
                             (int32_t)frad,
                             ROBO_COLOR_BAR_FILL_R, ROBO_COLOR_BAR_FILL_G, ROBO_COLOR_BAR_FILL_B);
        }
    }
}

/* ---------------------------------------------------------------- frames */

void render_boot_frame(fb_t *fb, const robo_image_t *logo)
{
    fb_clear_black(fb);
    if (logo)
        draw_logo(fb, logo);
    render_progress(fb, 0);
}

/* ------------------------------------------------------------------- text */

static void draw_char(fb_t *fb, uint32_t x, uint32_t y, char ch,
                      uint8_t r, uint8_t g, uint8_t b)
{
    unsigned idx;
    const uint8_t *bits;
    uint32_t gw, yoff, i;

    if (ch < ROBOFONT_FIRST || ch >= ROBOFONT_FIRST + ROBOFONT_COUNT)
        return;
    idx = (unsigned)(ch - ROBOFONT_FIRST);
    gw = robofont_width[idx];
    if (gw == 0)
        return;
    bits = &robofont_bits[robofont_offset[idx]];
    yoff = robofont_yoff[idx];
    for (uint32_t row = 0; row < ROBOFONT_HEIGHT; row++) {
        uint8_t line = bits[row]; /* one byte per row, MSB-left */
        for (uint32_t col = 0; col < gw; col++) {
            if (line & (0x80u >> col))
                fb_fill_rect(fb, x + col, y + yoff + row, 1, 1, r, g, b);
        }
    }
}

void render_debug_text(fb_t *fb, uint32_t x, uint32_t y, const char *text,
                       uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t cx = x;
    while (*text) {
        unsigned idx;
        uint8_t adv;
        if (*text == '\n') {
            y += ROBOFONT_LINE;
            cx = x;
            text++;
            continue;
        }
        if (*text < ROBOFONT_FIRST || *text >= ROBOFONT_FIRST + ROBOFONT_COUNT) {
            text++;
            continue;
        }
        idx = (unsigned)(*text - ROBOFONT_FIRST);
        draw_char(fb, cx, y, *text, r, g, b);
        adv = robofont_advance[idx];
        cx += adv;
        text++;
    }
}

/* ------------------------------------------------------------ error screen */

void render_fatal_error(fb_t *fb, const robo_image_t *logo)
{
    static const char *l1 = "Unable to start RoboticelaOS";
    static const char *l2 = "Please restart your computer";

    fb_clear_black(fb);
    if (logo)
        draw_logo(fb, logo);

    {
        uint32_t cw = 0, x, y;
        /* centre two lines of text under the logo */
        for (const char *p = l1; *p; p++) {
            unsigned i = (unsigned)(*p - ROBOFONT_FIRST);
            if (*p >= ROBOFONT_FIRST && i < ROBOFONT_COUNT)
                cw += robofont_advance[i];
        }
        x = (fb->width > cw) ? (fb->width - cw) / 2 : 0;
        y = (uint32_t)((double)fb->height * 0.68);
        render_debug_text(fb, x, y, l1, 0xE8, 0xE8, 0xE8);

        cw = 0;
        for (const char *p = l2; *p; p++) {
            unsigned i = (unsigned)(*p - ROBOFONT_FIRST);
            if (*p >= ROBOFONT_FIRST && i < ROBOFONT_COUNT)
                cw += robofont_advance[i];
        }
        x = (fb->width > cw) ? (fb->width - cw) / 2 : 0;
        y += ROBOFONT_LINE + 4;
        render_debug_text(fb, x, y, l2, 0x9A, 0x9A, 0x9A);
    }
}
