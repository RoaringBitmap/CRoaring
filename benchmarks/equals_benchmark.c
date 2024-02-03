#include <stdio.h>
#include <time.h>

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/mixed_equal.h>
#include <roaring/containers/run.h>
#include <roaring/portability.h>

#include "benchmark.h"
#include "random.h"

static inline int32_t array_container_get_nruns(const array_container_t *c) {
    (void)c;
    return -1;
}
static inline int32_t bitset_container_get_nruns(const bitset_container_t *c) {
    (void)c;
    return -1;
}
static inline int32_t run_container_get_nruns(const run_container_t *c) {
    return c->n_runs;
}

#define BENCHMARK_CONTAINER(cname1, cname2, fname, n)             \
    {                                                             \
        cname1##_container_t *c1 = cname1##_container_create();   \
        cname2##_container_t *c2 = cname2##_container_create();   \
        uint16_t *values = malloc(0x10000 * sizeof(uint16_t));    \
        for (uint32_t i = 0; i < 0x10000; i++) {                  \
            values[i] = i;                                        \
        }                                                         \
        shuffle_uint16(values, 0x10000);                          \
        for (uint32_t i = 0; i < n; i++) {                        \
            cname1##_container_add(c1, values[i]);                \
            cname2##_container_add(c2, values[i]);                \
        }                                                         \
        free(values);                                             \
        printf("[Size:%5u] ", n);                                 \
        printf("[NRuns:%5d] ", cname1##_container_get_nruns(c1)); \
        printf("[NRuns:%5d] ", cname2##_container_get_nruns(c2)); \
        BEST_TIME(fname(c1, c2), true, repeat, 1);                \
        cname1##_container_free(c1);                              \
        cname2##_container_free(c2);                              \
    }

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    const int repeat = 100 * 1000;

    BENCHMARK_CONTAINER(array, array, array_container_equals, 64);
    BENCHMARK_CONTAINER(array, array, array_container_equals, DEFAULT_MAX_SIZE);
    BENCHMARK_CONTAINER(array, array, array_container_equals,
                        2 * DEFAULT_MAX_SIZE);
    BENCHMARK_CONTAINER(bitset, bitset, bitset_container_equals, 65535);
    BENCHMARK_CONTAINER(bitset, bitset, bitset_container_equals, 65536);
    BENCHMARK_CONTAINER(run, run, run_container_equals, DEFAULT_MAX_SIZE / 2);
    BENCHMARK_CONTAINER(run, run, run_container_equals, DEFAULT_MAX_SIZE);
    BENCHMARK_CONTAINER(run, array, run_container_equals_array,
                        DEFAULT_MAX_SIZE);
    BENCHMARK_CONTAINER(array, bitset, array_container_equal_bitset,
                        DEFAULT_MAX_SIZE);
    BENCHMARK_CONTAINER(run, bitset, run_container_equals_bitset,
                        DEFAULT_MAX_SIZE);

    return 0;
}
