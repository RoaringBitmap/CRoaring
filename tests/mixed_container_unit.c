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

#include "test.h"

void array_bitset_and_or_test() {
    array_container_t* A1 = array_container_create();
    array_container_t* A2 = array_container_create();
    array_container_t* AI = array_container_create();
    array_container_t* AO = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BI = bitset_container_create();
    bitset_container_t* BO = bitset_container_create();

    for (int x = 0; x < (1 << 16); x += 3) {
        array_container_add(A1, x);
        array_container_add(AO, x);
        bitset_container_set(B1, x);
        bitset_container_set(BO, x);
    }

    // important: 62 is not divisible by 3
    for (int x = 0; x < (1 << 16); x += 62) {
        array_container_add(A2, x);
        array_container_add(AO, x);
        bitset_container_set(B2, x);
        bitset_container_set(BO, x);
    }

    for (int x = 0; x < (1 << 16); x += 62 * 3) {
        array_container_add(AI, x);
        bitset_container_set(BI, x);
    }

    // we interleave O and I on purpose (to trigger bugs!)
    int ci = array_container_cardinality(AI);  // expected intersection
    int co = array_container_cardinality(AO);  // expected union

    assert_int_equal(ci, bitset_container_cardinality(BI));
    assert_int_equal(co, bitset_container_cardinality(BO));

    array_container_intersection(A1, A2, AI);
    array_container_union(A1, A2, AO);
    bitset_container_intersection(B1, B2, BI);
    bitset_container_union(B1, B2, BO);

    assert_int_equal(ci, bitset_container_cardinality(BI));
    assert_int_equal(co, bitset_container_cardinality(BO));
    assert_int_equal(ci, array_container_cardinality(AI));
    assert_int_equal(co, array_container_cardinality(AO));

    array_bitset_container_intersection(A1, B2, AI);
    assert_int_equal(ci, array_container_cardinality(AI));

    array_bitset_container_intersection(A2, B1, AI);
    assert_int_equal(ci, array_container_cardinality(AI));

    array_bitset_container_union(A1, B2, BO);
    assert_int_equal(co, bitset_container_cardinality(BO));

    array_bitset_container_union(A2, B1, BO);
    assert_int_equal(co, bitset_container_cardinality(BO));

    array_container_free(A1);
    array_container_free(A2);
    array_container_free(AI);
    array_container_free(AO);

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(BI);
    bitset_container_free(BO);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(array_bitset_and_or_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
