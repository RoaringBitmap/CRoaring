#define _GNU_SOURCE
#include <roaring/roaring.h>
#include <inttypes.h>
#include "benchmark.h"
#include "numbersfromtextfiles.h"

void iterate_using_advance(roaring_bitmap_t* bm) {
    roaring_uint32_iterator_t *iter = roaring_iterator_create(bm);
    uint64_t sum = 0;
    while (iter->has_value) {
        sum += iter->current_value;
        roaring_uint32_iterator_advance(iter);
    }
    roaring_uint32_iterator_free(iter);
    *(volatile uint64_t*)(&sum) = sum;
}

void iterate_using_read(roaring_bitmap_t* bm, uint32_t bufsize) {
    uint32_t* buffer = malloc(sizeof(uint32_t) * bufsize);
    roaring_uint32_iterator_t *iter = roaring_iterator_create(bm);
    uint64_t sum = 0;
    while (1) {
        uint32_t ret = roaring_uint32_iterator_read(iter, buffer, bufsize);
        for (uint32_t i = 0; i < ret; i++) {
            sum += buffer[i];
        }
        if (ret < bufsize) {
            break;
        }
    }
    roaring_uint32_iterator_free(iter);
    free(buffer);
    *(volatile uint64_t*)(&sum) = sum;
}

int main(int argc, char* argv[]) {
    (void)&read_all_integer_files; // suppress unused warning

    if (argc < 2) {
        printf("Usage: %s <comma_separated_integers_file> ...\n", argv[0]);
        printf("Example: %s ~/CRoaring/benchmarks/realdata/weather_sept_85/*\n", argv[0]);
        return 1;
    }

    roaring_bitmap_t* bm = roaring_bitmap_create();
    for (int i = 1; i < argc; i++) {
        size_t count = 0;
        uint32_t* values = read_integer_file(argv[i], &count);
        if (count == 0) {
            printf("No integers found in %s\n", argv[i]);
            return 1;
        }
        //roaring_bitmap_add_many(bm, count, values);
        for (size_t j = 0; j < count; j++) {
            roaring_bitmap_add(bm, values[j]);
        }
        free(values);
    }
    //roaring_bitmap_run_optimize(bm);

    printf("Data:\n");
    printf("  cardinality: %"PRIu64"\n", roaring_bitmap_get_cardinality(bm));

    printf("Cycles/element:\n");
    uint64_t cycles_start, cycles_final;
    const int num_passes = 5;

    printf("  roaring_uint32_iterator_advance():");
    for (int p = 0; p < num_passes; p++) {
        RDTSC_START(cycles_start);
        iterate_using_advance(bm);
        RDTSC_FINAL(cycles_final);
        printf(" %f", (cycles_final - cycles_start) * 1.0 / roaring_bitmap_get_cardinality(bm));
    }
    printf("\n");

    const uint32_t bufsizes[] = {1,4,16,128,1024};
    for (size_t j = 0; j < sizeof(bufsizes)/sizeof(bufsizes[0]); j++) {
        uint32_t bufsize = bufsizes[j];
        printf("  roaring_uint32_iterator_read(bufsize=%u):", bufsize);
        for (int p = 0; p < num_passes; p++) {
            RDTSC_START(cycles_start);

            iterate_using_read(bm, bufsize);

            RDTSC_FINAL(cycles_final);
            printf(" %f", (cycles_final - cycles_start) * 1.0 / roaring_bitmap_get_cardinality(bm));
        }
        printf("\n");
    }

    roaring_bitmap_free(bm);
    return 0;
}

