/* robobootloader - ELF64 loader implementation. */
#include "elf_loader.h"
#include "lib.h"
#include "debug.h"

#include <stddef.h>

/* ---------------------------------------------------------------- ELF defs */

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr_;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr_;

#define PT_LOAD_ 1

#define EM_X86_64_ 62
#define ET_EXEC_   2

#define EI_MAG0_ 0
#define EI_CLASS_ 4
#define EI_DATA_ 5
#define EI_VERSION_ 6
#define ELFCLASS64_ 2
#define ELFDATA2LSB_ 1

/* -------------------------------------------------------------- allocators */

static void *(*elf_page_map_at)(uint64_t pages, uint64_t want_phys) = NULL;
static void (*elf_page_free)(uint64_t phys, uint64_t pages) = NULL;

void elf_set_allocators(void *(*page_map_at)(uint64_t pages, uint64_t want_phys),
                        void (*page_free)(uint64_t phys, uint64_t pages))
{
    elf_page_map_at = page_map_at;
    elf_page_free = page_free;
}

/* ------------------------------------------------------------ validation */

static inline uint32_t rd32e(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline uint64_t rd64e(const uint8_t *p)
{
    return (uint64_t)rd32e(p) | (uint64_t)rd32e(p + 4) << 32;
}

/* Inclusive range [off, off+len) within file of `size`? */
static bool in_file(uint64_t off, uint64_t len, uint64_t size)
{
    if (off > size)
        return false;
    if (len > size - off)
        return false;
    return true;
}

elf_result_t elf_validate(const uint8_t *data, uint64_t size)
{
    const Elf64_Ehdr_ *eh;
    uint64_t ph_table_end, phend_check;
    uint16_t phentsize, phnum;

    if (!data || !size)
        return ELF_ERR_PARAM;
    if (size < sizeof(Elf64_Ehdr_))
        return ELF_ERR_TOO_SMALL;

    eh = (const Elf64_Ehdr_ *)data;
    if (eh->e_ident[EI_MAG0_] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
        return ELF_ERR_MAGIC;
    if (eh->e_ident[EI_CLASS_] != ELFCLASS64_)
        return ELF_ERR_CLASS;
    if (eh->e_ident[EI_DATA_] != ELFDATA2LSB_)
        return ELF_ERR_ENDIAN;
    if (eh->e_ident[EI_VERSION_] != 1 || eh->e_version != 1)
        return ELF_ERR_VERSION;
    if (eh->e_machine != EM_X86_64_)
        return ELF_ERR_MACHINE;
    if (eh->e_type != ET_EXEC_)
        return ELF_ERR_TYPE;
    if (eh->e_ehsize != sizeof(Elf64_Ehdr_))
        return ELF_ERR_HEADER_SIZE;

    phentsize = eh->e_phentsize;
    phnum = eh->e_phnum;

    if (phnum == 0)
        return ELF_ERR_NO_LOAD;
    if (phentsize < 56) /* sizeof(Elf64_Phdr_) */
        return ELF_ERR_HEADER_SIZE;
    if (phentsize > 4096)
        return ELF_ERR_HEADER_SIZE;

    /* Program header table must lie inside the file. */
    if (robomul_ovf_u64((uint64_t)phnum, (uint64_t)phentsize, &ph_table_end))
        return ELF_ERR_OVERFLOW;
    if (roboadd_ovf_u64(eh->e_phoff, ph_table_end, &phend_check))
        return ELF_ERR_OVERFLOW;
    if (!in_file(eh->e_phoff, ph_table_end, size))
        return ELF_ERR_PHDR_BOUNDS;

    return ELF_OK;
}

/* --------------------------------------------------------------- loading */

elf_result_t elf_load(const uint8_t *data, uint64_t size, elf_load_result_t *out)
{
    elf_result_t vr;
    const Elf64_Ehdr_ *eh;
    uint16_t phentsize, phnum;
    uint64_t i;
    bool have_load = false;
    uint64_t min_addr = UINT64_MAX, max_addr = 0;
    elf_result_t result = ELF_OK;

    if (!out)
        return ELF_ERR_PARAM;
    out->entry = out->base = out->size = 0;

    vr = elf_validate(data, size);
    if (vr != ELF_OK)
        return vr;

    eh = (const Elf64_Ehdr_ *)data;
    phentsize = eh->e_phentsize;
    phnum = eh->e_phnum;

    /* Pass 1: validate all PT_LOAD segments and compute the address span. */
    for (i = 0; i < phnum; i++) {
        const uint8_t *ph_raw = data + eh->e_phoff + i * (uint64_t)phentsize;
        uint32_t p_type = rd32e(ph_raw + 0);
        uint64_t p_offset = rd64e(ph_raw + 8);
        uint64_t p_vaddr = rd64e(ph_raw + 16);
        uint64_t p_paddr = rd64e(ph_raw + 24);
        uint64_t p_filesz = rd64e(ph_raw + 32);
        uint64_t p_memsz = rd64e(ph_raw + 40);
        uint64_t seg_end_v, seg_end_f;

        if (p_type != PT_LOAD_)
            continue;

        if (p_memsz < p_filesz)
            return ELF_ERR_SEGMENT;
        if (!in_file(p_offset, p_filesz, size))
            return ELF_ERR_SEGMENT;
        if (roboadd_ovf_u64(p_vaddr, p_memsz, &seg_end_v))
            return ELF_ERR_OVERFLOW;
        if (roboadd_ovf_u64(p_offset, p_filesz, &seg_end_f))
            return ELF_ERR_OVERFLOW;
        (void)seg_end_f;
        /* Refuse absurdly low addresses (real mode / BIOS territory) that the
         * firmware may be using, and addresses that would wrap. */
        if (p_vaddr < 0x100000ULL)
            return ELF_ERR_ADDRESS;
        if (p_paddr != 0 && p_paddr != p_vaddr)
            return ELF_ERR_ADDRESS; /* Stage 01: identity-mapped loads only */

        if (p_memsz > 0) {
            if (p_vaddr < min_addr)
                min_addr = p_vaddr;
            if (seg_end_v > max_addr)
                max_addr = seg_end_v;
        }
        have_load = true;
    }

    if (!have_load)
        return ELF_ERR_NO_LOAD;

    /* Entry point must be inside the loaded span. */
    if (eh->e_entry < min_addr || eh->e_entry >= max_addr)
        return ELF_ERR_ENTRY;

    /* Pass 2: allocate and copy. We allocate per-segment at its requested
     * physical address (AllocateAddress). Segments are page-aligned apart. */
    for (i = 0; i < phnum && result == ELF_OK; i++) {
        const uint8_t *ph_raw = data + eh->e_phoff + i * (uint64_t)phentsize;
        uint32_t p_type = rd32e(ph_raw + 0);
        uint64_t p_offset = rd64e(ph_raw + 8);
        uint64_t p_vaddr = rd64e(ph_raw + 16);
        uint64_t p_filesz = rd64e(ph_raw + 32);
        uint64_t p_memsz = rd64e(ph_raw + 40);
        uint64_t page_count, page_base, span_end;

        if (p_type != PT_LOAD_ || p_memsz == 0)
            continue;

        span_end = p_vaddr + p_memsz; /* checked in pass 1 */
        page_base = p_vaddr & ~0xFFFULL;
        {
            uint64_t span_from_page = span_end - page_base;
            page_count = (span_from_page + 0xFFF) >> 12;
        }

        void *seg_host = NULL;
        if (elf_page_map_at) {
            seg_host = elf_page_map_at(page_count, page_base);
            if (!seg_host) {
                debugf("page map at 0x%llx (%llu pages) failed",
                       page_base, page_count);
                return ELF_ERR_ALLOC;
            }
        } else {
            /* No allocator: identity pointer (UEFI without injection). */
            seg_host = (void *)(uintptr_t)page_base;
        }

        {
            uint64_t off_in_seg = p_vaddr - page_base;
            uint8_t *host_bytes = (uint8_t *)seg_host + off_in_seg;

            /* Copy file bytes. */
            if (p_filesz > 0)
                robomemcpy(host_bytes, data + p_offset, p_filesz);
            /* Zero the remainder (BSS). */
            if (span_end > p_vaddr + p_filesz)
                robomemset(host_bytes + p_filesz, 0, span_end - (p_vaddr + p_filesz));
        }
    }

    out->entry = eh->e_entry;
    out->base = min_addr;
    out->size = max_addr - min_addr;
    return result;
}
