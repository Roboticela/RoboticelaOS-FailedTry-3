/* robobootloader - COM1 (16550) debug output. Never draws on screen. */
#include "debug.h"
#include "lib.h"

#include <stdarg.h>
#include <stdint.h>

#define COM1_BASE 0x3F8

/* 16550 registers (byte-wide, offsets from base) */
#define REG_DATA 0 /* THR (w) / RBR (r) */
#define REG_IER  1
#define REG_FCR  2
#define REG_LCR  3
#define REG_MCR  4
#define REG_LSR  5

#define LSR_THRE 0x20 /* transmit holding register empty */

static bool g_debug_ready = false;

static inline void out8(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t in8(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

void debug_init(void)
{
    out8(COM1_BASE + REG_IER, 0x00); /* no interrupts */
    out8(COM1_BASE + REG_LCR, 0x80); /* DLAB on */
    out8(COM1_BASE + 0, 0x01);       /* divisor 1 -> 115200 */
    out8(COM1_BASE + 1, 0x00);
    out8(COM1_BASE + REG_LCR, 0x03); /* 8N1, DLAB off */
    out8(COM1_BASE + REG_FCR, 0xC7); /* enable+clear FIFOs */
    out8(COM1_BASE + REG_MCR, 0x0B); /* DTR, RTS, OUT2 */
    g_debug_ready = true;
    debug_write("[BOOT] serial debug channel ready\n");
}

bool debug_enabled(void)
{
    return g_debug_ready;
}

void debug_write(const char *s)
{
    if (!g_debug_ready || !s)
        return;
    while (*s) {
        if (*s == '\n') {
            while (!(in8(COM1_BASE + REG_LSR) & LSR_THRE))
                ;
            out8(COM1_BASE + REG_DATA, '\r');
        }
        while (!(in8(COM1_BASE + REG_LSR) & LSR_THRE))
            ;
        out8(COM1_BASE + REG_DATA, (uint8_t)*s++);
    }
}

void debugf_raw(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    if (!g_debug_ready)
        return;
    va_start(ap, fmt);
    robovsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    debug_write(buf);
}

void debugf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    if (!g_debug_ready)
        return;
    va_start(ap, fmt);
    robovsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    debug_write("[BOOT] ");
    debug_write(buf);
    debug_write("\n");
}
