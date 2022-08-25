#define _GNU_SOURCE
#include <roaring/roaring.h>
#include <stdio.h>
#include "benchmark.h"
#include <stdio.h>

// see https://github.com/saulius/croaring-rs/issues/6#issuecomment-243341270
int main() {
    size_t N = 1000000;
    uint64_t cycles_start, cycles_final;

    RDTSC_START(cycles_start);
    for (size_t i = 0; i < N; i++) {
        roaring_bitmap_t* bm = roaring_bitmap_create();
        roaring_bitmap_free(bm);
    }
    RDTSC_FINAL(cycles_final);
    printf("%f cycles per object created \n",
           (cycles_final - cycles_start) * 1.0 / N);
    return 0;
}
