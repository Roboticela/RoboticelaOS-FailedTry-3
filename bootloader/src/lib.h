/* robobootloader - tiny freestanding C runtime helpers.
 *
 * The bootloader is built with -ffreestanding and no CRT: everything here is
 * what the rest of the code is allowed to assume exists. Keep this list short.
 * clang provides codegen for __builtin_va_list even freestanding, so the
 * formatter can use <stdarg.h>.
 */
#ifndef ROBO_LIB_H
#define ROBO_LIB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

/* memory ---------------------------------------------------------------- */
void  *robomemset(void *dst, uint8_t value, uint64_t count);
void  *robomemcpy(void *dst, const void *src, uint64_t count);
int    robomemcmp(const void *a, const void *b, uint64_t count);
void  *robomove(void *dst, const void *src, uint64_t count); /* overlap-safe */

/* strings (CHAR8) ------------------------------------------------------- */
uint64_t robolen(const char *s);
int      robostrcmp(const char *a, const char *b);
bool     robostreq(const char *a, const char *b);
void     robostrncpy(char *dst, uint64_t dstcap, const char *src); /* always NUL */
uint64_t roboc16len(const uint16_t *s); /* CHAR16 length, capped at 4096 */

/* parsing --------------------------------------------------------------- */

/* Parse a non-negative decimal integer. Returns true on success.
 * Rejects empty strings, non-digits, and values > 2^63-1. */
bool roboparse_u64(const char *s, uint64_t *out);

/* Checked arithmetic: return true on OVERFLOW (out untouched). */
bool roboadd_ovf_u64(uint64_t a, uint64_t b, uint64_t *out);
bool robomul_ovf_u64(uint64_t a, uint64_t b, uint64_t *out);

/* formatting (debug/error output only) ----------------------------------- */

/* Minimal printf: %s %c %d %u %x %p %% (no width/precision/floats).
 * Always NUL-terminates; returns characters written (excluding NUL). */
uint64_t robosnprintf(char *buf, uint64_t cap, const char *fmt, ...);
uint64_t robovsnprintf(char *buf, uint64_t cap, const char *fmt, va_list ap);

#endif /* ROBO_LIB_H */
