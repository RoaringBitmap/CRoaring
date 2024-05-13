#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>

#include <roaring/roaring.h>

#include "benchmark.h"
#include "random.h"

enum order_e { ASC, DESC, SHUFFLE };
typedef enum order_e order_t;

/*
 * Generate a number of non-overlapping intervals inside [0, $spanlen),
 * each interval is of length $intvlen.
 * Returns number of intervals and their offsets.
 *
 * TODO also generate overlapping intervals
 */
void make_data(uint32_t spanlen, uint32_t intvlen, double density,
               order_t order, uint32_t **offsets_out, uint32_t *count_out) {
    uint32_t count = floor(spanlen * density / intvlen);
    uint32_t *offsets = malloc(count * sizeof(uint32_t));

    uint64_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        offsets[i] = pcg32_random_r(&pcg32_global);
        sum += offsets[i];
    }
    for (uint32_t i = 0; i < count; i++) {
        double v = offsets[i];
        v /= sum;
        v *= (spanlen - count * intvlen) / (double)intvlen;
        uint32_t gap = floor(v);

        if (i == 0) {
            offsets[i] = gap;
        } else {
            offsets[i] = offsets[i - 1] + intvlen + gap;
        }
    }

    if (order == SHUFFLE) {
        shuffle_uint32(offsets, count);
    } else if (order == DESC) {
        for (uint32_t i = 0, j = count - 1; i < count / 2; i++, j--) {
            uint32_t tmp = offsets[i];
            offsets[i] = offsets[j];
            offsets[j] = tmp;
        }
    }

    *offsets_out = offsets;
    *count_out = count;
}

double array_min(double *arr, size_t len) {
    double min = arr[0];
    for (size_t i = 1; i < len; i++) {
        if (arr[i] < min) {
            min = arr[i];
        }
    }
    return min;
}

void run_test(uint32_t spanlen, uint32_t intvlen, double density,
              order_t order) {
    printf("intvlen=%u density=%f order=%s\n", intvlen, density,
           (order == ASC
                ? "ASC"
                : (order == DESC ? "DESC"
                                 : (order == SHUFFLE ? "SHUFFLE" : "???"))));

    uint32_t *offsets;
    uint32_t count;
    make_data(spanlen, intvlen, density, order, &offsets, &count);
    const int num_passes = 5;
    uint64_t cycles_start, cycles_final;
    double results[num_passes];

    printf("  roaring_bitmap_add():");
    for (int p = 0; p < num_passes; p++) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        RDTSC_START(cycles_start);
        for (int64_t i = 0; i < count; i++) {
            for (uint32_t j = 0; j < intvlen; j++) {
                roaring_bitmap_add(r, offsets[i] + j);
            }
        }
        RDTSC_FINAL(cycles_final);
        results[p] = (cycles_final - cycles_start) * 1.0 / count / intvlen;
        roaring_bitmap_free(r);
    }
    printf("          %6.1f\n", array_min(results, num_passes));

    printf("  roaring_bitmap_add_many():");
    for (int p = 0; p < num_passes; p++) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        uint32_t values[intvlen * count];
        for (int64_t i = 0; i < count; i++) {
            for (uint32_t j = 0; j < intvlen; j++) {
                values[i * intvlen + j] = offsets[i] + j;
            }
        }
        RDTSC_START(cycles_start);
        for (int64_t i = 0; i < count; i++) {
            roaring_bitmap_add_many(r, intvlen, values + (i * intvlen));
        }
        RDTSC_FINAL(cycles_final);
        results[p] = (cycles_final - cycles_start) * 1.0 / count / intvlen;
        roaring_bitmap_free(r);
    }
    printf("     %6.1f\n", array_min(results, num_passes));

    printf("  roaring_bitmap_add_bulk():");
    for (int p = 0; p < num_passes; p++) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        RDTSC_START(cycles_start);
        roaring_bulk_context_t context = {0, 0, 0, 0};
        for (int64_t i = 0; i < count; i++) {
            for (uint32_t j = 0; j < intvlen; j++) {
                roaring_bitmap_add_bulk(r, &context, offsets[i] + j);
            }
        }
        RDTSC_FINAL(cycles_final);
        results[p] = (cycles_final - cycles_start) * 1.0 / count / intvlen;
        roaring_bitmap_free(r);
    }
    printf("     %6.1f\n", array_min(results, num_passes));

    printf("  roaring_bitmap_add_range():");
    for (int p = 0; p < num_passes; p++) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        RDTSC_START(cycles_start);
        for (int64_t i = 0; i < count; i++) {
            roaring_bitmap_add_range(r, offsets[i], offsets[i] + intvlen);
        }
        RDTSC_FINAL(cycles_final);
        results[p] = (cycles_final - cycles_start) * 1.0 / count / intvlen;
        roaring_bitmap_free(r);
    }
    printf("    %6.1f\n", array_min(results, num_passes));

    printf("  roaring_bitmap_remove():");
    for (int p = 0; p < num_passes; p++) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        roaring_bitmap_add_range(r, 0, spanlen);
        RDTSC_START(cycles_start);
        for (int64_t i = 0; i < count; i++) {
            for (uint32_t j = 0; j < intvlen; j++) {
                roaring_bitmap_remove(r, offsets[i] + j);
            }
        }
        RDTSC_FINAL(cycles_final);
        results[p] = (cycles_final - cycles_start) * 1.0 / count / intvlen;
        roaring_bitmap_free(r);
    }
    printf("       %6.1f\n", array_min(results, num_passes));

    printf("  roaring_bitmap_remove_range():");
    for (int p = 0; p < num_passes; p++) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        roaring_bitmap_add_range(r, 0, spanlen);
        RDTSC_START(cycles_start);
        for (int64_t i = 0; i < count; i++) {
            roaring_bitmap_remove_range(r, offsets[i], offsets[i] + intvlen);
        }
        RDTSC_FINAL(cycles_final);
        results[p] = (cycles_final - cycles_start) * 1.0 / count / intvlen;
        roaring_bitmap_free(r);
    }
    printf(" %6.1f\n", array_min(results, num_passes));

    free(offsets);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    const uint32_t spanlen = 1000 * 1000;
    uint32_t intvlen_array[] = {1, 4, 16, 64};
    order_t order_array[] = {SHUFFLE, ASC, DESC};
    printf("[cycles/element]\n");
    for (size_t r = 0; r < sizeof(order_array) / sizeof(order_array[0]); r++) {
        for (size_t i = 0; i < sizeof(intvlen_array) / sizeof(intvlen_array[0]);
             i++) {
            run_test(spanlen, intvlen_array[i], 0.2, order_array[r]);
        }
        printf("\n");
    }

    return 0;
}
