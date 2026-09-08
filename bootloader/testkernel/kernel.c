/* robobootloader test kernel - integration stub ONLY.
 *
 * This is not the future RoboticelaOS kernel. It exists solely to verify the
 * UEFI -> RoboBootloader -> ELF loader -> RoboBootInfo -> ExitBootServices ->
 * kernel entry pipeline. It must never grow features.
 *
 * Entry ABI (set by the bootloader's handoff): SysV x86-64, first argument
 * (RDI) = physical pointer to RoboBootInfo. The ELF entry point is `kmain`.
 *
 * Success markers (checked by automated tests):
 *   serial: "[TESTKERNEL] OK"
 *   framebuffer top-left 16x16 pixels: green (0x00FF00 in RGBX-order BGRX mem)
 */
#include "../src/bootinfo/bootinfo.h"

#include <stdint.h>

#define COM1 0x3F8

static inline void out8(uint16_t port, uint8_t v)
{
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void serial_init(void)
{
    out8(COM1 + 1, 0x00);
    out8(COM1 + 3, 0x80);
    out8(COM1 + 0, 0x01); /* 115200 baud */
    out8(COM1 + 1, 0x00);
    out8(COM1 + 3, 0x03); /* 8N1 */
    out8(COM1 + 2, 0xC7);
    out8(COM1 + 4, 0x0B);
}

static void serial_putc(char c)
{
    while (!(inb(COM1 + 5) & 0x20))
        ;
    out8(COM1, (uint8_t)c);
}

static void serial_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s++);
    }
}

static void paint_success(const RoboBootInfo *bi)
{
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)bi->framebuffer_address;
    uint32_t stride = bi->framebuffer_pitch / 4;
    for (uint32_t y = 0; y < 16 && y < bi->framebuffer_height; y++)
        for (uint32_t x = 0; x < 16 && x < bi->framebuffer_width; x++)
            fb[y * stride + x] = 0x0000FF00; /* green in XRGB layout */
}

/* ELF entry point (linker script sets ENTRY(kmain)).
 * RDI = RoboBootInfo* per the handoff ABI. */
void kmain(uint64_t bi_phys)
{
    const RoboBootInfo *bi = (const RoboBootInfo *)(uintptr_t)bi_phys;
    serial_init();
    serial_puts("\n[TESTKERNEL] hello\n");
    if (bi && bi->version == ROBOBOOTINFO_VERSION) {
        serial_puts("[TESTKERNEL] bootinfo version OK\n");
        if (bi->framebuffer_address)
            paint_success(bi);
        serial_puts("[TESTKERNEL] OK\n");
    } else {
        serial_puts("[TESTKERNEL] FAIL: bad bootinfo\n");
    }
    for (;;)
        __asm__ volatile("hlt");
}
