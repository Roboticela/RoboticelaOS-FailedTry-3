/* robobootloader - RoboBootInfo: the bootloader-to-kernel handoff contract.
 *
 * This structure is the ONLY official interface between RoboBootloader and the
 * future RoboticelaOS kernel. It is filled in by the bootloader, placed in
 * EfiLoaderData memory, its physical address is passed in RDI (SysV) / RCX
 * (MSVC) at kernel entry, and the memory it occupies is EfiLoaderData after
 * ExitBootServices (the kernel must add descriptor attributes if it wants more
 * guarantees).
 *
 * Versioning: bump ROBOBOOTINFO_VERSION for any layout change; the kernel must
 * refuse to boot on a major version it does not understand.
 *
 * NOTE: x86-64 UEFI boots with paging identity-mapped; the kernel receives
 * physical addresses everywhere.
 */
#ifndef ROBO_BOOTINFO_H
#define ROBO_BOOTINFO_H

#include <stdint.h>

#define ROBOBOOTINFO_VERSION 1

/* framebuffer_format values (subset of UEFI GOP pixel formats we support) */
#define ROBO_FB_FORMAT_BGRX 0    /* 32bpp, byte order B,G,R,x (PixelBlueGreenRedReserved) */
#define ROBO_FB_FORMAT_RGBX 1    /* 32bpp, byte order R,G,B,x (PixelRedGreenBlueReserved) */
#define ROBO_FB_FORMAT_BITMASK 2
#define ROBO_FB_FORMAT_UNKNOWN 0xFF

/* boot_device: which kind of device the boot files came from (informational). */
#define ROBO_BOOT_DEVICE_ESP 1

typedef struct {
    uint64_t version;                   /* ROBOBOOTINFO_VERSION */

    /* UEFI memory map (final, captured just before ExitBootServices).
     * memory_map_size is in BYTES; entries are memory_map_descriptor_size
     * bytes apart (they may be larger than sizeof(descriptor)). */
    uint64_t memory_map;
    uint64_t memory_map_size;
    uint64_t memory_map_descriptor_size;
    uint32_t memory_map_descriptor_version;
    uint32_t _pad0;

    /* framebuffer */
    uint64_t framebuffer_address;       /* physical */
    uint64_t framebuffer_size;          /* bytes */
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;         /* bytes per scanline */
    uint32_t framebuffer_format;        /* ROBO_FB_FORMAT_* */

    /* ACPI */
    uint64_t acpi_rsdp;                 /* physical address of RSDP (ACPI 2.0+), 0 if none */

    /* kernel */
    uint64_t kernel_base;               /* physical load base (first PT_LOAD byte) */
    uint64_t kernel_size;               /* total memory span of PT_LOAD segments */
    uint64_t kernel_entry;              /* physical entry point */

    /* optional initfs (flat file loaded verbatim; 0/0 if absent) */
    uint64_t initfs_address;
    uint64_t initfs_size;

    /* misc */
    uint32_t boot_device;               /* ROBO_BOOT_DEVICE_* */
    uint32_t _pad1;
    uint64_t command_line;              /* physical ptr to NUL-terminated CHAR8, 0 if none */
} RoboBootInfo;

/* Compile-time sanity: layout is pointer-free and 8-byte multiples. */
_Static_assert(sizeof(RoboBootInfo) % 8 == 0, "RoboBootInfo must be 8-byte multiple");

/* --- construction helpers (implemented in bootinfo.c) -------------------- */

RoboBootInfo *bootinfo_alloc(void);
void bootinfo_set_framebuffer(RoboBootInfo *bi, uint64_t addr, uint64_t size,
                              uint32_t w, uint32_t h, uint32_t pitch, uint32_t format);
void bootinfo_set_kernel(RoboBootInfo *bi, uint64_t base, uint64_t size, uint64_t entry);
void bootinfo_set_initfs(RoboBootInfo *bi, uint64_t addr, uint64_t size);
void bootinfo_set_memory_map(RoboBootInfo *bi, uint64_t map, uint64_t size,
                             uint64_t desc_size, uint32_t desc_version);
void bootinfo_set_acpi_rsdp(RoboBootInfo *bi, uint64_t rsdp);
void bootinfo_debug_dump(const RoboBootInfo *bi);

#endif /* ROBO_BOOTINFO_H */
