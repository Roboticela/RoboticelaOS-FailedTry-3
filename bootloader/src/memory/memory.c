/* robobootloader - memory management implementation. */
#include "memory.h"
#include "debug.h"
#include "lib.h"

static EFI_SYSTEM_TABLE *g_st = NULL;
static EFI_BOOT_SERVICES *g_bs = NULL;
static EFI_HANDLE g_image = NULL;

void mem_init(EFI_SYSTEM_TABLE *st, EFI_HANDLE image_handle)
{
    g_st = st;
    g_bs = st->BootServices;
    g_image = image_handle;
}

EFI_BOOT_SERVICES *bs(void)
{
    return g_bs;
}

EFI_SYSTEM_TABLE *st(void)
{
    return g_st;
}

EFI_HANDLE image_handle(void)
{
    return g_image;
}

void *mem_alloc(uint64_t size)
{
    void *p = NULL;
    EFI_STATUS s;
    if (size == 0)
        return NULL;
    s = g_bs->AllocatePool(EfiLoaderData, (UINTN)size, &p);
    if (EFI_ERROR(s))
        return NULL;
    return p;
}

void *mem_alloc_zero(uint64_t size)
{
    void *p = mem_alloc(size);
    if (p)
        robomemset(p, 0, size);
    return p;
}

void mem_free(void *p)
{
    if (p)
        g_bs->FreePool(p);
}

EFI_STATUS mem_alloc_pages(uint64_t pages, uint64_t *addr)
{
    EFI_STATUS s;
    UINT64 memory = 0;
    if (pages == 0 || !addr)
        return EFI_INVALID_PARAMETER;
    s = g_bs->AllocatePages(AllocateAnyPages, EfiLoaderData, (UINTN)pages, &memory);
    if (EFI_ERROR(s))
        return s;
    *addr = memory;
    return EFI_SUCCESS;
}

EFI_STATUS mem_free_pages(uint64_t addr, uint64_t pages)
{
    return g_bs->FreePages((UINT64)addr, (UINTN)pages);
}

EFI_STATUS mem_get_map(EFI_MEMORY_DESCRIPTOR **map, uint64_t *map_size,
                       uint64_t *desc_size, uint32_t *desc_version, uint64_t *map_key)
{
    EFI_STATUS s;
    UINTN size = 0, key = 0, dsize = 0;
    UINT32 dver = 0;
    EFI_MEMORY_DESCRIPTOR *buf = NULL;

    if (!map || !map_size || !desc_size || !desc_version)
        return EFI_INVALID_PARAMETER;

    /* First call: firmware returns required size via EFI_BUFFER_TOO_SMALL. */
    size = 0;
    s = g_bs->GetMemoryMap(&size, NULL, &key, &dsize, &dver);
    if (s != EFI_BUFFER_TOO_SMALL || size == 0 || dsize == 0) {
        debugf("GetMemoryMap sizing call failed: 0x%llx (size=%llu dsize=%llu)",
               (uint64_t)s, (uint64_t)size, (uint64_t)dsize);
        return EFI_LOAD_ERROR;
    }

    /* Pad for descriptors created between sizing and the real call. */
    size += 4 * dsize;
    buf = mem_alloc(size);
    if (!buf)
        return EFI_OUT_OF_RESOURCES;

    s = g_bs->GetMemoryMap(&size, buf, &key, &dsize, &dver);
    if (EFI_ERROR(s)) {
        mem_free(buf);
        debugf("GetMemoryMap failed: 0x%llx", (uint64_t)s);
        return s;
    }

    *map = buf;
    *map_size = size;
    *desc_size = dsize;
    *desc_version = dver;
    if (map_key)
        *map_key = key;
    return EFI_SUCCESS;
}

void *mem_alloc_copy(const void *src, uint64_t bytes)
{
    void *p = mem_alloc(bytes);
    if (!p)
        return NULL;
    robomemcpy(p, src, bytes);
    return p;
}
