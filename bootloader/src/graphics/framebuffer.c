/* robobootloader - framebuffer implementation over UEFI GOP. */
#include "framebuffer.h"
#include "debug.h"
#include "lib.h"
#include "memory/memory.h"

static EFI_GRAPHICS_OUTPUT_PROTOCOL *g_gop = NULL;

static uint32_t normalize_format(EFI_GRAPHICS_PIXEL_FORMAT fmt,
                                 const EFI_PIXEL_BITMASK *m,
                                 uint32_t *r_mask, uint32_t *g_mask, uint32_t *b_mask)
{
    *r_mask = *g_mask = *b_mask = 0;
    switch (fmt) {
    case PixelBlueGreenRedReserved8BitPerColor:
        return ROBO_PX_BGRX;
    case PixelRedGreenBlueReserved8BitPerColor:
        return ROBO_PX_RGBX;
    case PixelBitMask:
        if (m) {
            *r_mask = m->RedMask;
            *g_mask = m->GreenMask;
            *b_mask = m->BlueMask;
            /* We only support the byte-aligned 32bpp masks. */
            if (m->RedMask == 0x00FF0000u && m->GreenMask == 0x0000FF00u &&
                m->BlueMask == 0x000000FFu)
                return ROBO_PX_RGBX;
            if (m->RedMask == 0x000000FFu && m->GreenMask == 0x0000FF00u &&
                m->BlueMask == 0x00FF0000u)
                return ROBO_PX_BGRX;
        }
        return ROBO_PX_UNKNOWN;
    default:
        return ROBO_PX_UNKNOWN;
    }
}

EFI_STATUS fb_init(fb_t *fb)
{
    EFI_STATUS s;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    robomemset(fb, 0, sizeof(*fb));

    s = bs()->LocateProtocol(&gop_guid, NULL, (void **)&g_gop);
    if (EFI_ERROR(s) || !g_gop) {
        debugf("LocateProtocol(GOP) failed: 0x%llx (handle=%p)", (uint64_t)s,
               (void *)g_gop);
        return EFI_NOT_FOUND;
    }

    if (!g_gop->Mode || !g_gop->Mode->Info)
        return EFI_DEVICE_ERROR;

    fb->base = g_gop->Mode->FrameBufferBase;
    fb->size = g_gop->Mode->FrameBufferSize;
    fb->width = g_gop->Mode->Info->HorizontalResolution;
    fb->height = g_gop->Mode->Info->VerticalResolution;
    fb->pitch = g_gop->Mode->Info->PixelsPerScanLine * 4; /* only 32bpp supported */
    debugf("GOP mode: %ux%u ppsl=%u fmt=%d fb=0x%llx", fb->width, fb->height,
           (unsigned)g_gop->Mode->Info->PixelsPerScanLine,
           (int)g_gop->Mode->Info->PixelFormat, fb->base);
    fb->format = normalize_format(g_gop->Mode->Info->PixelFormat,
                                  &g_gop->Mode->Info->PixelInformation,
                                  &fb->r_mask, &fb->g_mask, &fb->b_mask);
    fb->usable = (fb->format == ROBO_PX_BGRX || fb->format == ROBO_PX_RGBX);

    if (!fb->usable) {
        debugf("GOP pixel format unsupported (fmt=%d), screen output disabled",
               (int)g_gop->Mode->Info->PixelFormat);
        return EFI_UNSUPPORTED;
    }

    /* Basic sanity: pitch must hold at least one row and base must be sane. */
    if (fb->pitch < fb->width * 4u || fb->size < (uint64_t)fb->pitch * fb->height) {
        debugf("GOP framebuffer geometry inconsistent: %ux%u pitch=%u size=0x%llx",
               fb->width, fb->height, fb->pitch, fb->size);
        fb->usable = false;
        return EFI_DEVICE_ERROR;
    }

    debugf("GOP: %ux%u pitch=%u fmt=%s fb=0x%llx size=0x%llx",
           fb->width, fb->height, fb->pitch,
           fb->format == ROBO_PX_BGRX ? "BGRX" : "RGBX",
           fb->base, fb->size);
    return EFI_SUCCESS;
}

uint32_t fb_bytes_per_pixel(void)
{
    return 4;
}

/* Compose an opaque 32-bit pixel value in the framebuffer's native layout. */
static inline uint32_t compose_px(const fb_t *fb, uint8_t r, uint8_t g, uint8_t b)
{
    if (fb->format == ROBO_PX_BGRX)
        return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
    return (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r;
}

void fb_fill_rect(fb_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                  uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t y0, y1, row, x0, x1, col;
    uint32_t px = compose_px(fb, r, g, b);

    if (!fb->usable)
        return;

    /* Clip against framebuffer; careful with unsigned underflow. */
    x0 = x;
    y0 = y;
    x1 = x + w; /* may wrap if w huge; clip handles it below */
    y1 = y + h;
    if (x1 > fb->width)  x1 = fb->width;   /* also covers wrap */
    if (y1 > fb->height) y1 = fb->height;
    if (x0 >= x1 || y0 >= y1)
        return;

    for (row = y0; row < y1; row++) {
        uint32_t *line = (uint32_t *)(fb->base + (uint64_t)row * fb->pitch);
        for (col = x0; col < x1; col++)
            line[col] = px;
    }
}

void fb_blit_px(fb_t *fb, uint32_t dx, uint32_t dy,
                const uint8_t *src_bgra, bool src_has_alpha,
                uint32_t w, uint32_t h, uint32_t src_pitch_bytes)
{
    uint32_t row, col;

    if (!fb->usable || !src_bgra)
        return;

    for (row = 0; row < h; row++) {
        uint32_t fy = dy + row;
        if (fy >= fb->height)
            break;
        uint32_t *line = (uint32_t *)(fb->base + (uint64_t)fy * fb->pitch);
        const uint8_t *srow = src_bgra + (uint64_t)row * src_pitch_bytes;
        for (col = 0; col < w; col++) {
            uint32_t fx = dx + col;
            if (fx >= fb->width)
                break;
            const uint8_t *s = srow + col * 4;
            uint8_t sb = s[0], sg = s[1], sr = s[2], sa = s[3];
            if (src_has_alpha && sa < 128)
                continue; /* transparent: leave background */
            line[fx] = compose_px(fb, sr, sg, sb);
        }
    }
}

void fb_clear_black(fb_t *fb)
{
    if (!fb->usable)
        return;
    fb_fill_rect(fb, 0, 0, fb->width, fb->height, 0, 0, 0);
}
