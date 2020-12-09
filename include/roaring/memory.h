#ifndef MEMORY_H
#define MEMORY_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>  // for size_t

#ifndef ENABLECMM
#define ROARING_MALLOC(options, n) malloc(n)
#define ROARING_REALLOC(options, p, old_sz, new_sz) realloc(p, new_sz)
#define ROARING_CALLOC(options, n_elements, element_size) calloc(n_elements, element_size)
#define ROARING_FREE(options, p) free(p)
#define ROARING_ALIGNED_MALLOC(options, alignment, size) roaring_bitmap_aligned_malloc(alignment, size)
#define ROARING_ALIGNED_FREE(options, p) roaring_bitmap_aligned_free(p)
#else
#define ROARING_MALLOC(options, n) roaring_malloc(options, n)
#define ROARING_REALLOC(options, p, old_sz, new_sz) roaring_realloc(options, p, old_sz, new_sz)
#define ROARING_CALLOC(options, n_elements, element_size) roaring_calloc(options, n_elements, element_size)
#define ROARING_FREE(options, p) roaring_free(options, p)
#define ROARING_ALIGNED_MALLOC(options, alignment, size) roaring_aligned_malloc(options, alignment, size)
#define ROARING_ALIGNED_FREE(options, p) roaring_aligned_free(options, p)
#endif


struct roaring_options_s;  // forward declaration
typedef struct roaring_options_s roaring_options_t;

typedef void* (*p_roaring_malloc)(size_t, void*);
typedef void* (*p_roaring_realloc)(void*, size_t, size_t, void*);
typedef void* (*p_roaring_calloc)(size_t, size_t, void*);
typedef void (*p_roaring_free)(void*, void*);
typedef void* (*p_roaring_aligned_malloc)(size_t, size_t, void*);
typedef void (*p_roaring_aligned_free)(void*, void*);

struct roaring_memory_s {
    p_roaring_malloc malloc;
    p_roaring_realloc realloc;
    p_roaring_calloc calloc;
    p_roaring_free free;
    p_roaring_aligned_malloc aligned_malloc;
    p_roaring_aligned_free aligned_free;
    void* payload;
};

typedef struct roaring_memory_s roaring_memory_t;

void* roaring_malloc(roaring_options_t*, size_t);
void* roaring_realloc(roaring_options_t*, void*, size_t, size_t);
void* roaring_calloc(roaring_options_t*, size_t, size_t);
void roaring_free(roaring_options_t*, void*);
void* roaring_aligned_malloc(roaring_options_t*, size_t, size_t);
void roaring_aligned_free(roaring_options_t*, void*);

#ifdef __cplusplus
}
#endif

#endif