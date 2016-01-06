/*
 * array_container_unit.c
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "containers/array.h"
#include "benchmark.h"

int add_test(array_container_t* B) {
    int x;
    for (x = 0; x < 1 << 16; x += 3) {
        array_container_set(B, (uint16_t)x);
    }
    return 0;
}

int remove_test(array_container_t* B) {
    int x;
    for (x = 0; x < 1 << 16; x += 3) {
        array_container_unset(B, (uint16_t)x);
    }
    return 0;
}
int contains_test(array_container_t* B) {
    int card = 0;
    int x;
    for (x = 0; x < 1 << 16; x++) {
        card += array_container_contains(B, (uint16_t)x);
    }
    return card;
}

int main() {
    int repeat = 5000;
    int size = (1 << 16) / 3;
    printf("array container benchmarks\n");
    array_container_t* B = array_container_create();
    BEST_TIME(add_test(B), 0, repeat, size);
    int answer = get_test(B);
    size = 1 << 16;
    BEST_TIME(contains_test(B), answer, repeat, size);

    size = (1 << 16) / 3;
    BEST_TIME(remove_test(B), 0, repeat, size);
    array_container_free(B);

    array_container_t* B1 = array_container_create();
    for (int x = 0; x < 1 << 16; x += 3) {
        array_container_add(B1, (uint16_t)x);
    }
    array_container_t* B2 = array_container_create();
    for (int x = 0; x < 1 << 16; x += 5) {
        array_container_add(B2, (uint16_t)x);
    }
    array_container_t* BO = array_container_create();
    BEST_TIME(array_container_or(B1, B2, BO), -1, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    answer = array_container_compute_cardinality(BO);
    BEST_TIME(array_container_or(B1, B2, BO), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    BEST_TIME(array_container_cardinality(BO), answer, repeat, 1);
    BEST_TIME(array_container_compute_cardinality(BO), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    BEST_TIME(array_container_and_nocard(B1, B2, BO), -1, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    answer = array_container_compute_cardinality(BO);
    BEST_TIME(array_container_and(B1, B2, BO), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    BEST_TIME(array_container_cardinality(BO), answer, repeat, 1);
    BEST_TIME(array_container_compute_cardinality(BO), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);

    return 0;
}
