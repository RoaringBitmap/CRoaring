//
// Memory hooks for tests replacing some of the allocation functions: the
// others are the C library's, as CRoaring's defaults are.
//

#ifndef TESTS_TEST_MEMORY_HOOK_H_
#define TESTS_TEST_MEMORY_HOOK_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#include <malloc.h>
#endif

#include <roaring/memory.h>

#if !defined(__cplusplus) && !defined(_MSC_VER) && !defined(__MINGW32__) && \
    !defined(__MINGW64__)
extern int posix_memalign(void **memptr, size_t alignment, size_t size);
#endif

static inline void *test_aligned_malloc(size_t alignment, size_t size) {
    void *p;
#ifdef _MSC_VER
    p = _aligned_malloc(size, alignment);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    p = __mingw_aligned_malloc(size, alignment);
#else
    if (posix_memalign(&p, alignment, size) != 0) return NULL;
#endif
    return p;
}

static inline void test_aligned_free(void *p) {
#ifdef _MSC_VER
    _aligned_free(p);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    __mingw_aligned_free(p);
#else
    free(p);
#endif
}

// Installs these functions with the C library's aligned ones.
static inline void test_set_memory_hook(roaring_malloc_p malloc_fn,
                                        roaring_realloc_p realloc_fn,
                                        roaring_calloc_p calloc_fn,
                                        roaring_free_p free_fn) {
    roaring_memory_t hooks = {malloc_fn,           realloc_fn,
                              calloc_fn,           free_fn,
                              test_aligned_malloc, test_aligned_free};
    roaring_init_memory_hook(hooks);
}

static inline void test_reset_memory_hook(void) {
    test_set_memory_hook(malloc, realloc, calloc, free);
}

// The teardown of every test installing a hook: it also runs when the test
// fails or crashes, so the next tests get the default allocator.
static inline int test_reset_memory_hook_teardown(void **state) {
    (void)state;
    test_reset_memory_hook();
    return 0;
}

// What the tracking allocator holds, so a test reads what a call gave back.
// Aligned blocks are the C library's and not counted.
typedef struct {
    size_t blocks;
    size_t bytes;
    // Malloc, calloc and realloc calls since test_fail_allocations; those
    // numbered failing_first to failing_last, from 1, fail.
    size_t allocations;
    size_t failing_first;
    size_t failing_last;
} test_memory_t;

static inline test_memory_t *test_memory(void) {
    static test_memory_t memory;
    return &memory;
}

// Each block starts with its size, padded to 16 bytes to keep malloc's
// alignment.
typedef union {
    size_t size;
    unsigned char padding[16];
} test_block_header_t;

static inline int test_allocation_fails(void) {
    test_memory_t *m = test_memory();
    ++m->allocations;
    return m->allocations >= m->failing_first &&
           m->allocations <= m->failing_last;
}

static inline void *test_tracking_malloc(size_t size) {
    if (test_allocation_fails()) return NULL;
    test_block_header_t *h =
        (test_block_header_t *)malloc(sizeof(test_block_header_t) + size);
    if (h == NULL) return NULL;
    h->size = size;
    test_memory()->blocks++;
    test_memory()->bytes += size;
    return h + 1;
}

static inline void *test_tracking_calloc(size_t count, size_t size) {
    if (size != 0 && count > SIZE_MAX / size) return NULL;
    void *p = test_tracking_malloc(count * size);
    if (p != NULL) memset(p, 0, count * size);
    return p;
}

// At 0 bytes the block stays, as on macOS (C also allows freeing it).
static inline void *test_tracking_realloc(void *p, size_t size) {
    if (p == NULL) return test_tracking_malloc(size);
    if (test_allocation_fails()) return NULL;
    test_block_header_t *h = (test_block_header_t *)p - 1;
    size_t old_size = h->size;
    h = (test_block_header_t *)realloc(h, sizeof(test_block_header_t) + size);
    if (h == NULL) return NULL;
    h->size = size;
    test_memory()->bytes += size - old_size;
    return h + 1;
}

static inline void test_tracking_free(void *p) {
    if (p == NULL) return;
    test_block_header_t *h = (test_block_header_t *)p - 1;
    test_memory()->blocks--;
    test_memory()->bytes -= h->size;
    free(h);
}

// Fails the allocations numbered first to last, from 1, after this call;
// (0, 0) fails none.
static inline void test_fail_allocations(size_t first, size_t last) {
    test_memory_t *m = test_memory();
    m->allocations = 0;
    m->failing_first = first;
    m->failing_last = last;
}

// Installs the tracking allocator, holding nothing and failing nothing. Free
// what it allocates before the hook is reset.
static inline void test_track_memory(void) {
    test_memory_t *m = test_memory();
    m->blocks = 0;
    m->bytes = 0;
    test_fail_allocations(0, 0);
    test_set_memory_hook(test_tracking_malloc, test_tracking_realloc,
                         test_tracking_calloc, test_tracking_free);
}

#endif  // TESTS_TEST_MEMORY_HOOK_H_
