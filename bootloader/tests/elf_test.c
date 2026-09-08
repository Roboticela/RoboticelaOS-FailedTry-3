/* Host unit tests for the ELF64 loader.
 *
 * The allocator is simulated: a 64MB "RAM" arena starting at 1MB, so
 * AllocateAddress semantics can be mimicked deterministically.
 */
#include "elf/elf_loader.h"
#include "lib.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) { printf("  ok  %s\n", msg); }                          \
        else { printf("FAIL  %s (line %d)\n", msg, __LINE__); failures++; } \
    } while (0)

#define ARENA_BASE 0x100000ull            /* 1MB, matches kernel vaddrs */
#define ARENA_SIZE (64ull * 1024 * 1024)
static uint8_t arena[ARENA_SIZE];

static void *sim_map_at(uint64_t pages, uint64_t want)
{
    if (want < ARENA_BASE || want + pages * 4096 > ARENA_BASE + ARENA_SIZE)
        return NULL;
    return arena + (want - ARENA_BASE); /* physical -> arena offset */
}
static void sim_free(uint64_t phys, uint64_t pages)
{
    (void)phys; (void)pages;
}

/* --- ELF builders --- */
typedef struct {
    uint8_t *buf;
    uint64_t size;
} blob;

static void put16(uint8_t *p, uint16_t v){ p[0]=v; p[1]=v>>8; }
static void put32(uint8_t *p, uint32_t v){ p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
static void put64(uint8_t *p, uint64_t v){ put32(p, (uint32_t)v); put32(p+4, (uint32_t)(v>>32)); }

static blob make_elf(uint16_t type, uint16_t machine, int n_load,
                     uint64_t vaddr, uint64_t entry_override, int entry_override_on)
{
    /* ehdr(64) + 1 phdr(56) + data */
    uint64_t phnum = n_load > 0 ? 1 : 0;
    uint64_t size = 64 + phnum * 56 + 64;
    uint8_t *b = calloc(1, (size_t)size);
    blob out = { b, size };

    memcpy(b, "\x7F""ELF", 4);
    b[4] = 2;  /* 64-bit */
    b[5] = 1;  /* LSB */
    b[6] = 1;  /* version */
    put16(b+16, type);
    put16(b+18, machine);
    put32(b+20, 1);
    put64(b+24, entry_override_on ? entry_override : vaddr);
    put64(b+32, 64);      /* phoff */
    put16(b+52, 64);      /* ehsize */
    put16(b+54, 56);      /* phentsize */
    put16(b+56, (uint16_t)phnum);

    if (phnum) {
        uint8_t *ph = b + 64;
        put32(ph+0, 1);            /* PT_LOAD */
        put32(ph+4, 5);            /* R+X */
        put64(ph+8, 64 + 56);      /* offset (data right after phdr) */
        put64(ph+16, vaddr);       /* vaddr */
        put64(ph+24, vaddr);       /* paddr */
        put64(ph+32, 32);          /* filesz */
        put64(ph+40, 64);          /* memsz (32 bytes BSS tail) */
        put64(ph+48, 4096);        /* align */
    }
    return out;
}

int main(void)
{
    elf_set_allocators(sim_map_at, sim_free);
    printf("elf loader tests\n");

    /* valid ELF loads and zeroes BSS */
    {
        blob e = make_elf(2, 62, 1, 0x100000, 0, 0);
        elf_load_result_t r;
        elf_result_t er = elf_load(e.buf, e.size, &r);
        CHECK(er == ELF_OK, "valid ELF loads");
        if (er == ELF_OK) {
            CHECK(r.entry == 0x100000, "entry parsed");
            CHECK(r.base == 0x100000, "base parsed");
            CHECK(r.size == 64, "span covers memsz");
            /* segment file data landed in the arena via the mapping */
            CHECK(robomemcmp(arena, e.buf + 64 + 56, 32) == 0, "file bytes copied");
            /* BSS tail zeroed */
            int all_zero = 1;
            for (uint64_t i = 32; i < 64; i++)
                if (arena[i]) { all_zero = 0; break; }
            CHECK(all_zero, "BSS zeroed");
        }
        free(e.buf);
    }

    /* bad magic */
    {
        blob e = make_elf(2, 62, 1, 0x100000, 0, 0);
        e.buf[0] = 'B';
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_MAGIC, "bad magic rejected");
        free(e.buf);
    }

    /* 32-bit class */
    {
        blob e = make_elf(2, 62, 1, 0x100000, 0, 0);
        e.buf[4] = 1;
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_CLASS, "32-bit rejected");
        free(e.buf);
    }

    /* wrong machine */
    {
        blob e = make_elf(2, 40, 1, 0x100000, 0, 0); /* ARM */
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_MACHINE, "wrong machine rejected");
        free(e.buf);
    }

    /* wrong type (ET_REL) */
    {
        blob e = make_elf(1, 62, 1, 0x100000, 0, 0);
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_TYPE, "relocatable rejected");
        free(e.buf);
    }

    /* missing PT_LOAD */
    {
        blob e = make_elf(2, 62, 0, 0x100000, 0, 0);
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_NO_LOAD, "no PT_LOAD rejected");
        free(e.buf);
    }

    /* program header table out of bounds */
    {
        blob e = make_elf(2, 62, 1, 0x100000, 0, 0);
        put64(e.buf+32, 1000000); /* phoff far beyond file */
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_PHDR_BOUNDS, "phdr OOB rejected");
        free(e.buf);
    }

    /* memsz < filesz */
    {
        blob e = make_elf(2, 62, 1, 0x100000, 0, 0);
        uint8_t *ph = e.buf + 64;
        put64(ph+40, 8); /* memsz smaller than filesz=32 */
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_SEGMENT, "memsz<filesz rejected");
        free(e.buf);
    }

    /* segment file range out of file */
    {
        blob e = make_elf(2, 62, 1, 0x100000, 0, 0);
        uint8_t *ph = e.buf + 64;
        put64(ph+32, 0xFFFFF); /* filesz larger than file */
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_SEGMENT, "filesz OOB rejected");
        free(e.buf);
    }

    /* entry outside loaded segments */
    {
        blob e = make_elf(2, 62, 1, 0x100000, 0x500000, 1);
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_ENTRY, "entry outside span rejected");
        free(e.buf);
    }

    /* segment below 1MB */
    {
        blob e = make_elf(2, 62, 1, 0x1000, 0, 0);
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_ADDRESS, "low address rejected");
        free(e.buf);
    }

    /* paddr != vaddr (Stage 01 identity-only) */
    {
        blob e = make_elf(2, 62, 1, 0x100000, 0, 0);
        uint8_t *ph = e.buf + 64;
        put64(ph+24, 0x900000); /* different paddr */
        elf_load_result_t r;
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_ADDRESS, "non-identity paddr rejected");
        free(e.buf);
    }

    /* allocation failure (address outside sim arena) */
    {
        blob e = make_elf(2, 62, 1, 0x40000000, 0, 0); /* 1GB: outside sim */
        elf_load_result_t r;
        /* validation passes; alloc sim returns 0 -> ELF_ERR_ALLOC */
        CHECK(elf_load(e.buf, e.size, &r) == ELF_ERR_ALLOC, "alloc failure surfaces");
        free(e.buf);
    }

    if (failures) { printf("%d FAILURES\n", failures); return 1; }
    printf("all elf tests passed\n");
    return 0;
}
