/* Host unit tests for the BMP decoder (malloc/free as allocator). */
#include "image/bmp.h"
#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) { printf("  ok  %s\n", msg); }                          \
        else { printf("FAIL  %s (line %d)\n", msg, __LINE__); failures++; } \
    } while (0)

static void *host_alloc(uint64_t n) { return malloc((size_t)n); }
static void host_free(void *p) { free(p); }

static void put32(uint8_t *p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
static void put16(uint8_t *p, uint16_t v) { p[0]=v; p[1]=v>>8; }

/* Build a minimal valid 32bpp bottom-up BMP. */
static uint8_t *make_bmp(uint32_t w, uint32_t h, int top_down, uint64_t *out_size)
{
    uint64_t row = (uint64_t)w * 4;
    uint64_t pxbytes = row * h;
    uint64_t total = 14 + 40 + pxbytes;
    uint8_t *b = calloc(1, (size_t)total);
    b[0]='B'; b[1]='M';
    put32(b+2, (uint32_t)total);
    put32(b+10, 14+40);
    put32(b+14, 40);
    put32(b+18, w);
    put32(b+22, top_down ? (uint32_t)(-(int32_t)h) : h);
    put16(b+26, 1);
    put16(b+28, 32);
    put32(b+30, 0);
    put32(b+34, (uint32_t)pxbytes);
    *out_size = total;
    return b;
}

int main(void)
{
    bmp_set_allocator(host_alloc, host_free);
    printf("bmp tests\n");

    /* valid bottom-up BMP: orientation + pixel order */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(3, 2, 0, &sz);
        /* paint rows: file row0 = bottom row = blue; row1 = top row = red */
        for (uint32_t x = 0; x < 3; x++) {
            b[54 + x*4 + 0] = 0xFF; /* B */
            b[54 + (3+x)*4 + 2] = 0xFF; /* R in second file row */
        }
        uint8_t *px; uint32_t w, h;
        bmp_result_t r = bmp_decode(b, sz, &px, &w, &h);
        CHECK(r == BMP_OK, "valid bottom-up decodes");
        CHECK(w == 3 && h == 2, "dimensions");
        if (r == BMP_OK) {
            /* output row0 should be RED (top), row1 BLUE (bottom) */
            CHECK(px[2] == 0xFF && px[0] == 0x00, "top row is red (flipped)");
            CHECK(px[3*4+0] == 0xFF && px[3*4+2] == 0x00, "second row is blue");
            bmp_free_pixels(px);
        }
        free(b);
    }

    /* valid top-down BMP */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(2, 2, 1, &sz);
        b[54+2] = 0xFF; /* first stored row = top = red */
        uint8_t *px; uint32_t w, h;
        bmp_result_t r = bmp_decode(b, sz, &px, &w, &h);
        CHECK(r == BMP_OK, "top-down decodes");
        if (r == BMP_OK) {
            CHECK(px[2] == 0xFF, "top row preserved");
            bmp_free_pixels(px);
        }
        free(b);
    }

    /* bad magic */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(2, 2, 0, &sz);
        b[0] = 'X';
        uint8_t *px; uint32_t w, h;
        CHECK(bmp_decode(b, sz, &px, &w, &h) == BMP_ERR_MAGIC, "bad magic rejected");
        free(b);
    }

    /* truncated file */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(4, 4, 0, &sz);
        uint8_t *px; uint32_t w, h;
        CHECK(bmp_decode(b, 20, &px, &w, &h) == BMP_ERR_TOO_SMALL, "truncated header rejected");
        CHECK(bmp_decode(b, sz - 5, &px, &w, &h) == BMP_ERR_SIZE, "truncated pixels rejected");
        free(b);
    }

    /* unsupported format: 24bpp */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(2, 2, 0, &sz);
        b[28] = 24;
        uint8_t *px; uint32_t w, h;
        CHECK(bmp_decode(b, sz, &px, &w, &h) == BMP_ERR_FORMAT, "24bpp rejected");
        free(b);
    }

    /* unsupported: RLE compression */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(2, 2, 0, &sz);
        put32(b+30, 1);
        uint8_t *px; uint32_t w, h;
        CHECK(bmp_decode(b, sz, &px, &w, &h) == BMP_ERR_FORMAT, "compressed rejected");
        free(b);
    }

    /* zero dimensions */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(0, 5, 0, &sz);
        uint8_t *px; uint32_t w, h;
        CHECK(bmp_decode(b, sz, &px, &w, &h) == BMP_ERR_DIMENSIONS, "zero width rejected");
        free(b);
    }

    /* oversized dimensions */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(2, 2, 0, &sz);
        put32(b+18, 100000); /* width over limit; file stays tiny -> caught by dims */
        uint8_t *px; uint32_t w, h;
        CHECK(bmp_decode(b, sz, &px, &w, &h) == BMP_ERR_DIMENSIONS, "oversized rejected");
        free(b);
    }

    /* header size lies (declares data beyond file) */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(2, 2, 0, &sz);
        put32(b+34, 0xFFFFFF); /* huge declared image size */
        uint8_t *px; uint32_t w, h;
        /* our decoder ignores biSizeImage for validation and uses dims; still OK */
        bmp_result_t r = bmp_decode(b, sz, &px, &w, &h);
        CHECK(r == BMP_OK || r == BMP_ERR_SIZE, "lying size handled safely");
        if (r == BMP_OK) bmp_free_pixels(px);
        free(b);
    }

    /* data_offset beyond file */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(2, 2, 0, &sz);
        put32(b+10, 0x7FFFFFF0);
        uint8_t *px; uint32_t w, h;
        CHECK(bmp_decode(b, sz, &px, &w, &h) == BMP_ERR_SIZE, "bad offset rejected");
        free(b);
    }

    /* 1x1 tiny image */
    {
        uint64_t sz;
        uint8_t *b = make_bmp(1, 1, 0, &sz);
        uint8_t *px; uint32_t w, h;
        CHECK(bmp_decode(b, sz, &px, &w, &h) == BMP_OK, "1x1 decodes");
        if (px) bmp_free_pixels(px);
        free(b);
    }

    if (failures) { printf("%d FAILURES\n", failures); return 1; }
    printf("all bmp tests passed\n");
    return 0;
}
