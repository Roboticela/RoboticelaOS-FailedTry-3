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

/* Boot progress stages (per-mille of progress bar). */
enum {
    BOOT_PCT_START        = 50,
    BOOT_PCT_FRAMEBUFFER  = 150,
    BOOT_PCT_CONFIG       = 250,
    BOOT_PCT_KERNEL_FOUND = 400,
    BOOT_PCT_KERNEL_READ  = 550,
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
