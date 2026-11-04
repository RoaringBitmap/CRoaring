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
 */
#ifndef INCLUDE_ROARING_MEMORY_H_
#define INCLUDE_ROARING_MEMORY_H_

#include <stdbool.h>
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

// Internal: shrinks p to size bytes, freeing it at 0 as realloc(p, 0) varies.
// Returns the block to keep, p if realloc fails; *shrunk: whether it shrank.
static inline void* croaring_realloc_shrink(void* p, size_t size,
                                            bool* shrunk) {
    if (size == 0) {
        roaring_free(p);
        *shrunk = true;
        return NULL;
    }
    void* q = roaring_realloc(p, size);
    *shrunk = q != NULL;
    return q != NULL ? q : p;
}

#ifdef __cplusplus
}
#endif

#endif  // INCLUDE_ROARING_MEMORY_H_
