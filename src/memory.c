#include <stdlib.h>

#include <roaring/memory.h>
#include <roaring/options.h>
#include <roaring/portability.h>

void* roaring_malloc(roaring_options_t* options, size_t n) {
    if (options != NULL && options->memory != NULL &&
        options->memory->malloc != NULL) {
        return options->memory->malloc(n, options->memory->payload);
    }
    return malloc(n);
}

void* roaring_realloc(roaring_options_t* options, void* p, size_t old_sz,
                      size_t new_sz) {
    if (options != NULL && options->memory != NULL &&
        options->memory->realloc != NULL) {
        return options->memory->realloc(p, old_sz, new_sz,
                                        options->memory->payload);
    }
    return realloc(p, new_sz);
}

void* roaring_calloc(roaring_options_t* options, size_t n_elements,
                     size_t element_size) {
    if (options != NULL && options->memory != NULL &&
        options->memory->calloc != NULL) {
        return options->memory->calloc(n_elements, element_size,
                                       options->memory->payload);
    }
    return calloc(n_elements, element_size);
}

void roaring_free(roaring_options_t* options, void* p) {
    if (options != NULL && options->memory != NULL &&
        options->memory->free != NULL) {
        return options->memory->free(p, options->memory->payload);
    }
    return free(p);
}

void* roaring_aligned_malloc(roaring_options_t* options, size_t alignment,
                             size_t size) {
    if (options != NULL && options->memory != NULL &&
        options->memory->aligned_malloc != NULL) {
        return options->memory->aligned_malloc(alignment, size,
                                               options->memory->payload);
    }
    return roaring_bitmap_aligned_malloc(alignment, size);
}

void roaring_aligned_free(roaring_options_t* options, void* p) {
    if (options != NULL && options->memory != NULL &&
        options->memory->aligned_free != NULL) {
        return options->memory->aligned_free(p, options->memory->payload);
    }
    return roaring_bitmap_aligned_free(p);
}