/* robobootloader - memory management wrappers around UEFI boot services.
 *
 * Every allocation is overflow-checked; on any failure callers get NULL or an
 * EFI error and boot fails cleanly. The final memory map that is embedded in
 * RoboBootInfo is gathered here (with the documented re-fetch loop).
 */
#ifndef ROBO_MEMORY_H
#define ROBO_MEMORY_H

#include "uefi/uefi.h"

#include <stdint.h>

/* One-time init: captures gST/gBS/gImageHandle. */
void mem_init(EFI_SYSTEM_TABLE *st, EFI_HANDLE image_handle);

EFI_BOOT_SERVICES  *bs(void);
EFI_SYSTEM_TABLE   *st(void);
EFI_HANDLE          image_handle(void);

/* Pool allocation (EfiLoaderData). Returns NULL on failure or if size==0. */
void *mem_alloc(uint64_t size);
void *mem_alloc_zero(uint64_t size);
void  mem_free(void *p);

/* Page allocation (EfiLoaderData pages). *addr receives the physical base.
 * Returns EFI_SUCCESS or an EFI error; never partial. */
EFI_STATUS mem_alloc_pages(uint64_t pages, uint64_t *addr);
EFI_STATUS mem_free_pages(uint64_t addr, uint64_t pages);

/* Capture the current UEFI memory map into freshly allocated storage.
 * Returns EFI_SUCCESS; *map must be mem_free()d by the caller (unless it is
 * intentionally carried across ExitBootServices, as boot.c does). */
EFI_STATUS mem_get_map(EFI_MEMORY_DESCRIPTOR **map, uint64_t *map_size,
                       uint64_t *desc_size, uint32_t *desc_version, uint64_t *map_key);

/* Copy `bytes` from a file-style byte source into a freshly allocated buffer.
 * Convenience wrapper used by config/bmp/elf loaders. */
void *mem_alloc_copy(const void *src, uint64_t bytes);

#endif /* ROBO_MEMORY_H */
