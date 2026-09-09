/* robobootloader - UEFI service helpers: files, ACPI RSDP, etc. */
#include "uefi.h"
#include "config/config.h"
#include "debug.h"
#include "lib.h"
#include "memory/memory.h"

#include <stdint.h>

#define ROBO_MAX_FILE_SIZE (64ull * 1024 * 1024) /* refuse >64MB single files */

/* ---------------------------------------------------------------- volumes */

EFI_FILE_PROTOCOL *file_open_volume(void)
{
    EFI_STATUS s;
    EFI_HANDLE *handles = NULL;
    UINTN count = 0, buf_size = 0, i;
    EFI_GUID sfsg = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_FILE_PROTOCOL *volume = NULL;

    /* Prefer the volume we were booted from. */
    {
        EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
        s = bs()->HandleProtocol(image_handle(), &li_guid, (void **)&li);
        if (!EFI_ERROR(s) && li && li->DeviceHandle) {
            EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs = NULL;
            s = bs()->HandleProtocol(li->DeviceHandle, &sfsg, (void **)&sfs);
            if (!EFI_ERROR(s) && sfs) {
                s = sfs->OpenVolume(sfs, (void **)&volume);
                if (!EFI_ERROR(s) && volume)
                    return volume;
            }
        }
        debugf("boot-device volume unavailable (0x%llx), scanning all ESPs", (uint64_t)s);
    }

    /* Fallback: scan every SimpleFileSystem handle. */
    s = bs()->LocateHandleBuffer(EFI_BY_PROTOCOL, &sfsg, NULL, &count, &handles);
    if (EFI_ERROR(s))
        return NULL;
    for (i = 0; i < count && !volume; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs = NULL;
        if (EFI_ERROR(bs()->HandleProtocol(handles[i], &sfsg, (void **)&sfs)) || !sfs)
            continue;
        if (!EFI_ERROR(sfs->OpenVolume(sfs, (void **)&volume)))
            break;
        volume = NULL;
    }
    if (handles)
        bs()->FreePool(handles);
    return volume;
}

EFI_FILE_PROTOCOL *file_open(EFI_FILE_PROTOCOL *volume, const char *path)
{
    CHAR16 wpath[ROBO_CFG_MAX_PATH];
    EFI_FILE_PROTOCOL *f = NULL;
    uint64_t i, n;
    EFI_STATUS s;

    if (!volume || !path)
        return NULL;
    n = robolen(path);
    if (n == 0 || n >= ROBO_CFG_MAX_PATH)
        return NULL;
    /* UEFI paths use '\' as separator; our config uses '/'. Convert. */
    for (i = 0; i < n; i++) {
        char c = path[i];
        if (c == '/')
            c = '\\';
        wpath[i] = (CHAR16)(uint8_t)c; /* our paths are ASCII */
    }
    wpath[n] = 0;

    s = volume->Open(volume, (void **)&f, wpath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(s))
        return NULL;
    return f;
}

void file_close(EFI_FILE_PROTOCOL *f)
{
    if (f)
        f->Close(f);
}

bool file_read_all(EFI_FILE_PROTOCOL *f, uint8_t **out_data, uint64_t *out_size)
{
    EFI_STATUS s;
    EFI_GUID fi_guid = EFI_FILE_INFO_ID;
    UINTN fi_size = 0;
    uint8_t *fi_buf = NULL;
    UINTN fsize = 0;
    uint8_t *buf = NULL;

    if (!f || !out_data || !out_size)
        return false;
    *out_data = NULL;
    *out_size = 0;

    /* Query file size via GetInfo(EFI_FILE_INFO). */
    s = f->GetInfo(f, &fi_guid, &fi_size, NULL);
    if (s != EFI_BUFFER_TOO_SMALL || fi_size < sizeof(EFI_FILE_INFO))
        return false;
    fi_buf = mem_alloc((uint64_t)fi_size);
    if (!fi_buf)
        return false;
    s = f->GetInfo(f, &fi_guid, &fi_size, fi_buf);
    if (EFI_ERROR(s)) {
        mem_free(fi_buf);
        return false;
    }
    /* EFI_FILE_INFO.FileSize is at offset 8 (Size, FileSize). */
    fsize = (UINTN)*(uint64_t *)(fi_buf + 8);
    mem_free(fi_buf);

    if (fsize == 0 || fsize > ROBO_MAX_FILE_SIZE)
        return false;

    buf = mem_alloc((uint64_t)fsize + 1); /* +1 so text files can be NUL-terminated */
    if (!buf)
        return false;

    s = f->Read(f, &fsize, buf);
    if (EFI_ERROR(s)) {
        mem_free(buf);
        return false;
    }
    buf[fsize] = 0;
    *out_data = buf;
    *out_size = (uint64_t)fsize;
    return true;
}

/* Chunked read with progress callbacks. Whole-file semantics (same buffer
 * layout as file_read_all), but reads in ~256 KiB chunks and invokes the
 * callback after each chunk so the caller can animate UI. Never blocks:
 * the callback itself must not. */
#define READ_CHUNK_SIZE  (256u * 1024u)

bool file_read_all_progress(EFI_FILE_PROTOCOL *f, uint8_t **out_data,
                            uint64_t *out_size, file_progress_fn progress,
                            void *ctx)
{
    EFI_STATUS s;
    EFI_GUID fi_guid = EFI_FILE_INFO_ID;
    UINTN fi_size = 0;
    uint8_t *fi_buf = NULL;
    uint64_t fsize = 0;
    uint8_t *buf = NULL;
    uint64_t done = 0;

    if (!f || !out_data || !out_size)
        return false;
    *out_data = NULL;
    *out_size = 0;

    s = f->GetInfo(f, &fi_guid, &fi_size, NULL);
    if (s != EFI_BUFFER_TOO_SMALL || fi_size < sizeof(EFI_FILE_INFO))
        return false;
    fi_buf = mem_alloc((uint64_t)fi_size);
    if (!fi_buf)
        return false;
    s = f->GetInfo(f, &fi_guid, &fi_size, fi_buf);
    if (EFI_ERROR(s)) {
        mem_free(fi_buf);
        return false;
    }
    fsize = *(uint64_t *)(fi_buf + 8);  /* EFI_FILE_INFO.FileSize */
    mem_free(fi_buf);

    if (fsize == 0 || fsize > ROBO_MAX_FILE_SIZE)
        return false;

    buf = mem_alloc(fsize + 1);
    if (!buf)
        return false;

    while (done < fsize) {
        UINTN chunk = fsize - done;
        if (chunk > READ_CHUNK_SIZE)
            chunk = READ_CHUNK_SIZE;
        s = f->Read(f, &chunk, buf + done);
        if (EFI_ERROR(s)) {
            mem_free(buf);
            return false;
        }
        if (chunk == 0)
            break;                      /* short read; treat as EOF */
        done += chunk;
        if (progress)
            progress(done, fsize, ctx);
    }

    buf[done] = 0;
    *out_data = buf;
    *out_size = done;
    return true;
}

bool file_read_whole(EFI_FILE_PROTOCOL *volume, const char *path,
                     uint8_t **out_data, uint64_t *out_size)
{
    EFI_FILE_PROTOCOL *f = file_open(volume, path);
    bool ok;
    if (!f) {
        debugf("file not found: %s", path);
        return false;
    }
    ok = file_read_all(f, out_data, out_size);
    file_close(f);
    if (ok)
        debugf("file loaded: %s (%llu bytes)", path, *out_size);
    return ok;
}

/* ------------------------------------------------------------- ACPI RSDP */

static bool guid_eq(const EFI_GUID *a, const EFI_GUID *b)
{
    return robomemcmp(a, b, sizeof(EFI_GUID)) == 0;
}

uint64_t find_acpi_rsdp(void)
{
    EFI_SYSTEM_TABLE *syst = st();
    UINTN i;
    EFI_GUID acpi20 = EFI_ACPI_20_TABLE_GUID;
    EFI_GUID acpi10 = EFI_ACPI_10_TABLE_GUID;

    if (!syst)
        return 0;
    for (i = 0; i < syst->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *t = &syst->ConfigurationTable[i];
        if (guid_eq(&t->VendorGuid, &acpi20)) {
            debugf("ACPI 2.0+ RSDP at 0x%p", t->VendorTable);
            return (uint64_t)(uintptr_t)t->VendorTable;
        }
    }
    for (i = 0; i < syst->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *t = &syst->ConfigurationTable[i];
        if (guid_eq(&t->VendorGuid, &acpi10)) {
            debugf("ACPI 1.0 RSDP at 0x%p", t->VendorTable);
            return (uint64_t)(uintptr_t)t->VendorTable;
        }
    }
    debugf("no ACPI RSDP in configuration tables");
    return 0;
}
