/* robobootloader - minimal, hardened BMP decoder (32bpp subset).
 *
 * Stage 01 keeps image handling deliberately tiny. We accept only:
 *   - uncompressed 32-bit BMPs (BITMAPINFOHEADER and friends, 40..124 bytes),
 *   - bottom-up or top-down orientation,
 *   - positive dimensions within sane limits.
 * Output is a top-down BGRA buffer ready for the renderer.
 *
 * Everything is bounds-checked: a malformed file can never cause an
 * out-of-bounds read or an oversized allocation. Failing paths return a
 * specific error so boot can fall back to the safe (logoless) screen.
 */
#ifndef ROBO_BMP_H
#define ROBO_BMP_H

#include <stdint.h>

typedef enum {
    BMP_OK = 0,
    BMP_ERR_TOO_SMALL,      /* file smaller than the minimum header */
    BMP_ERR_MAGIC,          /* missing "BM" */
    BMP_ERR_HEADER,         /* bad/unsupported info header size or fields */
    BMP_ERR_FORMAT,         /* unsupported bpp/compression */
    BMP_ERR_DIMENSIONS,     /* zero, negative, or absurd dimensions */
    BMP_ERR_SIZE,           /* declared image size exceeds actual file size */
    BMP_ERR_ALLOC           /* allocation failure */
} bmp_result_t;

#define BMP_MAX_DIMENSION 16384u

/* Decode a BMP file image from [data, data+size).
 * On BMP_OK: *out_pixels is a malloc'd (by bmp_set_allocator backend) buffer of
 * width*height*4 bytes, rows top-down, BGRA order. Caller frees via
 * bmp_free_pixels(). out_w and out_h receive the dimensions. */
bmp_result_t bmp_decode(const uint8_t *data, uint64_t size,
                        uint8_t **out_pixels, uint32_t *out_w, uint32_t *out_h);

/* Free a buffer returned by bmp_decode (only valid with the UEFI allocator
 * wired in via bmp_set_allocator; on host tests, plain free() is used). */
void bmp_free_pixels(uint8_t *p);

/* Allocator injection: the UEFI build wires mem_alloc/mem_free here; host
 * tests wire malloc/free. Called once before any decode. */
void bmp_set_allocator(void *(*alloc_fn)(uint64_t), void (*free_fn)(void *));

#endif /* ROBO_BMP_H */
