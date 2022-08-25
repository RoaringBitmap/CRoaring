#define _GNU_SOURCE
#include <math.h>
#include <roaring/roaring.h>
#include <stdio.h>

#include "benchmark.h"
#include "random.h"

static roaring_bitmap_t *make_random_bitmap(uint32_t range[3]) {
    uint32_t start = ranged_random((1UL << 32) - 2);
    uint32_t stop = ranged_random((1UL << 32) - 1);
    uint32_t step = ranged_random((1UL << 16) - 1);

    if (start > stop) {
        start ^= stop;
        stop ^= start;
        start ^= stop;
    }
    if (start == stop) {
        stop++;
    }

    range[0] = start;
    range[1] = stop;
    range[2] = step;

    return roaring_bitmap_from_range(start, stop, step);
}

static void make_random_range(uint32_t range[2]) {
    uint32_t start = ranged_random((1UL << 32) - 2);
    uint32_t stop = ranged_random((1UL << 32) - 1);

    if (start > stop) {
        start ^= stop;
        stop ^= start;
        start ^= stop;
    }
    if (start == stop) {
        stop++;
    }

    range[0] = start;
    range[1] = stop;
}

typedef struct testvalue_s {
    roaring_bitmap_t *bitmap;
    uint32_t bitmap_range[3];
    uint32_t range[2];
    bool expected;
} testvalue_t;

static void pre(void *base) { (void)base; }

static bool naive_intersect(void *base, testvalue_t tv) {
    (void)base;
    const roaring_bitmap_t *bm = tv.bitmap;
    roaring_bitmap_t *range =
        roaring_bitmap_from_range(tv.range[0], tv.range[1], 1);
    bool res = roaring_bitmap_intersect(bm, range);
    roaring_bitmap_free(range);
    return res;
}

static const bool paranoid = true;

static bool range_intersect(void *base, testvalue_t tv) {
    (void)base;
    bool res = roaring_bitmap_intersect_with_range(tv.bitmap, tv.range[0],
                                                   tv.range[1]);
    if (paranoid && res != tv.expected) {
        printf(
            "ERROR: expected %s but got %s for intersection of bitmap "
            "[%u,%u,%u] with range [%u,%u]\n",
            tv.expected ? "'true'" : "'false'", res ? "'true'" : "'false'",
            tv.bitmap_range[0], tv.bitmap_range[1], tv.bitmap_range[2],
            tv.range[0], tv.range[1]);
    }
    return res;
}

#define NUM_SAMPLES  100
static void run_test() {
    static testvalue_t testvalues[NUM_SAMPLES];

    for (int i = 0; i < NUM_SAMPLES; i++) {
        testvalues[i].bitmap = make_random_bitmap(testvalues[i].bitmap_range);
        make_random_range(testvalues[i].range);
        testvalues[i].expected = naive_intersect(NULL, testvalues[i]);
    }

    printf("  roaring_bitmap_from_range():\n");
    BEST_TIME_PRE_ARRAY(NULL, naive_intersect, pre, testvalues, NUM_SAMPLES);

    printf("  roaring_bitmap_intersect_with_range():\n");
    BEST_TIME_PRE_ARRAY(NULL, range_intersect, pre, testvalues, NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        roaring_bitmap_free(testvalues[i].bitmap);
    }

    testvalue_t pathologic[1] = {
        {
            .bitmap = roaring_bitmap_from_range(0, (1UL << 32) - 2, 2),
            .bitmap_range = {0, (1UL << 32) - 2, 2},
            .range = {(1UL << 32) - 3, (1UL << 32) - 1},
        },
    };
    pathologic[0].expected = naive_intersect(NULL, pathologic[0]);

    printf("  roaring_bitmap_from_range():\n");
    BEST_TIME_PRE_ARRAY(NULL, naive_intersect, pre, pathologic, 1);

    printf("  roaring_bitmap_intersect_with_range():\n");
    BEST_TIME_PRE_ARRAY(NULL, range_intersect, pre, pathologic, 1);

    roaring_bitmap_free(pathologic[0].bitmap);
}

int main(void) {
    run_test();
    return 0;
}
