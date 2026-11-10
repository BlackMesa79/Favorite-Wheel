/* Force-included into every PlutoVG translation unit (see xmake.lua).
 *
 * PlutoVG v1.3.1 writes through malloc/realloc results without checking them
 * (plutovg-utils.h plutovg_array_ensure, path/canvas/state/outline creation,
 * the FreeType-derived stroker). A NULL from realloc followed by memcpy at
 * data + size can write to an arbitrary address instead of faulting on the
 * null page. SwfRaster's per-Render budgets keep every input-dependent request
 * to a few MiB, so failure here means the game process itself is out of
 * memory. Terminate deterministically (fail-fast, no unwinding through C)
 * rather than continue with a corrupted heap.
 */
#ifndef WHEEL_PLUTOVG_CHECKED_ALLOC_H
#define WHEEL_PLUTOVG_CHECKED_ALLOC_H
#include <stdlib.h>
#include <string.h>
#include <intrin.h>

static __inline void* wheel_plutovg_checked(void* p)
{
    if(p == NULL)
        __fastfail(7 /* FAST_FAIL_FATAL_APP_EXIT */);
    return p;
}
static __inline void* wheel_plutovg_malloc(size_t n) { return wheel_plutovg_checked(malloc(n ? n : 1)); }
static __inline void* wheel_plutovg_calloc(size_t c, size_t n) { return wheel_plutovg_checked(calloc(c ? c : 1, n ? n : 1)); }
static __inline void* wheel_plutovg_realloc(void* p, size_t n) { return wheel_plutovg_checked(realloc(p, n ? n : 1)); }

/* stdlib.h is already included above, so these only affect PlutoVG's calls. */
#define malloc(n) wheel_plutovg_malloc(n)
#define calloc(c, n) wheel_plutovg_calloc(c, n)
#define realloc(p, n) wheel_plutovg_realloc(p, n)
#endif
