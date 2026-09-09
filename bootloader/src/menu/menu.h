/* robobootloader - Apple-style boot device picker ("choose startup disk").
 *
 * Shown when the user holds Alt (option) at power-on, presses Down/F2 on the
 * firmware splash, or boot.cfg sets menu=always. Mirrors the macOS picker:
 * a dimmed black overlay, a centred window with the bootable devices as
 * rounded rows (volume label or \EFI\BOOT\BOOTX64.EFI path), arrow keys to
 * select, Enter to boot, Esc to continue the normal boot path, and a
 * countdown that auto-boots the highlighted entry when it hits zero.
 *
 * Rendering reuses the boot-splash primitives so the picker looks native to
 * the RoboticelaOS boot experience at any resolution (all layout is derived
 * from the framebuffer size, like the splash).
 *
 * Freestanding; only boot services are used, never after ExitBootServices.
 */
#ifndef ROBO_MENU_H
#define ROBO_MENU_H

#include "uefi/uefi.h"
#include "graphics/framebuffer.h"
#include "graphics/renderer.h"

#include <stdint.h>
#include <stdbool.h>

/* One selectable boot entry (a volume with a loader). */
typedef struct {
    EFI_FILE_PROTOCOL *root;    /* opened volume root (caller closes) */
    char label[48];             /* volume label or fallback text */
    bool has_loader;            /* \EFI\BOOT\BOOTX64.EFI present */
    bool this_volume;           /* volume we were booted from */
} robo_menu_entry_t;

/* Result of the picker. */
typedef enum {
    ROBO_MENU_BOOT_SELECTED = 0,    /* boot the selected entry */
    ROBO_MENU_CONTINUE,             /* Esc / timeout-expired default: normal boot */
    ROBO_MENU_ERROR                 /* no bootable volumes / no screen */
} robo_menu_result_t;

/* Run the picker. Blocks until the user picks, presses Esc, or the timeout
 * expires. `logo` is the boot-splash logo (drawn small in the window header,
 * may be NULL). `timeout_seconds` = countdown; 0 disables it (menu waits
 * forever for a key). Returns the outcome; on ROBO_MENU_BOOT_SELECTED the
 * chosen volume's root is left open in *out_root (caller owns it) and its
 * index reported in *out_index. */
robo_menu_result_t menu_run(fb_t *fb, const robo_image_t *logo,
                            uint32_t timeout_seconds,
                            robo_menu_entry_t **entries, uint64_t *entry_count,
                            uint64_t *out_index, EFI_FILE_PROTOCOL **out_root);

/* Draw the picker once (used for screenshots/tests); selection = index. */
void menu_draw(fb_t *fb, const robo_image_t *logo,
               const robo_menu_entry_t *entries, uint64_t count,
               uint64_t selection, int32_t seconds_left);

/* Apple's boot-key watch: poll the keyboard for the window duration (ms)
 * while `pump`ing the boot splash each frame. Returns true when the picker
 * should open (Alt held, Down/Escape pressed, or EX reports Alt). */
bool menu_boot_key_requested(uint32_t window_ms, void (*pump)(void));

/* Raw firmware device handle of entry `index` (for chainload device paths). */
EFI_HANDLE menu_entry_device_handle(uint64_t index);

#endif /* ROBO_MENU_H */
