/*
 * memory_pressure_unit.c
 *
 * Stress CRoaring under a seeded, randomly-failing custom allocator.
 * Any bitmap that survives an operation must pass internal_validate.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER
#ifndef __cplusplus
extern int posix_memalign(void **memptr, size_t alignment, size_t size);
#endif
#endif

#include <roaring/memory.h>
#include <roaring/misc/configreport.h>
#include <roaring/roaring.h>
#include <roaring/roaring64.h>

#include "test.h"

// Roughly one in FAIL_ALLOC_MODULO allocation attempts returns NULL.
#define FAIL_ALLOC_MODULO 6u

#define MAX_LIVE_32 8
#define MAX_LIVE_64 8
#define ROUNDS_PER_SEED 400u

typedef struct {
    uint64_t prng_state;
    uint64_t alloc_calls;
    uint64_t alloc_failures;
    roaring_memory_t baseline;
    bool baseline_initialized;
} failalloc_state_t;

static failalloc_state_t g_failalloc;

static void *baseline_aligned_malloc(size_t alignment, size_t size) {
    void *p = NULL;
#ifdef _MSC_VER
    p = _aligned_malloc(size, alignment);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    p = __mingw_aligned_malloc(size, alignment);
#else
    if (posix_memalign(&p, alignment, size) != 0) {
        return NULL;
    }
#endif
    return p;
}

static void baseline_aligned_free(void *memblock) {
#ifdef _MSC_VER
    _aligned_free(memblock);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    __mingw_aligned_free(memblock);
#else
    free(memblock);
#endif
}

static void init_baseline_hook(void) {
    if (g_failalloc.baseline_initialized) {
        return;
    }
    g_failalloc.baseline.malloc = malloc;
    g_failalloc.baseline.realloc = realloc;
    g_failalloc.baseline.calloc = calloc;
    g_failalloc.baseline.free = free;
    g_failalloc.baseline.aligned_malloc = baseline_aligned_malloc;
    g_failalloc.baseline.aligned_free = baseline_aligned_free;
    g_failalloc.baseline_initialized = true;
}

static uint64_t failalloc_rand(void) {
    uint64_t x = g_failalloc.prng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g_failalloc.prng_state = x;
    return x * UINT64_C(0x2545F4914F6CDD1D);
}

static bool failalloc_should_fail(void) {
    g_failalloc.alloc_calls++;
    if ((failalloc_rand() % FAIL_ALLOC_MODULO) == 0) {
        g_failalloc.alloc_failures++;
        return true;
    }
    return false;
}

static void *failalloc_malloc(size_t n) {
    if (failalloc_should_fail()) {
        return NULL;
    }
    return g_failalloc.baseline.malloc(n);
}

static void *failalloc_realloc(void *p, size_t new_sz) {
    if (failalloc_should_fail()) {
        return NULL;
    }
    return g_failalloc.baseline.realloc(p, new_sz);
}

static void *failalloc_calloc(size_t n_elements, size_t element_size) {
    if (failalloc_should_fail()) {
        return NULL;
    }
    return g_failalloc.baseline.calloc(n_elements, element_size);
}

static void failalloc_free(void *p) { g_failalloc.baseline.free(p); }

static void *failalloc_aligned_malloc(size_t alignment, size_t size) {
    if (failalloc_should_fail()) {
        return NULL;
    }
    return g_failalloc.baseline.aligned_malloc(alignment, size);
}

static void failalloc_aligned_free(void *p) {
    g_failalloc.baseline.aligned_free(p);
}

static void install_failing_allocator(uint64_t seed) {
    init_baseline_hook();
    g_failalloc.prng_state = seed;
    g_failalloc.alloc_calls = 0;
    g_failalloc.alloc_failures = 0;
    roaring_memory_t hook = {.malloc = failalloc_malloc,
                             .realloc = failalloc_realloc,
                             .calloc = failalloc_calloc,
                             .free = failalloc_free,
                             .aligned_malloc = failalloc_aligned_malloc,
                             .aligned_free = failalloc_aligned_free};
    roaring_init_memory_hook(hook);
}

static void restore_default_allocator(void) {
    init_baseline_hook();
    roaring_init_memory_hook(g_failalloc.baseline);
}

static void assert_bitmap32_valid(const roaring_bitmap_t *r) {
    if (r == NULL) {
        return;
    }
    const char *reason = NULL;
    if (!roaring_bitmap_internal_validate(r, &reason)) {
        fail_msg("32-bit internal_validate failed: %s",
                 reason != NULL ? reason : "(no reason)");
    }
}

static void assert_bitmap64_valid(const roaring64_bitmap_t *r) {
    if (r == NULL) {
        return;
    }
    const char *reason = NULL;
    if (!roaring64_bitmap_internal_validate(r, &reason)) {
        fail_msg("64-bit internal_validate failed: %s",
                 reason != NULL ? reason : "(no reason)");
    }
}

typedef struct {
    roaring_bitmap_t *items[MAX_LIVE_32];
    size_t count;
} pool32_t;

typedef struct {
    roaring64_bitmap_t *items[MAX_LIVE_64];
    size_t count;
} pool64_t;

static bool pool32_add(pool32_t *pool, roaring_bitmap_t *r) {
    if (r == NULL) {
        return false;
    }
    if (pool->count >= MAX_LIVE_32) {
        roaring_bitmap_free(r);
        return false;
    }
    pool->items[pool->count++] = r;
    return true;
}

static bool pool64_add(pool64_t *pool, roaring64_bitmap_t *r) {
    if (r == NULL) {
        return false;
    }
    if (pool->count >= MAX_LIVE_64) {
        roaring64_bitmap_free(r);
        return false;
    }
    pool->items[pool->count++] = r;
    return true;
}

static roaring_bitmap_t *pool32_pick(const pool32_t *pool) {
    if (pool->count == 0) {
        return NULL;
    }
    return pool->items[failalloc_rand() % pool->count];
}

static roaring64_bitmap_t *pool64_pick(const pool64_t *pool) {
    if (pool->count == 0) {
        return NULL;
    }
    return pool->items[failalloc_rand() % pool->count];
}

static void pool32_validate_all(const pool32_t *pool) {
    for (size_t i = 0; i < pool->count; ++i) {
        assert_bitmap32_valid(pool->items[i]);
    }
}

static void pool64_validate_all(const pool64_t *pool) {
    for (size_t i = 0; i < pool->count; ++i) {
        assert_bitmap64_valid(pool->items[i]);
    }
}

static void pool32_clear(pool32_t *pool) {
    for (size_t i = 0; i < pool->count; ++i) {
        roaring_bitmap_free(pool->items[i]);
        pool->items[i] = NULL;
    }
    pool->count = 0;
}

static void pool64_clear(pool64_t *pool) {
    for (size_t i = 0; i < pool->count; ++i) {
        roaring64_bitmap_free(pool->items[i]);
        pool->items[i] = NULL;
    }
    pool->count = 0;
}

static void pool32_remove_random(pool32_t *pool) {
    if (pool->count == 0) {
        return;
    }
    size_t idx = (size_t)(failalloc_rand() % pool->count);
    roaring_bitmap_free(pool->items[idx]);
    pool->items[idx] = pool->items[pool->count - 1];
    pool->items[pool->count - 1] = NULL;
    pool->count--;
}

static void pool64_remove_random(pool64_t *pool) {
    if (pool->count == 0) {
        return;
    }
    size_t idx = (size_t)(failalloc_rand() % pool->count);
    roaring64_bitmap_free(pool->items[idx]);
    pool->items[idx] = pool->items[pool->count - 1];
    pool->items[pool->count - 1] = NULL;
    pool->count--;
}

static uint32_t random_u32_value(void) {
    static const uint32_t candidates[] = {
        0,          1,          42,         255,        256,
        65535,      65536,      131072,     1048576,    0x7fffffff};
    return candidates[failalloc_rand() % (sizeof(candidates) / sizeof(candidates[0]))];
}

static uint64_t random_u64_value(void) {
    static const uint64_t candidates[] = {
        0,
        1,
        42,
        0xffff,
        0x10000,
        0x100000000ULL,
        0x123456789abcdefULL,
        0xffffffffffffffffULL};
    return candidates[failalloc_rand() %
                     (sizeof(candidates) / sizeof(candidates[0]))];
}

static void exercise_bitmap32(uint64_t seed) {
    install_failing_allocator(seed);
    pool32_t pool = {0};

    for (uint32_t round = 0; round < ROUNDS_PER_SEED; ++round) {
        const uint32_t op = (uint32_t)(failalloc_rand() % 12u);

        switch (op) {
            case 0: {
                pool32_add(&pool, roaring_bitmap_create());
                break;
            }
            case 1: {
                pool32_add(&pool,
                           roaring_bitmap_create_with_capacity(
                               (uint32_t)(1u + (failalloc_rand() % 64u))));
                break;
            }
            case 2: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    uint32_t a = random_u32_value();
                    uint32_t b = random_u32_value();
                    if (a > b) {
                        uint32_t tmp = a;
                        a = b;
                        b = tmp;
                    }
                    roaring_bitmap_add(r, random_u32_value());
                    roaring_bitmap_add_range_closed(r, a, b);
                }
                break;
            }
            case 3: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    roaring_bitmap_remove(r, random_u32_value());
                }
                break;
            }
            case 4: {
                if (pool.count >= 2) {
                    roaring_bitmap_t *a = pool.items[0];
                    roaring_bitmap_t *b = pool.items[1];
                    roaring_bitmap_t *result = roaring_bitmap_or(a, b);
                    if (result != NULL) {
                        assert_bitmap32_valid(result);
                        pool32_add(&pool, result);
                    }
                }
                break;
            }
            case 5: {
                if (pool.count >= 2) {
                    roaring_bitmap_t *a = pool.items[0];
                    roaring_bitmap_t *b = pool.items[1];
                    roaring_bitmap_t *result = roaring_bitmap_and(a, b);
                    if (result != NULL) {
                        assert_bitmap32_valid(result);
                        pool32_add(&pool, result);
                    }
                }
                break;
            }
            case 6: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    roaring_bitmap_t *copy = roaring_bitmap_copy(r);
                    pool32_add(&pool, copy);
                }
                break;
            }
            case 7: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    size_t sz = roaring_bitmap_portable_size_in_bytes(r);
                    char *buf = (char *)malloc(sz);
                    if (buf != NULL) {
                        roaring_bitmap_portable_serialize(r, buf);
                        roaring_bitmap_t *deser =
                            roaring_bitmap_portable_deserialize_safe(buf, sz);
                        if (deser != NULL) {
                            assert_bitmap32_valid(deser);
                            pool32_add(&pool, deser);
                        }
                        free(buf);
                    }
                }
                break;
            }
            case 8: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    roaring_bitmap_shrink_to_fit(r);
                    roaring_bitmap_run_optimize(r);
                }
                break;
            }
            case 9: {
                pool32_remove_random(&pool);
                break;
            }
            case 10: {
                if (pool.count >= 2) {
                    const roaring_bitmap_t *args[2] = {pool.items[0],
                                                       pool.items[1]};
                    roaring_bitmap_t *result =
                        roaring_bitmap_or_many_heap(2, args);
                    if (result != NULL) {
                        assert_bitmap32_valid(result);
                        pool32_add(&pool, result);
                    }
                }
                break;
            }
            default: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    roaring_uint32_iterator_t *it =
                        roaring_iterator_create(r);
                    if (it != NULL) {
                        roaring_uint32_iterator_free(it);
                    }
                }
                break;
            }
        }

        pool32_validate_all(&pool);
    }

    pool32_clear(&pool);
    restore_default_allocator();

    assert_true(g_failalloc.alloc_failures > 0);
}

static void exercise_bitmap64(uint64_t seed) {
    install_failing_allocator(seed);
    pool64_t pool = {0};

    for (uint32_t round = 0; round < ROUNDS_PER_SEED; ++round) {
        const uint32_t op = (uint32_t)(failalloc_rand() % 12u);

        switch (op) {
            case 0: {
                pool64_add(&pool, roaring64_bitmap_create());
                break;
            }
            case 1: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    roaring64_bitmap_add(r, random_u64_value());
                    uint64_t a = random_u64_value();
                    uint64_t b = random_u64_value();
                    if (a > b) {
                        uint64_t tmp = a;
                        a = b;
                        b = tmp;
                    }
                    roaring64_bitmap_add_range_closed(r, a, b);
                }
                break;
            }
            case 2: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    roaring64_bitmap_remove(r, random_u64_value());
                }
                break;
            }
            case 3: {
                if (pool.count >= 2) {
                    roaring64_bitmap_t *a = pool.items[0];
                    roaring64_bitmap_t *b = pool.items[1];
                    roaring64_bitmap_t *result = roaring64_bitmap_or(a, b);
                    if (result != NULL) {
                        assert_bitmap64_valid(result);
                        pool64_add(&pool, result);
                    }
                }
                break;
            }
            case 4: {
                if (pool.count >= 2) {
                    roaring64_bitmap_t *a = pool.items[0];
                    roaring64_bitmap_t *b = pool.items[1];
                    roaring64_bitmap_t *result = roaring64_bitmap_and(a, b);
                    if (result != NULL) {
                        assert_bitmap64_valid(result);
                        pool64_add(&pool, result);
                    }
                }
                break;
            }
            case 5: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    roaring64_bitmap_t *copy = roaring64_bitmap_copy(r);
                    pool64_add(&pool, copy);
                }
                break;
            }
            case 6: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    size_t sz = roaring64_bitmap_portable_size_in_bytes(r);
                    char *buf = (char *)malloc(sz);
                    if (buf != NULL) {
                        roaring64_bitmap_portable_serialize(r, buf);
                        roaring64_bitmap_t *deser =
                            roaring64_bitmap_portable_deserialize_safe(buf,
                                                                       sz);
                        if (deser != NULL) {
                            assert_bitmap64_valid(deser);
                            pool64_add(&pool, deser);
                        }
                        free(buf);
                    }
                }
                break;
            }
            case 7: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    roaring64_bitmap_shrink_to_fit(r);
                    roaring64_bitmap_run_optimize(r);
                }
                break;
            }
            case 8: {
                pool64_remove_random(&pool);
                break;
            }
            case 9: {
                if (pool.count >= 2) {
                    roaring64_bitmap_t *a = pool.items[0];
                    roaring64_bitmap_t *b = pool.items[1];
                    roaring64_bitmap_t *result = roaring64_bitmap_xor(a, b);
                    if (result != NULL) {
                        assert_bitmap64_valid(result);
                        pool64_add(&pool, result);
                    }
                }
                break;
            }
            case 10: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    roaring64_bitmap_t *flipped =
                        roaring64_bitmap_flip_closed(r, random_u64_value(),
                                                     random_u64_value());
                    if (flipped != NULL) {
                        assert_bitmap64_valid(flipped);
                        pool64_add(&pool, flipped);
                    }
                }
                break;
            }
            default: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    roaring64_iterator_t *it = roaring64_iterator_create(r);
                    if (it != NULL) {
                        roaring64_iterator_free(it);
                    }
                }
                break;
            }
        }

        pool64_validate_all(&pool);
    }

    pool64_clear(&pool);
    restore_default_allocator();

    assert_true(g_failalloc.alloc_failures > 0);
}

DEFINE_TEST(test_memory_pressure_roaring32) {
    static const uint64_t seeds[] = {1, 42, 0xC0FFEE, 0xDEADBEEFCAFEBABEULL};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_bitmap32(seeds[i]);
    }
}

DEFINE_TEST(test_memory_pressure_roaring64) {
    static const uint64_t seeds[] = {7, 99, 0xBADC0DE, 0x0123456789ABCDEFULL};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_bitmap64(seeds[i]);
    }
}

int main(void) {
    tellmeall();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_memory_pressure_roaring32),
        cmocka_unit_test(test_memory_pressure_roaring64),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}