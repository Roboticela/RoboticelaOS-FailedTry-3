/* robobootloader - UEFI service helpers: files, keyboard, ACPI RSDP, etc. */
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
    UINTN count = 0, i;
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

/* Convenience: open by path on a volume, read the whole file, close. */
bool file_read_whole(EFI_FILE_PROTOCOL *volume, const char *path,
                     uint8_t **out_data, uint64_t *out_size)
{
    EFI_FILE_PROTOCOL *f = file_open(volume, path);
    bool ok;

    if (!f)
        return false;
    ok = file_read_all(f, out_data, out_size);
    file_close(f);
    return ok;
}

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

/* ------------------------------------------------------- volume enumeration */

#define ROBO_MAX_VOLUMES 8
static EFI_HANDLE g_vol_handles[ROBO_MAX_VOLUMES];

uint64_t file_open_all_volumes(EFI_FILE_PROTOCOL **handles, uint64_t max)
{
    EFI_STATUS s;
    EFI_GUID sfsg = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    UINTN count = 0;
    EFI_HANDLE *raw = NULL;
    uint64_t n = 0, i;

    if (!handles || max == 0)
        return 0;

    s = bs()->LocateHandleBuffer(EFI_BY_PROTOCOL, &sfsg, NULL, &count, &raw);
    if (EFI_ERROR(s) || !raw || count == 0) {
        if (raw)
            bs()->FreePool(raw);
        return 0;
    }

    for (i = 0; i < count && n < max && n < ROBO_MAX_VOLUMES; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs = NULL;
        EFI_FILE_PROTOCOL *root = NULL;
        s = bs()->HandleProtocol(raw[i], &sfsg, (void **)&sfs);
        if (EFI_ERROR(s) || !sfs)
            continue;
        s = sfs->OpenVolume(sfs, (void **)&root);
        if (EFI_ERROR(s) || !root)
            continue;
        g_vol_handles[n] = raw[i];
        handles[n] = root;
        n++;
    }
    bs()->FreePool(raw);
    return n;
}

EFI_HANDLE volume_device_handle(uint64_t index)
{
    if (index >= ROBO_MAX_VOLUMES)
        return NULL;
    return g_vol_handles[index];
}

void file_volume_label(EFI_FILE_PROTOCOL *root, char *out, uint64_t cap)
{
    EFI_STATUS s;
    EFI_GUID fsinfo_guid = EFI_FILE_SYSTEM_INFO_ID;
    UINTN buf_size = 512;
    uint8_t *buf;
    EFI_FILE_SYSTEM_INFO *info;
    uint64_t i, n;

    if (cap)
        out[0] = '\0';
    if (!root || cap < 2)
        return;

    buf = mem_alloc(buf_size);
    if (!buf)
        return;
    s = root->GetInfo(root, &fsinfo_guid, &buf_size, buf);
    if (EFI_ERROR(s)) {
        mem_free(buf);
        return;
    }
    info = (EFI_FILE_SYSTEM_INFO *)buf;
    n = 0;
    for (i = 0; info->VolumeLabel[i] && n < cap - 1; i++) {
        CHAR16 c = info->VolumeLabel[i];
        if (c >= 0x20 && c < 0x7F)
            out[n++] = (char)c;         /* ASCII only; others dropped */
    }
    out[n] = '\0';
    mem_free(buf);
}

uint64_t file_size(EFI_FILE_PROTOCOL *f)
{
    EFI_STATUS s;
    EFI_GUID fi_guid = EFI_FILE_INFO_ID;
    UINTN fi_size = 0;
    uint8_t fi_small[sizeof(EFI_FILE_INFO) + 2];
    UINTN pos = 0;
    uint64_t sz;

    if (!f)
        return 0;
    fi_size = sizeof(fi_small);
    s = f->GetInfo(f, &fi_guid, &fi_size, fi_small);
    if (EFI_ERROR(s))
        return 0;
    sz = *(uint64_t *)(fi_small + 8);   /* EFI_FILE_INFO.FileSize */
    f->SetPosition(f, pos);             /* rewind for subsequent reads */
    if (sz > ROBO_MAX_FILE_SIZE)
        return 0;
    return sz;
}

/* ------------------------------------------------------------- console in */

EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *console_input_ex(void)
{
    EFI_STATUS s;
    EFI_GUID ex_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *ex = NULL;
    EFI_HANDLE cin_handle = st()->ConsoleInHandle;

    if (!cin_handle)
        return NULL;
    s = bs()->HandleProtocol(cin_handle, &ex_guid, (void **)&ex);
    if (!EFI_ERROR(s) && ex)
        return ex;
    /* Some firmware only exposes EX on child handles; probe all handles. */
    {
        UINTN count = 0;
        EFI_HANDLE *raw = NULL;

        s = bs()->LocateHandleBuffer(EFI_BY_PROTOCOL, &ex_guid, NULL,
                                     &count, &raw);
        if (!EFI_ERROR(s) && raw && count > 0) {
            s = bs()->HandleProtocol(raw[0], &ex_guid, (void **)&ex);
            if (EFI_ERROR(s))
                ex = NULL;
        }
        if (raw)
            bs()->FreePool(raw);
    }
    return ex;
}

/* ----------------------------------------------------- firmware chainload */

/* Build "<volume device path>\EFI\BOOT\BOOTX64.EFI" as a full device path:
 * [volume path nodes] + MEDIA/FILEPATH node + END node. LoadImage then
 * behaves exactly like the firmware launching that file itself. */
EFI_STATUS file_load_image(EFI_HANDLE volume_handle, const CHAR16 *path,
                           EFI_HANDLE *out_image)
{
    EFI_STATUS s;
    EFI_GUID dp_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
    EFI_DEVICE_PATH_PROTOCOL *vol_dp = NULL;
    EFI_DEVICE_PATH_PROTOCOL *full = NULL, *node, *end;
    UINTN vol_len = 0, path_len = 0, full_len, i;
    UINT8 *w;

    if (!volume_handle || !path || !out_image)
        return EFI_INVALID_PARAMETER;

    s = bs()->HandleProtocol(volume_handle, &dp_guid, (void **)&vol_dp);
    if (EFI_ERROR(s) || !vol_dp)
        return EFI_UNSUPPORTED;

    /* Walk the volume's device path to find its total length (incl. END). */
    node = vol_dp;
    while (node->Type != EFI_DP_TYPE_END) {
        UINT16 l = (UINT16)(node->Length[0] | (node->Length[1] << 8));
        if (l < 4)
            return EFI_INVALID_PARAMETER;
        vol_len += l;
        node = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)node + l);
    }
    vol_len += (UINTN)(node->Length[0] | (node->Length[1] << 8)); /* END node */

    while (path[path_len])
        path_len++;

    full_len = vol_len + 4 + (path_len + 1) * 2;
    full = mem_alloc((uint64_t)full_len);
    if (!full)
        return EFI_OUT_OF_RESOURCES;

    robomemcpy(full, vol_dp, vol_len);
    {
        /* Overwrite the volume path's trailing END node (4 bytes) with our
         * MEDIA/FILEPATH node, then append a fresh END. */
        UINTN body = vol_len - 4;
        w = (UINT8 *)full + body;

        w[0] = EFI_DP_TYPE_MEDIA;
        w[1] = EFI_DP_SUBTYPE_MEDIA_FILE;
        w[2] = (UINT8)((4 + (path_len + 1) * 2) & 0xFF);
        w[3] = (UINT8)((4 + (path_len + 1) * 2) >> 8);
        w += 4;
        for (i = 0; i < path_len; i++) {
            w[i * 2] = (UINT8)(path[i] & 0xFF);
            w[i * 2 + 1] = (UINT8)(path[i] >> 8);
        }
        w[path_len * 2] = 0;
        w[path_len * 2 + 1] = 0;
        w += (path_len + 1) * 2;

        end = (EFI_DEVICE_PATH_PROTOCOL *)w;
        end->Type = EFI_DP_TYPE_END;
        end->SubType = EFI_DP_SUBTYPE_END_ALL;
        end->Length[0] = 4;
        end->Length[1] = 0;
    }

    s = bs()->LoadImage(TRUE, image_handle(), full, 0, 0, out_image);
    mem_free(full);
    if (!EFI_ERROR(s))
        debugf("LoadImage OK (image handle %p)", (void *)*out_image);
    else
        debugf("LoadImage failed: 0x%llx", (uint64_t)s);
    return s;
}

EFI_STATUS file_start_image(EFI_HANDLE image)
{
    UINTN exit_data_size = 0;
    CHAR16 *exit_data = NULL;
    return bs()->StartImage(image, &exit_data_size, &exit_data);
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
