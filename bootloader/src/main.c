/* robobootloader - UEFI entry point. */
#include "uefi/uefi.h"
#include "memory/memory.h"
#include "boot.h"
#include "image/bmp.h"
#include "elf/elf_loader.h"
#include "debug.h"
#include "lib.h"

#include <stdint.h>

/* ---------------- allocator wiring for decoders ---------------- */

/* Reserve the kernel's fixed physical range and return the identity-mapped
 * pointer UEFI provides (x86-64 UEFI runs identity-mapped pre-ExitBootServices). */
static void *uefi_page_map_at(uint64_t pages, uint64_t want_phys)
{
    EFI_STATUS s;
    UINT64 addr = want_phys;
    s = bs()->AllocatePages(AllocateAddress, EfiLoaderData, (UINTN)pages, &addr);
    if (EFI_ERROR(s))
        return NULL;
    return (void *)(uintptr_t)addr; /* == want_phys on success */
}

static void uefi_page_free(uint64_t phys, uint64_t pages)
{
    bs()->FreePages(phys, (UINTN)pages);
}

static void *uefi_alloc_bridge(uint64_t size)
{
    return mem_alloc(size);
}

static void uefi_free_bridge(void *p)
{
    mem_free(p);
}

/* ---------------- entry ---------------- */

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    debug_init();
    debug_write("[BOOT] efi_main entered\n");
    mem_init(SystemTable, ImageHandle);
    bmp_set_allocator(uefi_alloc_bridge, uefi_free_bridge);
    elf_set_allocators(uefi_page_map_at, uefi_page_free);

    boot_run(ImageHandle, SystemTable);
    return EFI_LOAD_ERROR; /* not reached */
}
