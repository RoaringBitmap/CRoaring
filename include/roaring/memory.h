#ifndef INCLUDE_ROARING_MEMORY_H_
#define INCLUDE_ROARING_MEMORY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>  // for size_t

typedef void* (*p_roaring_malloc)(size_t);
typedef void* (*p_roaring_realloc)(void*, size_t);
typedef void* (*p_roaring_calloc)(size_t, size_t);
typedef void (*p_roaring_free)(void*);
typedef void* (*p_roaring_aligned_malloc)(size_t, size_t);
typedef void (*p_roaring_aligned_free)(void*);

typedef struct roaring_memory_s {
    p_roaring_malloc malloc;
    p_roaring_realloc realloc;
    p_roaring_calloc calloc;
    p_roaring_free free;
    p_roaring_aligned_malloc aligned_malloc;
    p_roaring_aligned_free aligned_free;
} roaring_memory_t;

void init_roaring_memory_hook(roaring_memory_t memory_hook);

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
