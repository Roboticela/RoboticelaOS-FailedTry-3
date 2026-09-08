/* robobootloader - boot flow implementation. */
#include "boot.h"
#include "uefi/uefi.h"
#include "config/config.h"
#include "graphics/framebuffer.h"
#include "graphics/renderer.h"
#include "image/bmp.h"
#include "elf/elf_loader.h"
#include "memory/memory.h"
#include "bootinfo/bootinfo.h"
#include "debug.h"
#include "lib.h"

#include <stdint.h>

/* --------------------------------------------------------- UI plumbing */

static fb_t g_fb;
static robo_image_t g_logo;         /* decoded logo (runtime or embedded) */
static uint8_t *g_logo_pixels = NULL;
static bool g_ui_ready = false;
static robo_config_t g_cfg;
static uint8_t *g_initfs_data = NULL;
static uint64_t g_initfs_size = 0;
static RoboBootInfo *g_bootinfo = NULL;

/* Embedded logo, generated into build/generated/logo_embed.S by the Makefile. */
extern const uint8_t embedded_logo_start[];
extern const uint8_t embedded_logo_end[];

static void ui_stage(uint32_t per_mille)
{
    if (g_ui_ready)
        render_progress(&g_fb, per_mille);
}

static bool load_logo_image(const robo_config_t *cfg)
{
    uint8_t *data = NULL;
    uint64_t size = 0;
    EFI_FILE_PROTOCOL *vol;

    /* 1) runtime logo from ESP (allows replacing without rebuilding) */
    vol = file_open_volume();
    if (vol) {
        if (file_read_whole(vol, cfg->logo_path, &data, &size)) {
            bmp_result_t r = bmp_decode(data, size, &g_logo_pixels,
                                        &g_logo.width, &g_logo.height);
            if (r == BMP_OK) {
                g_logo.pixels = g_logo_pixels;
                g_logo.pitch = g_logo.width * 4;
                mem_free(data);
                debugf("logo loaded from %s (%ux%u)", cfg->logo_path,
                       g_logo.width, g_logo.height);
                return true;
            }
            debugf("logo decode failed (%d) from %s", (int)r, cfg->logo_path);
            mem_free(data);
        }
    }

    /* 2) embedded fallback logo */
    {
        uint64_t esz = (uint64_t)(embedded_logo_end - embedded_logo_start);
        bmp_result_t r = bmp_decode(embedded_logo_start, esz, &g_logo_pixels,
                                    &g_logo.width, &g_logo.height);
        if (r == BMP_OK) {
            g_logo.pixels = g_logo_pixels;
            g_logo.pitch = g_logo.width * 4;
            debugf("embedded logo loaded (%ux%u)", g_logo.width, g_logo.height);
            return true;
        }
        debugf("embedded logo decode failed (%d) - booting logoless", (int)r);
    }
    return false;
}

/* -------------------------------------------------------- ExitBootServices */

static EFI_MEMORY_DESCRIPTOR *g_final_map = NULL;
static uint64_t g_final_map_size = 0, g_final_desc_size = 0, g_final_map_key = 0;
static uint32_t g_final_desc_version = 0;

/* ExitBootServices must be called with a current map key. Between our final
 * GetMemoryMap and the call, the firmware may allocate (e.g. for the stub we
 * call nothing, but defensive retry is spec-required anyway). */
static bool exit_boot_services(void)
{
    const int max_tries = 8;
    int t;
    for (t = 0; t < max_tries; t++) {
        EFI_STATUS s;
        /* (Re)fetch the map. */
        if (g_final_map) {
            /* Previous attempt failed after fetch; free and retry. */
            mem_free(g_final_map);
            g_final_map = NULL;
        }
        s = mem_get_map(&g_final_map, &g_final_map_size, &g_final_desc_size,
                        &g_final_desc_version, &g_final_map_key);
        if (EFI_ERROR(s))
            return false;

        s = bs()->ExitBootServices(image_handle(), (UINTN)g_final_map_key);
        if (!EFI_ERROR(s))
            return true;

        debugf("ExitBootServices attempt %d failed (0x%llx); retrying", t + 1,
               (uint64_t)s);
        /* Falling through retries with a fresh map/key. */
    }
    return false;
}

/* ------------------------------------------------------------- handoff */

/* Jump to the kernel and never return. Implemented here in inline asm so the
 * bootloader needs no external assembler.
 *
 * This file is compiled for the MS x86-64 ABI (UEFI calling convention), so
 * the arguments arrive in RCX (entry) and RDX (bootinfo). The future kernel
 * and the test kernel use the SysV ABI, whose first argument register is RDI:
 * we move bootinfo into RDI, entry into RAX, mask interrupts, clear DF,
 * terminate frame-pointer chains, and jump. There is no way back: boot
 * services are gone. */
__attribute__((noreturn)) static void kernel_handoff(uint64_t entry, RoboBootInfo *bi)
{
    __asm__ volatile(
        "mov %%rdx, %%rdi\n"   /* RDI = bootinfo (kernel SysV arg 1) */
        "mov %%rcx, %%rax\n"   /* RAX = entry */
        "cli\n"                /* interrupts stay off; we never iret */
        "cld\n"
        "xor %%ebp, %%ebp\n"
        "jmp *%%rax\n"
        :
        : "c"(entry), "d"(bi)
        : "memory");
    __builtin_unreachable();
}

/* ------------------------------------------------------------ main flow */

void boot_run(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    mem_init(SystemTable, ImageHandle);

    robomemset(&g_cfg, 0, sizeof(g_cfg));
    config_defaults(&g_cfg);

#ifdef ROBO_DEBUG_BUILD
    g_cfg.debug = true;
#endif
    if (g_cfg.debug)
        debug_init();
    debugf("RoboBootloader starting; firmware rev 0x%08x",
           SystemTable->FirmwareRevision);
    if (SystemTable->FirmwareVendor) {
        CHAR16 *v = SystemTable->FirmwareVendor;
        char ascii[64];
        uint64_t i;
        for (i = 0; i < sizeof(ascii) - 1 && v[i]; i++)
            ascii[i] = (char)v[i];
        ascii[i] = 0;
        debugf("firmware: %s", ascii);
    }

    /* Disable the boot-services watchdog: we control the boot experience. */
    bs()->SetWatchdogTimer(0, 0, 0, NULL);

    ui_stage(BOOT_PCT_START);

    /* --- framebuffer --- */
    {
        EFI_STATUS s = fb_init(&g_fb);
        if (EFI_ERROR(s)) {
            /* No usable framebuffer: debug serial is the only channel left. */
            debugf("framebuffer init failed: 0x%llx", (uint64_t)s);
            boot_fatal("no framebuffer");
            return;
        }
        g_ui_ready = true;
        render_boot_frame(&g_fb, NULL); /* black + zero bar while we decode logo */
        ui_stage(BOOT_PCT_FRAMEBUFFER);
    }

    /* --- configuration (missing file is fine: defaults) --- */
    {
        EFI_FILE_PROTOCOL *vol = file_open_volume();
        if (vol) {
            uint8_t *cfg_text = NULL;
            uint64_t cfg_size = 0;
            if (file_read_whole(vol, "RoboBoot/boot.cfg", &cfg_text, &cfg_size)) {
                cfg_result_t cr = config_parse((const char *)cfg_text, &g_cfg);
                mem_free(cfg_text);
                if (cr != CFG_OK) {
                    debugf("boot.cfg parse error %d", (int)cr);
                    boot_fatal("invalid boot.cfg");
                    return;
                }
            } else {
                debugf("no boot.cfg; using defaults");
            }
        }
        if (g_cfg.debug && !debug_enabled())
            debug_init();
        debugf("config: kernel='%s' logo='%s' initfs='%s' test=%d debug=%d",
               g_cfg.kernel_path, g_cfg.logo_path, g_cfg.initfs_path,
               (int)g_cfg.test_mode, (int)g_cfg.debug);
    }
    ui_stage(BOOT_PCT_CONFIG);

    /* --- logo --- */
    load_logo_image(&g_cfg);
    render_boot_frame(&g_fb, g_logo.pixels ? &g_logo : NULL);
    ui_stage(BOOT_PCT_CONFIG); /* redraw bar (logo draw resets it) */

    /* --- kernel --- */
    {
        EFI_FILE_PROTOCOL *vol = file_open_volume();
        uint8_t *kdata = NULL;
        uint64_t ksize = 0;
        elf_load_result_t elf = {0, 0, 0};
        elf_result_t er;

        if (!vol || !file_read_whole(vol, g_cfg.kernel_path, &kdata, &ksize)) {
            boot_fatal("kernel not found");
            return;
        }
        ui_stage(BOOT_PCT_KERNEL_READ);

        er = elf_load(kdata, ksize, &elf);
        mem_free(kdata);
        if (er != ELF_OK) {
            debugf("kernel rejected: elf error %d", (int)er);
            boot_fatal("invalid kernel");
            return;
        }
        debugf("kernel loaded: base=0x%llx size=0x%llx entry=0x%llx",
               elf.base, elf.size, elf.entry);
        ui_stage(BOOT_PCT_KERNEL_LOAD);

        /* --- optional initfs --- */
        {
            uint8_t *idata = NULL;
            uint64_t isize = 0;
            if (g_cfg.initfs_path[0] && vol &&
                file_read_whole(vol, g_cfg.initfs_path, &idata, &isize)) {
                /* Place it in fresh pages, record in bootinfo later. */
                RoboBootInfo *tmp = NULL;
                (void)tmp;
                /* Store pointer/size for bootinfo via statics. */
                g_initfs_data = idata;
                g_initfs_size = isize;
            } else {
                debugf("no initfs (ok)");
            }
        }

        /* --- bootinfo --- */
        {
            RoboBootInfo *bi = bootinfo_alloc();
            if (!bi) {
                boot_fatal("out of memory");
                return;
            }
            bootinfo_set_framebuffer(bi, g_fb.base, g_fb.size, g_fb.width,
                                     g_fb.height, g_fb.pitch,
                                     g_fb.format == ROBO_PX_BGRX
                                         ? ROBO_FB_FORMAT_BGRX : ROBO_FB_FORMAT_RGBX);
            bootinfo_set_kernel(bi, elf.base, elf.size, elf.entry);
            bootinfo_set_acpi_rsdp(bi, find_acpi_rsdp());
            bi->boot_device = ROBO_BOOT_DEVICE_ESP;
            if (g_initfs_data)
                bootinfo_set_initfs(bi, (uint64_t)(uintptr_t)g_initfs_data,
                                    g_initfs_size);
            ui_stage(BOOT_PCT_BOOTINFO);
            bootinfo_debug_dump(bi);
            g_bootinfo = bi;
        }

        /* --- test mode: hold the completed boot screen for inspection --- */
        if (g_cfg.test_mode) {
            debugf("TEST MODE: holding boot screen");
            bs()->Stall(15000000); /* 15s: enough for screenshots at any res */
        }

        /* --- exit boot services --- */
        ui_stage(BOOT_PCT_DONE);
        if (!exit_boot_services()) {
            /* Boot services may be gone-ish; serial remains. */
            debugf_raw("ExitBootServices FAILED\n");
            /* We cannot safely continue. Show error via framebuffer (still
             * mapped) and hang. */
            render_fatal_error(&g_fb, g_logo.pixels ? &g_logo : NULL);
            for (;;)
                __asm__ volatile("hlt");
        }
        debugf_raw("[BOOT] ExitBootServices successful\n");
        debugf_raw("[BOOT] jumping to kernel entry 0x%llx\n", g_bootinfo->kernel_entry);

        kernel_handoff(g_bootinfo->kernel_entry, g_bootinfo);
        /* Not reached. */
    }
}

void boot_fatal(const char *reason)
{
    debugf_raw("[BOOT] FATAL: %s\n", reason);
    if (g_ui_ready)
        render_fatal_error(&g_fb, g_logo.pixels ? &g_logo : NULL);
    /* Keep the screen visible; UEFI watchdog is off. */
    for (;;)
        __asm__ volatile("hlt");
}

