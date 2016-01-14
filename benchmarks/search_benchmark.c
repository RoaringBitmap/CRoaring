#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "benchmark.h"

static int32_t linear_search(const uint16_t *array, size_t len, uint16_t key) {
    for (size_t i = 0; i < len; ++i) {
        if (array[i] == key) {
            return i;
        }
    }
    return -1;
}

#define linear_search_simd(simd_size)                                      \
    static int32_t linear_search_##simd_size##(const uint16_t *array,      \
                                               size_t len, uint16_t key) { \
        for (size_t i = 0; i < len; ++i) {                                 \
            if (array[i] == key) {                                         \
                return i;                                                  \
            }                                                              \
        }                                                                  \
        return -1;                                                         \
    }

static int32_t binary_search(uint16_t *source, int32_t n, uint16_t target) {
    uint16_t *base = source;
    if (n == 0) return -1;
    while (n > 1) {
        int32_t half = n / 2;
        base = (base[half] < target) ? &base[half] : base;
        n -= half;
    }
    base += *base < target;
    return *base == target ? base - source : -1;
}

static int32_t binary_search_leaf_prefetch(uint16_t *source, int32_t n,
                                           uint16_t target) {
    uint16_t *base = source;
    if (n == 0) return -1;
    while (n > 1) {
        int32_t half = n / 2;
        __builtin_prefetch(base + (half / 2), 0, 0);
        __builtin_prefetch(base + half + (half / 2), 0, 0);
        base = (base[half] < target) ? &base[half] : base;
        n -= half;
    }
    base += *base < target;
    return *base == target ? base - source : -1;
}

int32_t run_test(__typeof__(linear_search) search, const uint16_t *array,
                 size_t n_elems, const uint16_t *searches, size_t n_searches,
                 bool cache_warm) {
    size_t found = 0;

    for (size_t i = 0; i < n_searches; ++i) {
        if (search(array, n_elems, searches[i]) != -1) {
            found++;
        }
    }

    return found;
}

#define val(idx) (2 * idx)

#define assert_eq(a, b)                                                \
    do {                                                               \
        if (a != b) {                                                  \
            fprintf(stderr, "expected %d got %d from %s\n", a, b, #b); \
        }                                                              \
    } while (0)

#define CACHELINE_PER_ARRAY(size) (size * 2 / 64)

void cache_populate(uint16_t *array, size_t n_elems) {
    for (size_t i = 0; i < CACHELINE_PER_ARRAY(n_elems); ++i)
        __builtin_prefetch(&array[i]);
}

void cache_flush(uint16_t *array, size_t n_elems) {
    for (size_t i = 0; i < n_elems; ++i) __builtin_ia32_clflush(&array[i]);
}

int main() {
    size_t repeat = 100;
    size_t n_elems = 16;
    // WARN: update searches init loop if changing this expression.
    size_t n_searches = n_elems * 2;
    size_t expected_finds = 0;

    uint16_t array[n_elems];
    for (size_t i = 0; i < n_elems; ++i) {
        array[i] = val(i);
    }

    uint16_t searches[n_searches];
    for (size_t i = 0; i < n_searches; ++i) {
        searches[i] = i;

        if ((i % 2 == 0) && array[i / 2] == i) {
            expected_finds++;
        }
    }

    /* shuffling searches is used to compensate the advantage in cache
     * locality induced by the benchmark that linear_search would benefit */
    /* shuffle(searches, n_searches); */

    /* used for reporting */
    const bool cache_warm = true;
    const bool cache_cold = false;

    /* validate implementations */
    for (size_t i = 0; i < n_searches; ++i) {
        const uint32_t expected = linear_search(array, n_elems, searches[i]);
        assert_eq(expected, binary_search(array, n_elems, searches[i]));
        assert_eq(expected,
                  binary_search_leaf_prefetch(array, n_elems, searches[i]));
    }

    // clang-format off
    BEST_TIME_PRE(run_test(linear_search,
                       &array, n_elems,
                       &searches, n_searches, cache_warm),
                  cache_populate(array, n_elems),
                  expected_finds, repeat, n_searches);

    BEST_TIME_PRE(run_test(binary_search,
                       &array, n_elems,
                       &searches, n_searches, cache_warm),
                  cache_populate(array, n_elems),
                  expected_finds, repeat, n_searches);

    BEST_TIME_PRE(run_test(binary_search_leaf_prefetch,
                       &array, n_elems,
                       &searches, n_searches, cache_warm),
                  cache_populate(array, n_elems),
                  expected_finds, repeat, n_searches);

    BEST_TIME_PRE(run_test(linear_search,
                       &array, n_elems,
                       &searches, n_searches, cache_cold),
                  cache_flush(array, n_elems),
                  expected_finds, repeat, n_searches);

    BEST_TIME_PRE(run_test(binary_search,
                       &array, n_elems,
                       &searches, n_searches, cache_cold),
                  cache_flush(array, n_elems),
                  expected_finds, repeat, n_searches);

    BEST_TIME_PRE(run_test(binary_search_leaf_prefetch,
                       &array, n_elems,
                       &searches, n_searches, cache_cold),
                  cache_flush(array, n_elems),
                  expected_finds, repeat, n_searches);

    // clang-format on

    return EXIT_SUCCESS;
}
