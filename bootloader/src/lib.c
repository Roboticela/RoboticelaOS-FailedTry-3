/* robobootloader - tiny freestanding runtime implementation. */
#include "lib.h"

void *robomemset(void *dst, uint8_t value, uint64_t count)
{
    uint8_t *d = (uint8_t *)dst;
    while (count--)
        *d++ = value;
    return dst;
}

void *robomemcpy(void *dst, const void *src, uint64_t count)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (count--)
        *d++ = *s++;
    return dst;
}

int robomemcmp(const void *a, const void *b, uint64_t count)
{
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    while (count--) {
        if (*x != *y)
            return (int)*x - (int)*y;
        x++;
        y++;
    }
    return 0;
}

void *robomove(void *dst, const void *src, uint64_t count)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || count == 0)
        return dst;
    if (d < s) {
        while (count--)
            *d++ = *s++;
    } else {
        d += count;
        s += count;
        while (count--)
            *--d = *--s;
    }
    return dst;
}

uint64_t robolen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (uint64_t)(p - s);
}

int robostrcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

bool robostreq(const char *a, const char *b)
{
    return robostrcmp(a, b) == 0;
}

void robostrncpy(char *dst, uint64_t dstcap, const char *src)
{
    uint64_t i;
    if (dstcap == 0)
        return;
    for (i = 0; i + 1 < dstcap && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

uint64_t roboc16len(const uint16_t *s)
{
    uint64_t n = 0;
    while (n < 4096 && s[n])
        n++;
    return n;
}

bool roboparse_u64(const char *s, uint64_t *out)
{
    uint64_t v = 0;
    if (!s || !out)
        return false;
    if (*s == '\0')
        return false;
    while (*s) {
        if (*s < '0' || *s > '9')
            return false;
        if (v > (UINT64_MAX - (uint64_t)(*s - '0')) / 10u)
            return false; /* overflow */
        v = v * 10u + (uint64_t)(*s - '0');
        s++;
    }
    *out = v;
    return true;
}

bool roboadd_ovf_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    if (a > UINT64_MAX - b)
        return true; /* overflowed */
    if (out)
        *out = a + b;
    return false;
}

bool robomul_ovf_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    if (a != 0 && b > UINT64_MAX / a)
        return true; /* overflowed */
    if (out)
        *out = a * b;
    return false;
}

/* ------------------------------------------------------------- formatting */

static void emit(char *buf, uint64_t cap, uint64_t *n, char c)
{
    if (*n + 1 < cap)
        buf[*n] = c;
    (*n)++;
}

static void emit_str(char *buf, uint64_t cap, uint64_t *n, const char *s)
{
    while (*s)
        emit(buf, cap, n, *s++);
}

static void emit_u64(char *buf, uint64_t cap, uint64_t *n, uint64_t v,
                     unsigned base, bool zero_pad, int width)
{
    char tmp[32];
    const char *digits = "0123456789abcdef";
    int i = 0;
    if (v == 0)
        tmp[i++] = '0';
    while (v) {
        tmp[i++] = digits[v % base];
        v /= base;
    }
    while (zero_pad && i < width && i < (int)sizeof(tmp))
        tmp[i++] = '0';
    while (i > 0)
        emit(buf, cap, n, tmp[--i]);
}

uint64_t robovsnprintf(char *buf, uint64_t cap, const char *fmt, va_list ap)
{
    uint64_t n = 0;
    while (*fmt) {
        if (*fmt != '%') {
            emit(buf, cap, &n, *fmt++);
            continue;
        }
        fmt++;
        if (*fmt == '0') { /* zero-padded hex, e.g. %016llx */
            fmt++;
            int width = 0;
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + (*fmt++ - '0');
            if (*fmt == 'l') fmt++;
            if (*fmt == 'l') fmt++;
            if (*fmt == 'x') {
                emit_u64(buf, cap, &n, va_arg(ap, uint64_t), 16, true, width);
                fmt++;
            } else {
                emit(buf, cap, &n, '%');
                emit(buf, cap, &n, 'x');
            }
            continue;
        }
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            emit_str(buf, cap, &n, s ? s : "(null)");
            break;
        }
        case 'c':
            emit(buf, cap, &n, (char)va_arg(ap, int));
            break;
        case 'd': {
            int64_t v = (int64_t)va_arg(ap, int); /* %d args are 32-bit int */
            uint64_t uv;
            if (v < 0) {
                emit(buf, cap, &n, '-');
                uv = (uint64_t)(-(v + 1)) + 1;
            } else {
                uv = (uint64_t)v;
            }
            emit_u64(buf, cap, &n, uv, 10, false, 0);
            break;
        }
        case 'u':
            emit_u64(buf, cap, &n, (uint64_t)va_arg(ap, unsigned int), 10, false, 0);
            break;
        case 'l': /* length modifiers: %llu %llx %ld %lld ... */
            while (*fmt == 'l')
                fmt++; /* consume all l's; outer loop skips the conversion */
            if (*fmt == 'x') {
                emit_u64(buf, cap, &n, va_arg(ap, uint64_t), 16, false, 0);
            } else if (*fmt == 'u') {
                emit_u64(buf, cap, &n, va_arg(ap, uint64_t), 10, false, 0);
            } else if (*fmt == 'd') {
                int64_t v = (int64_t)va_arg(ap, uint64_t);
                uint64_t uv;
                if (v < 0) {
                    emit(buf, cap, &n, '-');
                    uv = (uint64_t)(-(v + 1)) + 1;
                } else {
                    uv = (uint64_t)v;
                }
                emit_u64(buf, cap, &n, uv, 10, false, 0);
            } else {
                emit(buf, cap, &n, '%');
                emit(buf, cap, &n, 'l');
            }
            break;
        case 'x':
            emit_u64(buf, cap, &n, (uint64_t)va_arg(ap, unsigned int), 16, false, 0);
            break;
        case 'p': {
            uint64_t v = (uint64_t)(uintptr_t)va_arg(ap, void *);
            emit_str(buf, cap, &n, "0x");
            emit_u64(buf, cap, &n, v, 16, true, 16);
            break;
        }
        case '%':
            emit(buf, cap, &n, '%');
            break;
        default:
            emit(buf, cap, &n, '%');
            emit(buf, cap, &n, *fmt);
            break;
        }
        fmt++;
    }
    if (cap > 0)
        buf[n < cap ? n : cap - 1] = '\0';
    return n;
}

uint64_t robosnprintf(char *buf, uint64_t cap, const char *fmt, ...)
{
    va_list ap;
    uint64_t n;
    va_start(ap, fmt);
    n = robovsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}
