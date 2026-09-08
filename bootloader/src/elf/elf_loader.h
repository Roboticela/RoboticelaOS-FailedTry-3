/* robobootloader - ELF64 x86-64 kernel loader.
 *
 * Loads a statically linked ELF64 executable for x86-64:
 *   - validates magic, class, endianness, version, machine, type,
 *   - validates program header table bounds and entry sizes,
 *   - loads every PT_LOAD segment into freshly allocated EfiLoaderData pages,
 *   - zeroes the p_memsz > p_filesz tail (BSS),
 *   - computes the physical entry point and total memory span.
 *
 * All header-supplied counts/offsets/sizes are bounds-checked against the
 * actual file size; every arithmetic combination is overflow-checked. A
 * malformed kernel can never corrupt memory or execute garbage.
 */
#ifndef ROBO_ELF_LOADER_H
#define ROBO_ELF_LOADER_H

#include <stdint.h>

typedef enum {
    ELF_OK = 0,
    ELF_ERR_PARAM,
    ELF_ERR_TOO_SMALL,      /* file smaller than ELF header */
    ELF_ERR_MAGIC,
    ELF_ERR_CLASS,          /* not ELFCLASS64 */
    ELF_ERR_ENDIAN,         /* not little-endian */
    ELF_ERR_VERSION,        /* bad EI_VERSION or e_version */
    ELF_ERR_MACHINE,        /* not EM_X86_64 */
    ELF_ERR_TYPE,           /* not ET_EXEC */
    ELF_ERR_HEADER_SIZE,    /* e_phentsize/phoff impossible */
    ELF_ERR_PHDR_BOUNDS,    /* program header table out of file bounds */
    ELF_ERR_NO_LOAD,        /* no PT_LOAD segments */
    ELF_ERR_ENTRY,          /* entry point outside any loaded segment */
    ELF_ERR_OVERFLOW,       /* any size/address arithmetic overflowed */
    ELF_ERR_SEGMENT,        /* bad segment: file range OOB, memsz < filesz, etc. */
    ELF_ERR_ALLOC,          /* page allocation failed */
    ELF_ERR_ADDRESS         /* segment vaddr unsafe (e.g. below 1MB, or wraps) */
} elf_result_t;

typedef struct {
    uint64_t entry;         /* physical entry point */
    uint64_t base;          /* lowest byte loaded */
    uint64_t size;          /* span from base to highest loaded byte */
} elf_load_result_t;

/* Load the ELF image from [data, data+size). Allocation goes through the
 * injected page allocator (UEFI boot services in the firmware build). */
elf_result_t elf_load(const uint8_t *data, uint64_t size, elf_load_result_t *out);

/* Allocator injection: reserve `pages` pages at physical `want_phys` and
 * return a host pointer the loader may read/write (identity on UEFI; an arena
 * offset in host tests). NULL on failure. Frees by physical address. */
void elf_set_allocators(void *(*page_map_at)(uint64_t pages, uint64_t want_phys),
                        void (*page_free)(uint64_t phys, uint64_t pages));

/* Parse-only validation used by tests: everything except allocation. */
elf_result_t elf_validate(const uint8_t *data, uint64_t size);

#endif /* ROBO_ELF_LOADER_H */
