#define _GNU_SOURCE
#include <roaring/roaring.h>
#include <stdio.h>
#include "benchmark.h"
static inline int quickfull() {
    printf("The naive approach works well when the bitmaps quickly become full\n");
    uint64_t cycles_start, cycles_final;
    size_t bitmapcount = 100;
    size_t size = 1000000;
    roaring_bitmap_t **bitmaps =
        (roaring_bitmap_t **)malloc(sizeof(roaring_bitmap_t *) * bitmapcount);
    for (size_t i = 0; i < bitmapcount; i++) {
        bitmaps[i] = roaring_bitmap_from_range(0, 1000000, 1);
        for (size_t j = 0; j < size / 20; j++)
            roaring_bitmap_remove(bitmaps[i], rand() % size);
        roaring_bitmap_run_optimize(bitmaps[i]);
    }

    RDTSC_START(cycles_start);
    roaring_bitmap_t *answer0 = roaring_bitmap_or_many_heap(bitmapcount, (const roaring_bitmap_t **)bitmaps);
    RDTSC_FINAL(cycles_final);
    printf("%f cycles per union (many heap) \n",
           (cycles_final - cycles_start) * 1.0 / bitmapcount);

    RDTSC_START(cycles_start);
    roaring_bitmap_t *answer1 = roaring_bitmap_or_many(bitmapcount, (const roaring_bitmap_t **)bitmaps);
    RDTSC_FINAL(cycles_final);
    printf("%f cycles per union (many) \n",
           (cycles_final - cycles_start) * 1.0 / bitmapcount);

    RDTSC_START(cycles_start);
    roaring_bitmap_t *answer2  = roaring_bitmap_copy(bitmaps[0]);
    for (size_t i = 1; i < bitmapcount; i++) {
        roaring_bitmap_or_inplace(answer2, bitmaps[i]);
    }
    RDTSC_FINAL(cycles_final);
    printf("%f cycles per union (naive) \n",
           (cycles_final - cycles_start) * 1.0 / bitmapcount);

    for (size_t i = 0; i < bitmapcount; i++) {
        roaring_bitmap_free(bitmaps[i]);
    }
    free(bitmaps);
    roaring_bitmap_free(answer0);
    roaring_bitmap_free(answer1);
    roaring_bitmap_free(answer2);
    return 0;
}

static inline int notsofull() {
    printf("The naive approach works less well when the bitmaps do not quickly become full\n");
    uint64_t cycles_start, cycles_final;
    size_t bitmapcount = 100;
    size_t size = 1000000;
    roaring_bitmap_t **bitmaps =
        (roaring_bitmap_t **)malloc(sizeof(roaring_bitmap_t *) * bitmapcount);
    for (size_t i = 0; i < bitmapcount; i++) {
        bitmaps[i] = roaring_bitmap_from_range(0, 1000000, 100);
        for (size_t j = 0; j < size / 20; j++)
            roaring_bitmap_remove(bitmaps[i], rand() % size);
        roaring_bitmap_run_optimize(bitmaps[i]);
    }

    RDTSC_START(cycles_start);
    roaring_bitmap_t *answer0 = roaring_bitmap_or_many_heap(bitmapcount, (const roaring_bitmap_t **)bitmaps);
    RDTSC_FINAL(cycles_final);
    printf("%f cycles per union (many heap) \n",
           (cycles_final - cycles_start) * 1.0 / bitmapcount);

    RDTSC_START(cycles_start);
    roaring_bitmap_t *answer1 = roaring_bitmap_or_many(bitmapcount, (const roaring_bitmap_t **)bitmaps);
    RDTSC_FINAL(cycles_final);
    printf("%f cycles per union (many) \n",
           (cycles_final - cycles_start) * 1.0 / bitmapcount);

    RDTSC_START(cycles_start);
    roaring_bitmap_t *answer2  = roaring_bitmap_copy(bitmaps[0]);
    for (size_t i = 1; i < bitmapcount; i++) {
        roaring_bitmap_or_inplace(answer2, bitmaps[i]);
    }
    RDTSC_FINAL(cycles_final);
    printf("%f cycles per union (naive) \n",
           (cycles_final - cycles_start) * 1.0 / bitmapcount);

    for (size_t i = 0; i < bitmapcount; i++) {
        roaring_bitmap_free(bitmaps[i]);
    }
    free(bitmaps);
    roaring_bitmap_free(answer0);
    roaring_bitmap_free(answer1);
    roaring_bitmap_free(answer2);
    return 0;
}


int main() {
    printf("How to best aggregate the bitmaps is data-sensitive.\n");
    quickfull();
    notsofull();
    return 0;
}
