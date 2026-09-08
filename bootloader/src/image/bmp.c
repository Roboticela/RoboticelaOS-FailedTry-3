/* robobootloader - hardened 32bpp BMP decoder implementation. */
#include "bmp.h"
#include "lib.h"

static void *(*bmp_alloc)(uint64_t) = NULL;
static void (*bmp_release)(void *) = NULL;

void bmp_set_allocator(void *(*alloc_fn)(uint64_t), void (*free_fn)(void *))
{
    bmp_alloc = alloc_fn;
    bmp_release = free_fn;
}

void bmp_free_pixels(uint8_t *p)
{
    if (bmp_release)
        bmp_release(p);
}

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline int32_t rds32(const uint8_t *p) { return (int32_t)rd32(p); }

bmp_result_t bmp_decode(const uint8_t *data, uint64_t size,
                        uint8_t **out_pixels, uint32_t *out_w, uint32_t *out_h)
{
    uint32_t data_offset, header_size, width, height_abs, stride, padding;
    int32_t height_signed;
    bool top_down;
    uint64_t row, src_row_idx, stride64;
    uint8_t *out;

    if (!out_pixels || !out_w || !out_h)
        return BMP_ERR_TOO_SMALL;
    *out_pixels = NULL;
    *out_w = *out_h = 0;
    if (!data)
        return BMP_ERR_TOO_SMALL;

    /* "BM" magic + file header (14) + minimal info header (40):
     * every field we read below lies within the first 54 bytes. */
    if (size < 2 || data[0] != 'B' || data[1] != 'M')
        return BMP_ERR_MAGIC;
    if (size < 14 + 40)
        return BMP_ERR_TOO_SMALL;

    data_offset = rd32(data + 10);
    header_size = rd32(data + 14);

    /* The declared info header must itself fit in the file. */
    if (header_size < 40 || header_size > 124)
        return BMP_ERR_HEADER;
    if ((uint64_t)header_size + 14 > size)
        return BMP_ERR_TOO_SMALL;

    /* Accept the standard info headers (BITMAPINFOHEADER=40 up to
     * BITMAPV5HEADER=124). Reject anything else (incl. OS/2 core header). */

    width = (uint32_t)rds32(data + 18);
    height_signed = rds32(data + 22);
    top_down = height_signed < 0;
    height_abs = top_down ? (uint32_t)(-(int64_t)height_signed) : (uint32_t)height_signed;

    if (width == 0 || height_abs == 0 || width > BMP_MAX_DIMENSION ||
        height_abs > BMP_MAX_DIMENSION)
        return BMP_ERR_DIMENSIONS;

    {
        uint16_t bpp = rd16(data + 28);
        uint32_t compression = rd32(data + 30);
        if (bpp != 32 || compression != 0)
            return BMP_ERR_FORMAT;
    }

    /* Overflow-safe stride: 4 bytes per pixel + row padding to 4 bytes
     * (32bpp rows are already 4-aligned, but stay defensive). */
    if (robomul_ovf_u64(width, 4, &stride64))
        return BMP_ERR_DIMENSIONS;
    padding = (4u - ((uint32_t)stride64 & 3u)) & 3u;
    if (roboadd_ovf_u64(stride64, padding, &stride64))
        return BMP_ERR_DIMENSIONS;
    stride = (uint32_t)stride64;

    /* The pixel data must actually be present in the file. */
    {
        uint64_t need;
        if (robomul_ovf_u64(stride, height_abs, &need))
            return BMP_ERR_SIZE;
        if (roboadd_ovf_u64(data_offset, need, &need))
            return BMP_ERR_SIZE;
        if (need > size)
            return BMP_ERR_SIZE;
    }

    out = bmp_alloc((uint64_t)width * height_abs * 4);
    if (!out)
        return BMP_ERR_ALLOC;

    /* Pixel data starts at data_offset; rows may be stored bottom-up
     * (classic BMP) or top-down (negative height). We normalize to top-down. */
    for (row = 0; row < height_abs; row++) {
        uint64_t dst_row = row;
        src_row_idx = top_down ? row : (height_abs - 1 - row);
        {
            const uint8_t *src = data + data_offset + src_row_idx * (uint64_t)stride;
            uint8_t *dst = out + dst_row * (uint64_t)width * 4;
            robomemcpy(dst, src, (uint64_t)width * 4);
        }
    }

    *out_pixels = out;
    *out_w = width;
    *out_h = height_abs;
    (void)padding;
    return BMP_OK;
}
