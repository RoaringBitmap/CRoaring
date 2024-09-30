#define _GNU_SOURCE
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/roaring.h>

#include "benchmark.h"
#include "numbersfromtextfiles.h"
#include "random.h"

void contains_multi_via_contains(roaring_bitmap_t* bm, const uint32_t* values,
                                 bool* results, const size_t count) {
    for (size_t i = 0; i < count; ++i) {
        results[i] = roaring_bitmap_contains(bm, values[i]);
    }
}

void contains_multi_bulk(roaring_bitmap_t* bm, const uint32_t* values,
                         bool* results, const size_t count) {
    roaring_bulk_context_t context = CROARING_ZERO_INITIALIZER;
    for (size_t i = 0; i < count; ++i) {
        results[i] = roaring_bitmap_contains_bulk(bm, &context, values[i]);
    }
}

int compare_uint32(const void* a, const void* b) {
    uint32_t arg1 = *(const uint32_t*)a;
    uint32_t arg2 = *(const uint32_t*)b;
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

int main(int argc, char* argv[]) {
    (void)&read_all_integer_files;  // suppress unused warning

    if (argc < 2) {
        printf("Usage: %s <comma_separated_integers_file> ...\n", argv[0]);
        printf("Example: %s ~/CRoaring/benchmarks/realdata/weather_sept_85/*\n",
               argv[0]);
        return 1;
    }

    size_t fields = argc - 1;
    uint32_t* values[argc];
    size_t count[argc];

    roaring_bitmap_t* bm = roaring_bitmap_create();
    for (int i = 1; i < argc; i++) {
        size_t t_count = 0;
        uint32_t* t_values = read_integer_file(argv[i], &t_count);
        if (t_count == 0) {
            printf("No integers found in %s\n", argv[i]);
            return 1;
        }
        roaring_bitmap_add_many(bm, t_count, t_values);

        shuffle_uint32(t_values, t_count);

        values[i - 1] = t_values;
        count[i - 1] = t_count;
    }
    // roaring_bitmap_run_optimize(bm);

    printf("Data:\n");
    printf("  cardinality: %" PRIu64 "\n", roaring_bitmap_get_cardinality(bm));
    printf("  buckets: %d\n", (int)bm->high_low_container.size);
    printf("  range: %" PRIu32 "-%" PRIu32 "\n",
           roaring_bitmap_minimum(bm) >> 16, roaring_bitmap_maximum(bm) >> 16);

    const int num_passes = 10;
    printf("Cycles/element: %d\n", num_passes);
    uint64_t cycles_start, cycles_final;

    printf("                          roaring_bitmap_contains:");
    for (int p = 0; p < num_passes; p++) {
        bool result[count[p]];
        RDTSC_START(cycles_start);
        contains_multi_via_contains(bm, values[p], result, count[p]);
        RDTSC_FINAL(cycles_final);
        printf(" %10f", (cycles_final - cycles_start) * 1.0 / count[p]);
    }
    printf("\n");

    printf("                     roaring_bitmap_contains_bulk:");
    for (int p = 0; p < num_passes; p++) {
        bool result[count[p]];
        RDTSC_START(cycles_start);
        contains_multi_bulk(bm, values[p], result, count[p]);
        RDTSC_FINAL(cycles_final);
        printf(" %10f", (cycles_final - cycles_start) * 1.0 / count[p]);
    }
    printf("\n");

    // sort input array
    for (size_t i = 0; i < fields; ++i) {
        qsort(values[i], count[i], sizeof(uint32_t), compare_uint32);
    }

    printf("        roaring_bitmap_contains with sorted input:");
    for (int p = 0; p < num_passes; p++) {
        bool result[count[p]];
        RDTSC_START(cycles_start);
        contains_multi_via_contains(bm, values[p], result, count[p]);
        RDTSC_FINAL(cycles_final);
        printf(" %10f", (cycles_final - cycles_start) * 1.0 / count[p]);
    }
    printf("\n");

    printf("   roaring_bitmap_contains_bulk with sorted input:");
    for (int p = 0; p < num_passes; p++) {
        bool result[count[p]];
        RDTSC_START(cycles_start);
        contains_multi_bulk(bm, values[p], result, count[p]);
        RDTSC_FINAL(cycles_final);
        printf(" %10f", (cycles_final - cycles_start) * 1.0 / count[p]);
    }
    printf("\n");

    roaring_bitmap_free(bm);
    for (size_t i = 0; i < fields; ++i) {
        free(values[i]);
    }
    return 0;
}
