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
 * Throughout the library, a non-NULL pointer returned by roaring_malloc,
 * roaring_realloc, roaring_calloc, or roaring_aligned_malloc (or by the
 * corresponding functions installed via roaring_init_memory_hook) is treated
 * as usable memory: CRoaring will read and write the full requested size
 * without further checks, as if the storage were committed.
 *
 * This is a simplifying assumption, not a guarantee of the underlying
 * platform. On systems with overcommit, demand paging, lazy commit, or
 * similar behavior, a non-NULL pointer does not necessarily mean that every
 * byte is backed by physical storage or that access cannot fail later (for
 * example with SIGBUS or the OOM killer). CRoaring does not probe or fault-in
 * memory after allocation.
 *
 * Custom allocators hooked into this layer should return NULL on failure. If
 * they return non-NULL, they should provide memory that is safe to use
 * immediately for the requested size; otherwise library behavior is undefined.
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
