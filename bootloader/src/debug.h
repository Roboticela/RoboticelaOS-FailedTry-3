/* robobootloader - developer-only diagnostics channel (COM1, 16550 UART).
 *
 * Normal boot never touches this module unless debug mode is enabled
 * (compile-time ROBO_DEBUG=1 or boot.cfg "debug=1"). Output goes to the COM1
 * I/O port 0x3F8, which QEMU captures via -serial. Nothing here ever draws to
 * the framebuffer: the graphical boot screen stays clean.
 */
#ifndef ROBO_DEBUG_H
#define ROBO_DEBUG_H

#include <stdbool.h>

void debug_init(void);
void debug_write(const char *s);

/* Formatted debug line. Prefixes with "[BOOT] ". No-op unless debug_init()
 * has been called (guarded by debug-enabled state). */
void debugf(const char *fmt, ...);

/* Raw formatted output without the prefix (still debug-gated). */
void debugf_raw(const char *fmt, ...);

bool debug_enabled(void);

#endif /* ROBO_DEBUG_H */
