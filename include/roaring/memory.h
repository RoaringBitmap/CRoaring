/*
 * memory.h
 *
 * This header defines CRoaring's memory-allocation abstraction layer. It
 * declares the function pointer types and hook structure used to override the
 * library's malloc/realloc/calloc/free and aligned allocation routines, along
 * with the wrapper functions used throughout the codebase.
 *
 * This allows applications to integrate CRoaring with custom allocators,
 * memory trackers, arenas, or platform-specific aligned allocation policies
 * without changing the rest of the library code.
 *
 * ## Memory allocation policy
 *
 *  We recommend halting your system when allocations are required
 * but no longer possible. Nevertheless, we aim to maintain a consistent
 * approach even in low-memory conditions.
 *
 *
 * Throughout CRoaring, a non-NULL pointer from `roaring_malloc`,
 * `roaring_realloc`, `roaring_calloc`, or `roaring_aligned_malloc` (or from the
 * functions you install with `roaring_init_memory_hook`) is treated as usable
 * memory: the library reads and writes the full requested size without further
 * checks, as if the storage were committed.
 *
 * This is a simplifying assumption. A non-NULL return value does not mean that
 * every byte is backed by physical storage or that access cannot fail later.
 * Thus, under low memory condition, an out-of-memory condition typically
 * resulting in a crash may still occur. Thankfully, you may change this
 * behavior by providing your own allocation functions.
 *
 *
 * Our policy is that even random memory failures should still preserve a
 * consistent internal state within the Roaring bitmaps. In particular, CRoaring
 * keeps each live bitmap passing `roaring_bitmap_internal_validate` or
 * `roaring64_bitmap_internal_validate`.
 *
 * However, allocation failures may leave the bitmap in a state that no longer
 * matches the intended mathematical result if the requested operations could
 * not be completed. For this reason, trying to continue running after running
 * out of memory is often worse than aborting the process.
 */
#ifndef INCLUDE_ROARING_MEMORY_H_
#define INCLUDE_ROARING_MEMORY_H_

#include <stddef.h>  // for size_t

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*roaring_malloc_p)(size_t);
typedef void* (*roaring_realloc_p)(void*, size_t);
typedef void* (*roaring_calloc_p)(size_t, size_t);
typedef void (*roaring_free_p)(void*);
typedef void* (*roaring_aligned_malloc_p)(size_t, size_t);
typedef void (*roaring_aligned_free_p)(void*);

typedef struct roaring_memory_s {
    roaring_malloc_p malloc;
    roaring_realloc_p realloc;
    roaring_calloc_p calloc;
    roaring_free_p free;
    roaring_aligned_malloc_p aligned_malloc;
    roaring_aligned_free_p aligned_free;
} roaring_memory_t;

void roaring_init_memory_hook(roaring_memory_t memory_hook);

void* roaring_malloc(size_t);
void* roaring_realloc(void*, size_t);
void* roaring_calloc(size_t, size_t);
void roaring_free(void*);
void* roaring_aligned_malloc(size_t, size_t);
void roaring_aligned_free(void*);

#ifdef __cplusplus
}
#endif

#endif  // INCLUDE_ROARING_MEMORY_H_
