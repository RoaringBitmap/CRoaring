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

// Roughly one in g_failalloc_modulo allocation attempts returns NULL.
#define FAIL_ALLOC_MODULO 6u

static uint32_t g_failalloc_modulo = FAIL_ALLOC_MODULO;

// Keep live bitmaps few and small so this test stays lightweight even when
// allocations succeed under the seeded failing hook.
#define MAX_LIVE_32 4
#define MAX_LIVE_64 4
#define ROUNDS_PER_SEED 120u
#define NUM_OPS_32 29u
#define NUM_OPS_64 25u
#define MAX_RANGE_SPAN_32 48u
#define MAX_RANGE_SPAN_64 48u
#define MAX_FROM_RANGE_ELEMS 64u

typedef struct {
    uint64_t prng_state;
    uint64_t alloc_calls;
    uint64_t alloc_failures;
    uint64_t fail_nth;  // 0 = disabled; else fail this 1-based attempt
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
    if (g_failalloc.fail_nth != 0 &&
        g_failalloc.alloc_calls == g_failalloc.fail_nth) {
        g_failalloc.alloc_failures++;
        return true;
    }
    if (g_failalloc.fail_nth == 0 &&
        (failalloc_rand() % g_failalloc_modulo) == 0) {
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

static void install_failing_allocator_modulo(uint64_t seed, uint32_t modulo) {
    init_baseline_hook();
    g_failalloc_modulo = modulo;
    g_failalloc.fail_nth = 0;
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

static void install_failing_allocator(uint64_t seed) {
    install_failing_allocator_modulo(seed, FAIL_ALLOC_MODULO);
}

static void install_fail_nth_allocator(uint64_t fail_nth) {
    init_baseline_hook();
    g_failalloc_modulo = FAIL_ALLOC_MODULO;
    g_failalloc.fail_nth = fail_nth;
    g_failalloc.prng_state = 1;
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

static bool pool32_pick_two(const pool32_t *pool, roaring_bitmap_t **a,
                            roaring_bitmap_t **b) {
    if (pool->count < 2) {
        return false;
    }
    *a = pool->items[failalloc_rand() % pool->count];
    *b = pool->items[failalloc_rand() % pool->count];
    return true;
}

static roaring_bitmap_t *pool32_copy_for_inplace(const pool32_t *pool) {
    roaring_bitmap_t *src = pool32_pick(pool);
    if (src == NULL) {
        return NULL;
    }
    return roaring_bitmap_copy(src);
}

static roaring64_bitmap_t *pool64_pick(const pool64_t *pool) {
    if (pool->count == 0) {
        return NULL;
    }
    return pool->items[failalloc_rand() % pool->count];
}

static bool pool64_pick_two(const pool64_t *pool, roaring64_bitmap_t **a,
                            roaring64_bitmap_t **b) {
    if (pool->count < 2) {
        return false;
    }
    *a = pool->items[failalloc_rand() % pool->count];
    *b = pool->items[failalloc_rand() % pool->count];
    return true;
}

static roaring64_bitmap_t *pool64_copy_for_inplace(const pool64_t *pool) {
    roaring64_bitmap_t *src = pool64_pick(pool);
    if (src == NULL) {
        return NULL;
    }
    return roaring64_bitmap_copy(src);
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
    static const uint32_t candidates[] = {0, 1, 7, 42, 255, 256, 511, 1023};
    return candidates[failalloc_rand() %
                      (sizeof(candidates) / sizeof(candidates[0]))];
}

static uint64_t random_u64_value(void) {
    static const uint64_t candidates[] = {
        0, 1, 7, 42, 255, 256, 511, 4095, 0x10000ULL, 0x10001ULL};
    return candidates[failalloc_rand() %
                      (sizeof(candidates) / sizeof(candidates[0]))];
}

static void random_u32_closed_range(uint32_t *min_out, uint32_t *max_out) {
    const uint32_t min = (uint32_t)(failalloc_rand() % 1024u);
    const uint32_t span = 1u + (uint32_t)(failalloc_rand() % MAX_RANGE_SPAN_32);
    *min_out = min;
    *max_out = min + span;
}

static void random_u64_closed_range(uint64_t *min_out, uint64_t *max_out) {
    const uint64_t min = failalloc_rand() % 4096u;
    const uint64_t span = 1u + (failalloc_rand() % MAX_RANGE_SPAN_64);
    *min_out = min;
    *max_out = min + span;
}

static void random_from_range_params32(uint64_t *min_out, uint64_t *max_out,
                                       uint32_t *step_out) {
    const uint64_t min = failalloc_rand() % 2048u;
    const uint64_t span = 1u + (failalloc_rand() % MAX_FROM_RANGE_ELEMS);
    *min_out = min;
    *max_out = min + span;
    *step_out = 1u + (uint32_t)(failalloc_rand() % 4u);
}

static void random_from_range_params64(uint64_t *min_out, uint64_t *max_out,
                                       uint32_t *step_out) {
    const uint64_t min = failalloc_rand() % 8192u;
    const uint64_t span = 1u + (failalloc_rand() % MAX_FROM_RANGE_ELEMS);
    *min_out = min;
    *max_out = min + span;
    *step_out = 1u + (uint32_t)(failalloc_rand() % 4u);
}

static void exercise_bitmap32(uint64_t seed) {
    install_failing_allocator(seed);
    pool32_t pool = {0};

    for (uint32_t round = 0; round < ROUNDS_PER_SEED; ++round) {
        const uint32_t op = (uint32_t)(failalloc_rand() % NUM_OPS_32);

        switch (op) {
            case 0:
                pool32_add(&pool, roaring_bitmap_create());
                break;
            case 1:
                pool32_add(&pool, roaring_bitmap_create_with_capacity((
                                      uint32_t)(1u + (failalloc_rand() % 8u))));
                break;
            case 2: {
                uint64_t a, b;
                uint32_t step;
                random_from_range_params32(&a, &b, &step);
                pool32_add(&pool, roaring_bitmap_from_range(a, b, step));
                break;
            }
            case 3: {
                const uint32_t vals[] = {1, 42, 65537};
                pool32_add(&pool, roaring_bitmap_of_ptr(3, vals));
                break;
            }
            case 4: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    uint32_t a, b;
                    random_u32_closed_range(&a, &b);
                    roaring_bitmap_add(r, random_u32_value());
                    roaring_bitmap_add_range_closed(r, a, b);
                }
                break;
            }
            case 5: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    const uint32_t vals[] = {7, 99, 1000, 65535};
                    roaring_bitmap_add_many(r, 4, vals);
                }
                break;
            }
            case 6: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    uint32_t a, b;
                    random_u32_closed_range(&a, &b);
                    roaring_bitmap_remove(r, random_u32_value());
                    roaring_bitmap_remove_range_closed(r, a, b);
                }
                break;
            }
            case 7: {
                roaring_bitmap_t *copy = pool32_copy_for_inplace(&pool);
                if (copy != NULL) {
                    roaring_bitmap_clear(copy);
                    pool32_add(&pool, copy);
                }
                break;
            }
            case 8: {
                roaring_bitmap_t *a, *b;
                if (pool32_pick_two(&pool, &a, &b)) {
                    roaring_bitmap_t *result = roaring_bitmap_and(a, b);
                    if (result != NULL) {
                        assert_bitmap32_valid(result);
                        pool32_add(&pool, result);
                    }
                }
                break;
            }
            case 9: {
                roaring_bitmap_t *a, *b;
                if (pool32_pick_two(&pool, &a, &b)) {
                    roaring_bitmap_t *result = roaring_bitmap_or(a, b);
                    if (result != NULL) {
                        assert_bitmap32_valid(result);
                        pool32_add(&pool, result);
                    }
                }
                break;
            }
            case 10: {
                roaring_bitmap_t *a, *b;
                if (pool32_pick_two(&pool, &a, &b)) {
                    roaring_bitmap_t *result = roaring_bitmap_xor(a, b);
                    if (result != NULL) {
                        assert_bitmap32_valid(result);
                        pool32_add(&pool, result);
                    }
                }
                break;
            }
            case 11: {
                roaring_bitmap_t *a, *b;
                if (pool32_pick_two(&pool, &a, &b)) {
                    roaring_bitmap_t *result = roaring_bitmap_andnot(a, b);
                    if (result != NULL) {
                        assert_bitmap32_valid(result);
                        pool32_add(&pool, result);
                    }
                }
                break;
            }
            case 12: {
                roaring_bitmap_t *copy = pool32_copy_for_inplace(&pool);
                roaring_bitmap_t *other = pool32_pick(&pool);
                if (copy != NULL && other != NULL) {
                    roaring_bitmap_and_inplace(copy, other);
                    assert_bitmap32_valid(copy);
                    pool32_add(&pool, copy);
                } else if (copy != NULL) {
                    roaring_bitmap_free(copy);
                }
                break;
            }
            case 13: {
                roaring_bitmap_t *copy = pool32_copy_for_inplace(&pool);
                roaring_bitmap_t *other = pool32_pick(&pool);
                if (copy != NULL && other != NULL) {
                    roaring_bitmap_or_inplace(copy, other);
                    assert_bitmap32_valid(copy);
                    pool32_add(&pool, copy);
                } else if (copy != NULL) {
                    roaring_bitmap_free(copy);
                }
                break;
            }
            case 14: {
                roaring_bitmap_t *copy = pool32_copy_for_inplace(&pool);
                roaring_bitmap_t *other = pool32_pick(&pool);
                if (copy != NULL && other != NULL) {
                    roaring_bitmap_xor_inplace(copy, other);
                    assert_bitmap32_valid(copy);
                    pool32_add(&pool, copy);
                } else if (copy != NULL) {
                    roaring_bitmap_free(copy);
                }
                break;
            }
            case 15: {
                roaring_bitmap_t *copy = pool32_copy_for_inplace(&pool);
                roaring_bitmap_t *other = pool32_pick(&pool);
                if (copy != NULL && other != NULL) {
                    roaring_bitmap_andnot_inplace(copy, other);
                    assert_bitmap32_valid(copy);
                    pool32_add(&pool, copy);
                } else if (copy != NULL) {
                    roaring_bitmap_free(copy);
                }
                break;
            }
            case 16: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    uint32_t a, b;
                    random_u32_closed_range(&a, &b);
                    roaring_bitmap_t *flipped =
                        roaring_bitmap_flip_closed(r, a, b);
                    if (flipped != NULL) {
                        assert_bitmap32_valid(flipped);
                        pool32_add(&pool, flipped);
                    }
                }
                break;
            }
            case 17: {
                roaring_bitmap_t *copy = pool32_copy_for_inplace(&pool);
                if (copy != NULL) {
                    uint32_t a, b;
                    random_u32_closed_range(&a, &b);
                    roaring_bitmap_flip_inplace_closed(copy, a, b);
                    assert_bitmap32_valid(copy);
                    pool32_add(&pool, copy);
                }
                break;
            }
            case 18: {
                roaring_bitmap_t *a, *b;
                if (pool32_pick_two(&pool, &a, &b)) {
                    roaring_bitmap_t *lazy = roaring_bitmap_lazy_or(
                        a, b, (bool)(failalloc_rand() & 1u));
                    if (lazy != NULL) {
                        roaring_bitmap_repair_after_lazy(lazy);
                        assert_bitmap32_valid(lazy);
                        pool32_add(&pool, lazy);
                    }
                }
                break;
            }
            case 19: {
                roaring_bitmap_t *copy = pool32_copy_for_inplace(&pool);
                roaring_bitmap_t *other = pool32_pick(&pool);
                if (copy != NULL && other != NULL) {
                    roaring_bitmap_lazy_xor_inplace(copy, other);
                    roaring_bitmap_repair_after_lazy(copy);
                    assert_bitmap32_valid(copy);
                    pool32_add(&pool, copy);
                } else if (copy != NULL) {
                    roaring_bitmap_free(copy);
                }
                break;
            }
            case 20: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    pool32_add(&pool, roaring_bitmap_copy(r));
                }
                break;
            }
            case 21: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    pool32_add(&pool,
                               roaring_bitmap_add_offset(
                                   r, (uint32_t)(failalloc_rand() % 64u)));
                }
                break;
            }
            case 22: {
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
            case 23: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    roaring_bitmap_shrink_to_fit(r);
                    roaring_bitmap_run_optimize(r);
                    roaring_bitmap_remove_run_compression(r);
                }
                break;
            }
            case 24: {
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
            case 25: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL && !roaring_bitmap_is_empty(r)) {
                    uint32_t value = 0;
                    (void)roaring_bitmap_select(r, 0, &value);
                    (void)roaring_bitmap_rank(r, value);
                    (void)roaring_bitmap_contains(r, value);
                }
                break;
            }
            case 26: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    roaring_uint32_iterator_t *it = roaring_iterator_create(r);
                    if (it != NULL) {
                        roaring_uint32_iterator_free(it);
                    }
                }
                break;
            }
            case 27: {
                roaring_bitmap_t *r = pool32_pick(&pool);
                if (r != NULL) {
                    const uint32_t vals[] = {7u, 13u, 21u, 40010u};
                    roaring_bitmap_remove_many(r, 4, vals);
                }
                break;
            }
            default:
                pool32_remove_random(&pool);
                break;
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
        const uint32_t op = (uint32_t)(failalloc_rand() % NUM_OPS_64);

        switch (op) {
            case 0:
                pool64_add(&pool, roaring64_bitmap_create());
                break;
            case 1: {
                uint64_t a, b;
                uint32_t step;
                random_from_range_params64(&a, &b, &step);
                pool64_add(&pool, roaring64_bitmap_from_range(a, b, step));
                break;
            }
            case 2: {
                const uint64_t vals[] = {1, 42, 0x100000000ULL};
                pool64_add(&pool, roaring64_bitmap_of_ptr(3, vals));
                break;
            }
            case 3: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    uint64_t a, b;
                    random_u64_closed_range(&a, &b);
                    roaring64_bitmap_add(r, random_u64_value());
                    roaring64_bitmap_add_range_closed(r, a, b);
                }
                break;
            }
            case 4: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    uint64_t a, b;
                    const uint64_t vals[] = {7, 99, 0xdeadbeefULL};
                    random_u64_closed_range(&a, &b);
                    roaring64_bitmap_add_many(r, 3, vals);
                    roaring64_bitmap_remove(r, random_u64_value());
                    roaring64_bitmap_remove_range_closed(r, a, b);
                }
                break;
            }
            case 5: {
                roaring64_bitmap_t *copy = pool64_copy_for_inplace(&pool);
                if (copy != NULL) {
                    roaring64_bitmap_clear(copy);
                    pool64_add(&pool, copy);
                }
                break;
            }
            case 6: {
                roaring64_bitmap_t *a, *b;
                if (pool64_pick_two(&pool, &a, &b)) {
                    roaring64_bitmap_t *result = roaring64_bitmap_and(a, b);
                    if (result != NULL) {
                        assert_bitmap64_valid(result);
                        pool64_add(&pool, result);
                    }
                }
                break;
            }
            case 7: {
                roaring64_bitmap_t *a, *b;
                if (pool64_pick_two(&pool, &a, &b)) {
                    roaring64_bitmap_t *result = roaring64_bitmap_or(a, b);
                    if (result != NULL) {
                        assert_bitmap64_valid(result);
                        pool64_add(&pool, result);
                    }
                }
                break;
            }
            case 8: {
                roaring64_bitmap_t *a, *b;
                if (pool64_pick_two(&pool, &a, &b)) {
                    roaring64_bitmap_t *result = roaring64_bitmap_xor(a, b);
                    if (result != NULL) {
                        assert_bitmap64_valid(result);
                        pool64_add(&pool, result);
                    }
                }
                break;
            }
            case 9: {
                roaring64_bitmap_t *a, *b;
                if (pool64_pick_two(&pool, &a, &b)) {
                    roaring64_bitmap_t *result = roaring64_bitmap_andnot(a, b);
                    if (result != NULL) {
                        assert_bitmap64_valid(result);
                        pool64_add(&pool, result);
                    }
                }
                break;
            }
            case 10: {
                roaring64_bitmap_t *copy = pool64_copy_for_inplace(&pool);
                roaring64_bitmap_t *other = pool64_pick(&pool);
                if (copy != NULL && other != NULL) {
                    roaring64_bitmap_and_inplace(copy, other);
                    assert_bitmap64_valid(copy);
                    pool64_add(&pool, copy);
                } else if (copy != NULL) {
                    roaring64_bitmap_free(copy);
                }
                break;
            }
            case 11: {
                roaring64_bitmap_t *copy = pool64_copy_for_inplace(&pool);
                roaring64_bitmap_t *other = pool64_pick(&pool);
                if (copy != NULL && other != NULL) {
                    roaring64_bitmap_or_inplace(copy, other);
                    assert_bitmap64_valid(copy);
                    pool64_add(&pool, copy);
                } else if (copy != NULL) {
                    roaring64_bitmap_free(copy);
                }
                break;
            }
            case 12: {
                roaring64_bitmap_t *copy = pool64_copy_for_inplace(&pool);
                roaring64_bitmap_t *other = pool64_pick(&pool);
                if (copy != NULL && other != NULL) {
                    roaring64_bitmap_xor_inplace(copy, other);
                    assert_bitmap64_valid(copy);
                    pool64_add(&pool, copy);
                } else if (copy != NULL) {
                    roaring64_bitmap_free(copy);
                }
                break;
            }
            case 13: {
                roaring64_bitmap_t *copy = pool64_copy_for_inplace(&pool);
                roaring64_bitmap_t *other = pool64_pick(&pool);
                if (copy != NULL && other != NULL) {
                    roaring64_bitmap_andnot_inplace(copy, other);
                    assert_bitmap64_valid(copy);
                    pool64_add(&pool, copy);
                } else if (copy != NULL) {
                    roaring64_bitmap_free(copy);
                }
                break;
            }
            case 14: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    uint64_t a, b;
                    random_u64_closed_range(&a, &b);
                    roaring64_bitmap_t *flipped =
                        roaring64_bitmap_flip_closed(r, a, b);
                    if (flipped != NULL) {
                        assert_bitmap64_valid(flipped);
                        pool64_add(&pool, flipped);
                    }
                }
                break;
            }
            case 15: {
                roaring64_bitmap_t *copy = pool64_copy_for_inplace(&pool);
                if (copy != NULL) {
                    uint64_t a, b;
                    random_u64_closed_range(&a, &b);
                    roaring64_bitmap_flip_closed_inplace(copy, a, b);
                    assert_bitmap64_valid(copy);
                    pool64_add(&pool, copy);
                }
                break;
            }
            case 16: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    pool64_add(&pool, roaring64_bitmap_copy(r));
                    pool64_add(&pool,
                               roaring64_bitmap_add_offset(
                                   r, (uint64_t)(failalloc_rand() % 64u)));
                }
                break;
            }
            case 17: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    size_t sz = roaring64_bitmap_portable_size_in_bytes(r);
                    char *buf = (char *)malloc(sz);
                    if (buf != NULL) {
                        roaring64_bitmap_portable_serialize(r, buf);
                        roaring64_bitmap_t *deser =
                            roaring64_bitmap_portable_deserialize_safe(buf, sz);
                        if (deser != NULL) {
                            assert_bitmap64_valid(deser);
                            pool64_add(&pool, deser);
                        }
                        free(buf);
                    }
                }
                break;
            }
            case 18: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    roaring64_bitmap_shrink_to_fit(r);
                    roaring64_bitmap_run_optimize(r);
                    roaring64_bitmap_remove_run_compression(r);
                }
                break;
            }
            case 19: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL && !roaring64_bitmap_is_empty(r)) {
                    uint64_t value = 0;
                    (void)roaring64_bitmap_select(r, 0, &value);
                    (void)roaring64_bitmap_rank(r, value);
                    (void)roaring64_bitmap_contains(r, value);
                }
                break;
            }
            case 20: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    roaring64_iterator_t *it = roaring64_iterator_create(r);
                    if (it != NULL) {
                        roaring64_iterator_free(it);
                    }
                }
                break;
            }
            case 21: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    roaring64_bitmap_overwrite(r, r);
                }
                break;
            }
            case 22: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    uint64_t a, b;
                    random_u64_closed_range(&a, &b);
                    roaring64_bitmap_t *flipped =
                        roaring64_bitmap_flip(r, a, b);
                    if (flipped != NULL) {
                        assert_bitmap64_valid(flipped);
                        pool64_add(&pool, flipped);
                    }
                }
                break;
            }
            case 23: {
                roaring64_bitmap_t *r = pool64_pick(&pool);
                if (r != NULL) {
                    const uint64_t vals[] = {7u, 13u, 0x10001ULL};
                    roaring64_bitmap_remove_many(r, 3, vals);
                    (void)roaring64_bitmap_add_checked(r, 99u);
                    (void)roaring64_bitmap_remove_checked(r, 7u);
                }
                break;
            }
            default:
                pool64_remove_random(&pool);
                break;
        }

        pool64_validate_all(&pool);
    }

    pool64_clear(&pool);
    restore_default_allocator();

    assert_true(g_failalloc.alloc_failures > 0);
}

// Chunk-0 bitsets (>4096 values, step-2 so Roaring uses bitset not run).
// Inplace xor/andnot with a partner shrinks cardinality enough to trigger
// bitset-to-array conversion.
#define BITSET_SHRINK_BASE_MIN 1u
#define BITSET_SHRINK_BASE_MAX 16386u
#define BITSET_SHRINK_BASE_STEP 2u
#define BITSET_SHRINK_PARTNER_MIN 8193u
#define BITSET_SHRINK_PARTNER_MAX 16386u
#define BITSET_SHRINK_PARTNER_STEP 2u
#define BITSET_SHRINK_ROUNDS 80u

static roaring_bitmap_t *make_dense_bitset_chunk_bitmap(void) {
    return roaring_bitmap_from_range(BITSET_SHRINK_BASE_MIN,
                                     BITSET_SHRINK_BASE_MAX,
                                     BITSET_SHRINK_BASE_STEP);
}

static roaring_bitmap_t *make_bitset_shrink_partner_bitmap(void) {
    return roaring_bitmap_from_range(BITSET_SHRINK_PARTNER_MIN,
                                     BITSET_SHRINK_PARTNER_MAX,
                                     BITSET_SHRINK_PARTNER_STEP);
}

static void exercise_bitset_shrink_oom_paths(uint64_t seed) {
    // Build inputs with the default allocator so chunk containers are valid
    // dense bitsets before we stress inplace shrink-to-array under the hook.
    restore_default_allocator();
    roaring_bitmap_t *base = make_dense_bitset_chunk_bitmap();
    roaring_bitmap_t *partner = make_bitset_shrink_partner_bitmap();
    if (base == NULL || partner == NULL) {
        roaring_bitmap_free(base);
        roaring_bitmap_free(partner);
        fail_msg("failed to build bitset-shrink test inputs");
    }
    assert_bitmap32_valid(base);
    assert_bitmap32_valid(partner);

    install_failing_allocator(seed);

    for (uint32_t round = 0; round < BITSET_SHRINK_ROUNDS; ++round) {
        roaring_bitmap_t *xor_inplace_copy = roaring_bitmap_copy(base);
        if (xor_inplace_copy != NULL) {
            roaring_bitmap_xor_inplace(xor_inplace_copy, partner);
            assert_bitmap32_valid(xor_inplace_copy);
            roaring_bitmap_free(xor_inplace_copy);
        }

        roaring_bitmap_t *xor_alloc = roaring_bitmap_xor(base, partner);
        if (xor_alloc != NULL) {
            assert_bitmap32_valid(xor_alloc);
            roaring_bitmap_free(xor_alloc);
        }

        roaring_bitmap_t *andnot_alloc = roaring_bitmap_andnot(base, partner);
        if (andnot_alloc != NULL) {
            assert_bitmap32_valid(andnot_alloc);
            roaring_bitmap_free(andnot_alloc);
        }
    }

    restore_default_allocator();
    roaring_bitmap_free(base);
    roaring_bitmap_free(partner);
    assert_true(g_failalloc.alloc_failures > 0);
}

DEFINE_TEST(test_bitset_shrink_oom_paths) {
    static const uint64_t seeds[] = {0xBEEFu, 0xCAFEu, 0xDEADu};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_bitset_shrink_oom_paths(seeds[i]);
    }
}

// Inplace andnot bitset-shrink under OOM may leave wrong semantics (no
// xor-style undo), but must not crash. internal_validate is not asserted here.
static void exercise_andnot_inplace_shrink_oom_paths(uint64_t seed) {
    restore_default_allocator();
    roaring_bitmap_t *base = make_dense_bitset_chunk_bitmap();
    roaring_bitmap_t *partner = make_bitset_shrink_partner_bitmap();
    if (base == NULL || partner == NULL) {
        roaring_bitmap_free(base);
        roaring_bitmap_free(partner);
        fail_msg("failed to build andnot-inplace shrink test inputs");
    }

    install_failing_allocator(seed);

    for (uint32_t round = 0; round < BITSET_SHRINK_ROUNDS; ++round) {
        roaring_bitmap_t *copy = roaring_bitmap_copy(base);
        if (copy != NULL) {
            roaring_bitmap_andnot_inplace(copy, partner);
            roaring_bitmap_free(copy);
        }
    }

    restore_default_allocator();
    roaring_bitmap_free(base);
    roaring_bitmap_free(partner);
    assert_true(g_failalloc.alloc_failures > 0);
}

DEFINE_TEST(test_andnot_inplace_shrink_oom_paths) {
    static const uint64_t seeds[] = {0xAD00u, 0xAD01u};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_andnot_inplace_shrink_oom_paths(seeds[i]);
    }
}

// Small dense array container in chunk 0; full-chunk flip triggers
// container_inot on an array (bitset allocation may fail under the hook).
#define ARRAY_INOT_ROUNDS 60u

static roaring_bitmap_t *make_small_array_chunk_bitmap(void) {
    return roaring_bitmap_from_range(1u, 400u, 1u);
}

static void exercise_array_inot_oom_paths(uint64_t seed) {
    restore_default_allocator();
    roaring_bitmap_t *base = make_small_array_chunk_bitmap();
    if (base == NULL) {
        fail_msg("failed to build array-inot test input");
    }
    assert_bitmap32_valid(base);

    install_failing_allocator(seed);

    for (uint32_t round = 0; round < ARRAY_INOT_ROUNDS; ++round) {
        roaring_bitmap_t *flip_inplace_copy = roaring_bitmap_copy(base);
        if (flip_inplace_copy != NULL) {
            roaring_bitmap_flip_inplace_closed(flip_inplace_copy, 0u, 0xFFFFu);
            assert_bitmap32_valid(flip_inplace_copy);
            roaring_bitmap_free(flip_inplace_copy);
        }

        roaring_bitmap_t *flip_alloc =
            roaring_bitmap_flip_closed(base, 0u, 0xFFFFu);
        if (flip_alloc != NULL) {
            assert_bitmap32_valid(flip_alloc);
            roaring_bitmap_free(flip_alloc);
        }
    }

    restore_default_allocator();
    roaring_bitmap_free(base);
    assert_true(g_failalloc.alloc_failures > 0);
}

DEFINE_TEST(test_array_inot_oom_paths) {
    static const uint64_t seeds[] = {0xF00Du, 0xBABEu};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_array_inot_oom_paths(seeds[i]);
    }
}

#define FLIP_BITSET_ROUNDS 60u

static void exercise_flip_inplace_oom_paths(uint64_t seed) {
    restore_default_allocator();
    roaring_bitmap_t *base = make_dense_bitset_chunk_bitmap();
    if (base == NULL) {
        fail_msg("failed to build flip-inplace test input");
    }
    assert_bitmap32_valid(base);

    install_failing_allocator(seed);

    for (uint32_t round = 0; round < FLIP_BITSET_ROUNDS; ++round) {
        roaring_bitmap_t *copy = roaring_bitmap_copy(base);
        if (copy != NULL) {
            roaring_bitmap_flip_inplace_closed(copy, 100u, 20000u);
            assert_bitmap32_valid(copy);
            roaring_bitmap_free(copy);
        }
    }

    restore_default_allocator();
    roaring_bitmap_free(base);
    assert_true(g_failalloc.alloc_failures > 0);
}

DEFINE_TEST(test_flip_inplace_oom_paths) {
    static const uint64_t seeds[] = {0xFACEu, 0x600Du};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_flip_inplace_oom_paths(seeds[i]);
    }
}

#define LAZY_OR_INPLACE_ROUNDS 60u

static void exercise_lazy_or_inplace_oom_paths(uint64_t seed) {
    restore_default_allocator();
    roaring_bitmap_t *base = make_dense_bitset_chunk_bitmap();
    roaring_bitmap_t *partner = make_bitset_shrink_partner_bitmap();
    if (base == NULL || partner == NULL) {
        roaring_bitmap_free(base);
        roaring_bitmap_free(partner);
        fail_msg("failed to build lazy-or-inplace test inputs");
    }
    assert_bitmap32_valid(base);
    assert_bitmap32_valid(partner);

    install_failing_allocator(seed);

    for (uint32_t round = 0; round < LAZY_OR_INPLACE_ROUNDS; ++round) {
        roaring_bitmap_t *copy = roaring_bitmap_copy(base);
        if (copy != NULL) {
            roaring_bitmap_lazy_or_inplace(copy, partner, false);
            assert_bitmap32_valid(copy);
            roaring_bitmap_free(copy);
        }
    }

    restore_default_allocator();
    roaring_bitmap_free(base);
    roaring_bitmap_free(partner);
    assert_true(g_failalloc.alloc_failures > 0);
}

DEFINE_TEST(test_lazy_or_inplace_oom_paths) {
    static const uint64_t seeds[] = {0x1CEu, 0x0FFu};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_lazy_or_inplace_oom_paths(seeds[i]);
    }
}

#define COW_INPLACE_ROUNDS 60u

static void exercise_cow_inplace_oom_paths(uint64_t seed) {
    restore_default_allocator();
    roaring_bitmap_t *low = roaring_bitmap_from_range(1u, 500u, 1u);
    roaring_bitmap_t *high = roaring_bitmap_from_range(40000u, 40500u, 1u);
    if (low == NULL || high == NULL) {
        roaring_bitmap_free(low);
        roaring_bitmap_free(high);
        fail_msg("failed to build COW test inputs");
    }
    roaring_bitmap_or_inplace(low, high);
    roaring_bitmap_t *cow = roaring_bitmap_copy(low);
    roaring_bitmap_set_copy_on_write(cow, true);
    if (cow == NULL) {
        roaring_bitmap_free(low);
        roaring_bitmap_free(high);
        fail_msg("failed to copy bitmap for COW test");
    }
    assert_bitmap32_valid(low);
    assert_bitmap32_valid(cow);

    install_failing_allocator(seed);

    for (uint32_t round = 0; round < COW_INPLACE_ROUNDS; ++round) {
        roaring_bitmap_t *extra = roaring_bitmap_from_range(60000u, 60100u, 1u);
        if (extra != NULL) {
            roaring_bitmap_or_inplace(cow, extra);
            assert_bitmap32_valid(cow);
            assert_bitmap32_valid(low);
            roaring_bitmap_free(extra);
        }
    }

    restore_default_allocator();
    roaring_bitmap_free(cow);
    roaring_bitmap_free(low);
    roaring_bitmap_free(high);
    assert_true(g_failalloc.alloc_failures > 0);
}

DEFINE_TEST(test_cow_inplace_oom_paths) {
    static const uint64_t seeds[] = {0xC0B1u, 0xC0B2u};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_cow_inplace_oom_paths(seeds[i]);
    }
}

#define COW_ADD_REMOVE_ROUNDS 80u
// Fail often enough that shared-container extract reliably sees OOM.
#define COW_ADD_REMOVE_FAIL_MODULO 3u

// Parent must be COW before copy so ra_overwrite installs SHARED wrappers.
static roaring_bitmap_t *make_cow_shared_parent_bitmap(void) {
    restore_default_allocator();
    roaring_bitmap_t *parent = roaring_bitmap_from_range(1u, 500u, 1u);
    roaring_bitmap_t *high = roaring_bitmap_from_range(40000u, 40500u, 1u);
    if (parent == NULL || high == NULL) {
        roaring_bitmap_free(parent);
        roaring_bitmap_free(high);
        fail_msg("failed to build COW shared test inputs");
    }
    roaring_bitmap_or_inplace(parent, high);
    roaring_bitmap_free(high);
    roaring_bitmap_set_copy_on_write(parent, true);
    assert_bitmap32_valid(parent);
    return parent;
}

/**
 * Bulk remove on a COW copy with SHARED containers: each value calls
 * ra_unshare then container_remove, with SHARED/NULL guards on OOM.
 */
static void exercise_cow_shared_remove_many_oom(uint64_t fail_nth) {
    roaring_bitmap_t *parent = make_cow_shared_parent_bitmap();

    restore_default_allocator();
    roaring_bitmap_t *cow = roaring_bitmap_copy(parent);
    if (cow == NULL) {
        roaring_bitmap_free(parent);
        fail_msg("failed to copy COW shared bitmap");
    }
    assert_true(roaring_bitmap_get_copy_on_write(cow));
    assert_bitmap32_valid(cow);

    const uint32_t remove_vals[] = {50u, 75u, 100u, 40001u, 40100u};

    install_fail_nth_allocator(fail_nth);
    roaring_bitmap_remove_many(cow, 5, remove_vals);
    restore_default_allocator();

    assert_bitmap32_valid(cow);
    assert_bitmap32_valid(parent);
    assert_true(roaring_bitmap_contains(cow, 200u));
    assert_true(roaring_bitmap_contains(cow, 40150u));

    assert_true(g_failalloc.alloc_failures > 0);

    roaring_bitmap_free(cow);
    roaring_bitmap_free(parent);
}

DEFINE_TEST(test_cow_shared_remove_many_oom) {
    exercise_cow_shared_remove_many_oom(1);
}

static void exercise_cow_shared_range_oom(uint64_t fail_nth) {
    roaring_bitmap_t *parent = make_cow_shared_parent_bitmap();

    restore_default_allocator();
    roaring_bitmap_t *cow = roaring_bitmap_copy(parent);
    if (cow == NULL) {
        roaring_bitmap_free(parent);
        fail_msg("failed to copy COW shared bitmap for range test");
    }
    assert_bitmap32_valid(cow);

    install_fail_nth_allocator(fail_nth);
    roaring_bitmap_add_range_closed(cow, 10u, 60u);
    roaring_bitmap_remove_range_closed(cow, 20u, 30u);
    restore_default_allocator();

    assert_bitmap32_valid(cow);
    assert_bitmap32_valid(parent);
    assert_true(g_failalloc.alloc_failures > 0);

    roaring_bitmap_free(cow);
    roaring_bitmap_free(parent);
}

DEFINE_TEST(test_cow_shared_range_oom) { exercise_cow_shared_range_oom(1); }

static void exercise_inplace_merge_insert_oom(void) {
    restore_default_allocator();
    roaring_bitmap_t *high_base = roaring_bitmap_from_range(40000u, 40100u, 1u);
    roaring_bitmap_t *low_base = roaring_bitmap_from_range(1u, 100u, 1u);
    if (high_base == NULL || low_base == NULL) {
        roaring_bitmap_free(high_base);
        roaring_bitmap_free(low_base);
        fail_msg("failed to build inplace-merge test inputs");
    }

    bool saw_failure = false;
    for (uint64_t fail_nth = 1; fail_nth <= 64; fail_nth++) {
        restore_default_allocator();
        roaring_bitmap_t *high = roaring_bitmap_copy(high_base);
        roaring_bitmap_t *low = roaring_bitmap_copy(low_base);
        if (high == NULL || low == NULL) {
            roaring_bitmap_free(high);
            roaring_bitmap_free(low);
            roaring_bitmap_free(high_base);
            roaring_bitmap_free(low_base);
            fail_msg("failed to copy inplace-merge test inputs");
        }

        install_fail_nth_allocator(fail_nth);
        roaring_bitmap_or_inplace(high, low);
        restore_default_allocator();

        assert_bitmap32_valid(high);
        assert_bitmap32_valid(low);
        if (g_failalloc.alloc_failures > 0) {
            saw_failure = true;
        }
        roaring_bitmap_free(high);
        roaring_bitmap_free(low);
    }

    restore_default_allocator();
    roaring_bitmap_free(high_base);
    roaring_bitmap_free(low_base);
    assert_true(saw_failure);
}

DEFINE_TEST(test_inplace_merge_insert_oom) {
    exercise_inplace_merge_insert_oom();
}

static void exercise_cow_add_remove_oom_paths(uint64_t seed) {
    roaring_bitmap_t *parent = make_cow_shared_parent_bitmap();

    uint64_t total_failures = 0;

    for (uint32_t round = 0; round < COW_ADD_REMOVE_ROUNDS; ++round) {
        restore_default_allocator();
        roaring_bitmap_t *cow = roaring_bitmap_copy(parent);
        if (cow == NULL) {
            roaring_bitmap_free(parent);
            fail_msg("failed to copy bitmap for COW add/remove test");
        }
        assert_bitmap32_valid(cow);

        install_failing_allocator_modulo(seed + (uint64_t)round,
                                         COW_ADD_REMOVE_FAIL_MODULO);

        const uint32_t low_vals[] = {50u, 100u, 250u, 499u};
        const uint32_t high_vals[] = {40001u, 40100u, 40499u};
        const uint32_t mixed_vals[] = {75u, 40050u, 40200u};
        const uint32_t remove_many_vals[] = {60u, 90u, 40010u, 40120u};

        roaring_bitmap_add(cow, low_vals[round % 4u]);
        roaring_bitmap_add(cow, high_vals[round % 3u]);
        (void)roaring_bitmap_add_checked(cow, low_vals[(round + 1u) % 4u]);
        (void)roaring_bitmap_add_checked(cow, high_vals[(round + 2u) % 3u]);
        roaring_bitmap_add_many(cow, 3, mixed_vals);
        roaring_bitmap_add_range_closed(cow, 15u, 25u);
        roaring_bitmap_remove_range_closed(cow, 18u, 22u);

        roaring_bitmap_remove_many(cow, 4, remove_many_vals);

        roaring_bitmap_remove(cow, low_vals[(round + 2u) % 4u]);
        roaring_bitmap_remove(cow, high_vals[(round + 1u) % 3u]);
        (void)roaring_bitmap_remove_checked(cow, low_vals[round % 4u]);
        (void)roaring_bitmap_remove_checked(cow, high_vals[round % 3u]);

        assert_bitmap32_valid(cow);
        assert_bitmap32_valid(parent);

        total_failures += g_failalloc.alloc_failures;
        roaring_bitmap_free(cow);
    }

    restore_default_allocator();
    roaring_bitmap_free(parent);
    assert_true(total_failures > 0);
}

DEFINE_TEST(test_cow_add_remove_oom_paths) {
    static const uint64_t seeds[] = {0xADD1u, 0xADD2u, 0xADD3u};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_cow_add_remove_oom_paths(seeds[i]);
    }
}

DEFINE_TEST(test_memory_pressure_roaring32) {
    static const uint64_t seeds[] = {1, 42};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_bitmap32(seeds[i]);
    }
}

DEFINE_TEST(test_memory_pressure_roaring64) {
    static const uint64_t seeds[] = {7, 99};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        exercise_bitmap64(seeds[i]);
    }
}

int main(void) {
    tellmeall();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bitset_shrink_oom_paths),
        cmocka_unit_test(test_andnot_inplace_shrink_oom_paths),
        cmocka_unit_test(test_array_inot_oom_paths),
        cmocka_unit_test(test_flip_inplace_oom_paths),
        cmocka_unit_test(test_lazy_or_inplace_oom_paths),
        cmocka_unit_test(test_cow_inplace_oom_paths),
        cmocka_unit_test(test_cow_shared_remove_many_oom),
        cmocka_unit_test(test_cow_shared_range_oom),
        cmocka_unit_test(test_inplace_merge_insert_oom),
        cmocka_unit_test(test_cow_add_remove_oom_paths),
        cmocka_unit_test(test_memory_pressure_roaring32),
        cmocka_unit_test(test_memory_pressure_roaring64),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}