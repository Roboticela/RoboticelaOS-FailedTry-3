/* robobootloader - minimal CRT shims for the PE/COFF link.
 *
 * clang targets MSVC when building UEFI binaries; any translation unit that
 * touches floating point pulls in references to `_fltused` (normally provided
 * by the MSVC CRT, which freestanding UEFI code does not link). We don't use
 * floats, but the double literals in the renderer layout arithmetic still
 * generate FP code, so define the marker ourselves.
 */
#include <stdint.h>

#ifdef _MSC_VER
int _fltused = 1;
#endif

/* clang may emit __chkstk calls for large stack frames on Windows targets. */
void __chkstk(void);
void __chkstk(void) {}

/* Some toolchain configurations reference memset/memcpy intrinsics even with
 * -fno-builtin (e.g. struct copies); route them to our helpers. */
void *memset(void *dst, int value, unsigned long long count);
void *memset(void *dst, int value, unsigned long long count)
{
    extern void *robomemset(void *, uint8_t, uint64_t);
    return robomemset(dst, (uint8_t)value, count);
}

void *memcpy(void *dst, const void *src, unsigned long long count);
void *memcpy(void *dst, const void *src, unsigned long long count)
{
    extern void *robomemcpy(void *, const void *, uint64_t);
    return robomemcpy(dst, src, count);
}
