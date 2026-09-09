/* robobootloader - Apple-style boot device picker implementation. */
#include "menu.h"
#include "keyboard/keyboard.h"
#include "uefi/uefi.h"
#include "memory/memory.h"
#include "font/robofont.h"
#include "debug.h"
#include "lib.h"

#include <stdint.h>
#include <stdbool.h>

#define ROBO_MENU_MAX       8
#define ROBO_MENU_VISIBLE   5   /* rows shown at once (simple scroll) */
#define ROBO_LOADER_PATH    "EFI/BOOT/BOOTX64.EFI"

/* Static storage: the picker owns the entry table for the boot's lifetime
 * (single-threaded bootloader; no allocation needed). */
static robo_menu_entry_t g_entries[ROBO_MENU_MAX];
static EFI_HANDLE g_entry_handles[ROBO_MENU_MAX];   /* raw SFS handle per entry */

/* Colours (Apple-dark palette). */
#define ROW_SEL_R  0x2C
#define ROW_SEL_G  0x3E
#define ROW_SEL_B  0x55
#define ROW_IDLE_R 0x23
#define ROW_IDLE_G 0x24
#define ROW_IDLE_B 0x27
#define TXT_MAIN   0xF2
#define TXT_SUB    0x8E

/* ------------------------------------------------------------ discovery */

static bool volume_has_loader(EFI_FILE_PROTOCOL *root)
{
    EFI_FILE_PROTOCOL *f = file_open(root, ROBO_LOADER_PATH);
    if (f) {
        file_close(f);
        return true;
    }
    return false;
}

static uint64_t discover_entries(void)
{
    EFI_FILE_PROTOCOL *roots[ROBO_MENU_MAX];
    uint64_t n, m = 0, i;
    EFI_HANDLE boot_dev = NULL;

    /* Device handle of our own image -> marks "this disk". */
    {
        EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
        if (!EFI_ERROR(bs()->HandleProtocol(image_handle(), &li_guid,
                                            (void **)&li)) && li)
            boot_dev = li->DeviceHandle;
    }

    n = file_open_all_volumes(roots, ROBO_MENU_MAX);
    for (i = 0; i < n && m < ROBO_MENU_MAX; i++) {
        robo_menu_entry_t *e = &g_entries[m];
        EFI_HANDLE raw = volume_device_handle(i);

        if (!volume_has_loader(roots[i])) {
            /* Not bootable: not listed (Apple lists only bootable disks). */
            debugf("menu: volume %u has no " ROBO_LOADER_PATH " - skipped",
                   (unsigned)(i + 1));
            file_close(roots[i]);
            continue;
        }

        robomemset(e, 0, sizeof(*e));
        e->root = roots[i];
        file_volume_label(roots[i], e->label, sizeof(e->label));
        if (!e->label[0])
            robosnprintf(e->label, sizeof(e->label), "Disk %u", (unsigned)(i + 1));
        e->has_loader = true;
        e->this_volume = (boot_dev != NULL && raw == boot_dev);
        g_entry_handles[m] = raw;
        debugf("menu: volume %u label='%s'%s", (unsigned)(i + 1), e->label,
               e->this_volume ? " [boot device]" : "");
        m++;
    }

    /* Stable sort: the volume we booted from first (and preselected). */
    for (i = 1; i < m; i++) {
        if (g_entries[i].this_volume && !g_entries[i - 1].this_volume) {
            robo_menu_entry_t te = g_entries[i];
            EFI_HANDLE th = g_entry_handles[i];
            uint64_t j = i;
            while (j > 0 && !g_entries[j - 1].this_volume) {
                g_entries[j] = g_entries[j - 1];
                g_entry_handles[j] = g_entry_handles[j - 1];
                j--;
            }
            g_entries[j] = te;
            g_entry_handles[j] = th;
        }
    }
    return m;
}

/* ------------------------------------------------------------- drawing */

static void draw_centred_text(fb_t *fb, uint32_t y, const char *text,
                              uint32_t scale, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t w = render_text_width(text, scale);
    uint32_t x = (fb->width > w) ? (fb->width - w) / 2 : 0;
    render_text_scaled(fb, x, y, text, scale, r, g, b);
}

void menu_draw(fb_t *fb, const robo_image_t *logo,
               const robo_menu_entry_t *entries, uint64_t count,
               uint64_t selection, int32_t seconds_left)
{
    uint32_t W = fb->width, H = fb->height;
    uint32_t title_scale = (H >= 700) ? 3 : 2;
    uint32_t text_scale  = (H >= 700) ? 2 : 1;
    uint32_t row_h = (uint32_t)((double)H * 0.085);
    uint32_t row_w = (uint32_t)((double)W * ((W >= 900) ? 0.56 : 0.86));
    uint32_t row_x = (W >= row_w) ? (W - row_w) / 2 : 0;
    uint32_t y0    = (uint32_t)((double)H * 0.34);
    double   gap_f = 0.25;   /* rows breathe: 25% of row height between them */
    uint64_t offset, shown, i;

    fb_clear_black(fb);

    /* Header: small logo + title, like the macOS "choose startup disk". */
    render_logo_scaled(fb, logo, 0.145, 0.085);
    draw_centred_text(fb, (uint32_t)((double)H * 0.235), "Choose Startup Disk",
                      title_scale, TXT_MAIN, TXT_MAIN, TXT_MAIN);

    if (count == 0) {
        draw_centred_text(fb, (uint32_t)((double)H * 0.42),
                          "No Bootable Devices", title_scale, TXT_MAIN, 0x60,
                          0x60);
        draw_centred_text(fb, (uint32_t)((double)H * 0.50),
                          "Connect a disk with an EFI bootloader",
                          text_scale, TXT_SUB, TXT_SUB, TXT_SUB);
        return;
    }

    /* Simple scroll window on the selection. */
    offset = (selection >= ROBO_MENU_VISIBLE) ? selection - (ROBO_MENU_VISIBLE - 1)
                                              : 0;
    shown = count - offset;
    if (shown > ROBO_MENU_VISIBLE)
        shown = ROBO_MENU_VISIBLE;

    for (i = 0; i < shown; i++) {
        const robo_menu_entry_t *e = &entries[offset + i];
        uint32_t ry = y0 + (uint32_t)(i * (row_h + row_h * gap_f));
        bool sel = (offset + i) == selection;

        render_round_rect(fb, row_x, ry, row_w, row_h,
                          sel ? ROW_SEL_R : ROW_IDLE_R,
                          sel ? ROW_SEL_G : ROW_IDLE_G,
                          sel ? ROW_SEL_B : ROW_IDLE_B);

        /* Volume label, vertically centred in the row. */
        {
            uint32_t tx = row_x + row_w / 14;
            uint32_t ty = ry + (row_h > ROBOFONT_LINE * text_scale
                                    ? (row_h - ROBOFONT_LINE * text_scale) / 2
                                    : 0);
            render_text_scaled(fb, tx, ty, e->label, text_scale,
                               TXT_MAIN, TXT_MAIN, TXT_MAIN);
        }

        /* Marker dot on the disk we booted from (Apple's "current disk"). */
        if (e->this_volume) {
            uint32_t d = row_h / 6;
            if (d < 6)
                d = 6;
            render_round_rect(fb, row_x + row_w - d * 2,
                              ry + (row_h - d) / 2, d, d,
                              0x30, 0xD1, 0x58);
        }
    }

    /* Footer: countdown while it runs, key hints otherwise. */
    if (seconds_left >= 0) {
        char buf[96];
        robosnprintf(buf, sizeof(buf), "Booting %s in %d s   (Enter = now, Esc = cancel)",
                     entries[selection].label, (int)seconds_left);
        draw_centred_text(fb, (uint32_t)((double)H * 0.92), buf, text_scale,
                          TXT_SUB, TXT_SUB, TXT_SUB);
    } else {
        draw_centred_text(fb, (uint32_t)((double)H * 0.92),
                          "Up/Down select   Enter boot   Esc continue",
                          text_scale, TXT_SUB, TXT_SUB, TXT_SUB);
    }
}

/* -------------------------------------------------------- boot-key watch */

bool menu_boot_key_requested(uint32_t window_ms, void (*pump)(void))
{
    uint32_t frames = window_ms / 16;

    for (uint32_t f = 0; f < frames; f++) {
        robo_key_t k;
        if (keyboard_poll(&k)) {
            /* Any keystroke with Alt held, or an explicit menu key. */
            if (k.alt_down)
                return true;
            if (k.scan_code == SCAN_DOWN || k.scan_code == SCAN_ESC)
                return true;
        }
        /* Cheap EX modifier snapshot (firmware that reports Alt without a
         * keystroke). */
        if (keyboard_alt_down())
            return true;
        if (pump)
            pump();
        bs()->Stall(16000);
    }
    return false;
}

/* ------------------------------------------------------------ event loop */

robo_menu_result_t menu_run(fb_t *fb, const robo_image_t *logo,
                            uint32_t timeout_seconds,
                            robo_menu_entry_t **entries, uint64_t *entry_count,
                            uint64_t *out_index, EFI_FILE_PROTOCOL **out_root)
{
    uint64_t count, selection = 0;
    uint64_t frame = 0;
    uint64_t frames_total = (timeout_seconds > 0) ? (uint64_t)timeout_seconds * 62
                                                  : 0; /* 0 = wait forever */
    bool have_countdown = frames_total > 0;

    keyboard_flush();

    count = discover_entries();
    if (entries)
        *entries = g_entries;
    if (entry_count)
        *entry_count = count;
    if (out_root)
        *out_root = NULL;
    if (out_index)
        *out_index = 0;

    debugf("menu: %u bootable volume(s), timeout=%u s", (unsigned)count,
           (unsigned)timeout_seconds);

    /* First paint (with countdown visible, if any). */
    menu_draw(fb, logo, g_entries, count, selection,
              have_countdown ? (int32_t)timeout_seconds : -1);

    for (;;) {
        robo_key_t k;

        if (keyboard_poll(&k)) {
            bool handled = false;

            if (k.scan_code == SCAN_UP && selection > 0) {
                selection--;
                handled = true;
            } else if (k.scan_code == SCAN_DOWN && selection + 1 < count) {
                selection++;
                handled = true;
            } else if (k.unicode == 13 || k.unicode == 10) {
                break;                          /* Enter: boot selection */
            } else if (k.scan_code == SCAN_ESC) {
                /* Esc: leave everything open for the normal boot path. */
                for (uint64_t i = 0; i < count; i++)
                    file_close(g_entries[i].root);
                if (entries)
                    *entries = NULL;
                return ROBO_MENU_CONTINUE;
            }

            if (handled) {
                /* Apple restarts the countdown when the selection moves. */
                frame = 0;
                menu_draw(fb, logo, g_entries, count, selection,
                          have_countdown ? (int32_t)timeout_seconds : -1);
            }
        }

        if (have_countdown && frame >= frames_total)
            break;                              /* countdown expired: boot */

        /* One countdown tick per second (62 frames of 16 ms). */
        if (have_countdown && frame > 0 && (frame % 62) == 0) {
            int32_t left = (int32_t)(timeout_seconds - frame / 62);
            if (left < 0)
                left = 0;
            menu_draw(fb, logo, g_entries, count, selection, left);
        }

        frame++;
        bs()->Stall(16000);
    }

    if (count == 0) {
        /* Countdown cannot run with no entries; error paint + brief hold. */
        for (uint32_t f = 0; f < 180; f++)      /* ~3 s */
            bs()->Stall(16000);
        return ROBO_MENU_ERROR;
    }

    /* Close every root except the chosen one. */
    for (uint64_t i = 0; i < count; i++) {
        if (i == selection)
            continue;
        file_close(g_entries[i].root);
    }
    if (out_index)
        *out_index = selection;
    if (out_root)
        *out_root = g_entries[selection].root;
    return ROBO_MENU_BOOT_SELECTED;
}

/* Raw device handle of a menu entry (for LoadImage device paths). */
EFI_HANDLE menu_entry_device_handle(uint64_t index)
{
    if (index >= ROBO_MENU_MAX)
        return NULL;
    return g_entry_handles[index];
}
