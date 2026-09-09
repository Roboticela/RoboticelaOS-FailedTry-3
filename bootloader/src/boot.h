/* robobootloader - boot flow orchestration.
 *
 * Sequence (each stage updates the progress bar):
 *   UEFI startup -> init -> framebuffer -> config -> kernel locate/read ->
 *   validate/load -> bootinfo -> ExitBootServices -> kernel entry.
 */
#ifndef ROBO_BOOT_H
#define ROBO_BOOT_H

#include "uefi/uefi.h"
#include "bootinfo/bootinfo.h"

/* Boot progress stages (per-mille of progress bar).
 *
 * The bar is animated by graphics/progress.c: stage values are floors on a
 * smooth ease-out timeline, so a fast boot still shows calm motion and slow
 * stages (kernel read) are tracked byte-by-byte inside their span without
 * blocking the actual work. */
enum {
    BOOT_PCT_START        = 30,
    BOOT_PCT_FRAMEBUFFER  = 100,
    BOOT_PCT_CONFIG       = 200,
    BOOT_PCT_KERNEL_FOUND = 450,
    /* 450..600 is the kernel-read span; filled by real byte progress. */
    BOOT_PCT_KERNEL_READ_END = 600,
    BOOT_PCT_KERNEL_VALID = 700,
    BOOT_PCT_KERNEL_LOAD  = 850,
    BOOT_PCT_BOOTINFO     = 950,
    BOOT_PCT_DONE         = 1000
};

/* Entry from efi_main. Never returns: on success jumps to the kernel, on
 * failure shows the fatal screen and resets the machine. */
void boot_run(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

/* Show the clean fatal-error screen and hang (after ExitBootServices it is
 * the only safe option; before it we could reset via Runtime Services). */
void boot_fatal(const char *reason);

#endif /* ROBO_BOOT_H */
