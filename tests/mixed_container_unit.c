/*
 * array_container_unit.c
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "containers/mixed_intersection.h"
#include "containers/mixed_union.h"

#include "misc/configreport.h"

#include "test.h"

// returns 1 if ok.
int array_bitset_and_or_test() {
    DESCRIBE_TEST;

    array_container_t* A1 = array_container_create();
    array_container_t* A2 = array_container_create();
    array_container_t* AI = array_container_create();
    array_container_t* AO = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BI = bitset_container_create();
    bitset_container_t* BO = bitset_container_create();

    for (int x = 0; x < (1 << 16); x += 3) {
        array_container_add(A1, (uint16_t)x);
        array_container_add(AO, (uint16_t)x);
        bitset_container_set(B1, (uint16_t)x);
        bitset_container_set(BO, (uint16_t)x);
    }
    for (int x = 0; x < (1 << 16);
         x += 62) {  // important: 62 is not divisible by 3
        array_container_add(A2, (uint16_t)x);
        array_container_add(AO, (uint16_t)x);
        bitset_container_set(B2, (uint16_t)x);
        bitset_container_set(BO, (uint16_t)x);
    }
    for (int x = 0; x < (1 << 16); x += 62 * 3) {
        array_container_add(AI, (uint16_t)x);
        bitset_container_set(BI, (uint16_t)x);
    }

    // we interleave O and I on purpose (to trigger bugs!)
    int ci = array_container_cardinality(AI);  // expected intersection
    int co = array_container_cardinality(AO);  // expected union
    assert(ci == bitset_container_cardinality(BI));
    assert(co == bitset_container_cardinality(BO));
    array_container_intersection(A1, A2, AI);
    array_container_union(A1, A2, AO);
    bitset_container_intersection(B1, B2, BI);
    bitset_container_union(B1, B2, BO);
    assert(ci == bitset_container_cardinality(BI));
    assert(co == bitset_container_cardinality(BO));
    assert(ci == array_container_cardinality(AI));
    assert(co == array_container_cardinality(AO));
    array_bitset_container_intersection(A1, B2, AI);
    assert(ci == array_container_cardinality(AI));
    array_bitset_container_intersection(A2, B1, AI);
    assert(ci == array_container_cardinality(AI));
    array_bitset_container_union(A1, B2, BO);
    assert(co == bitset_container_cardinality(BO));
    array_bitset_container_union(A2, B1, BO);
    assert(co == bitset_container_cardinality(BO));
    array_container_free(A1);
    array_container_free(A2);
    array_container_free(AI);
    array_container_free(AO);
    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(BI);
    bitset_container_free(BO);

    return 1;
}

int main() {
    tellmeall();
    if (!array_bitset_and_or_test()) return -1;

    return EXIT_SUCCESS;
}
