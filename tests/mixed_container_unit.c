/*
 * mixed_container_unit.c
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "containers/mixed_intersection.h"
#include "containers/mixed_union.h"
#include "containers/mixed_negation.h"
#include "containers/containers.h"

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

    // nb, array containers will be illegally big.
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

int array_negation_empty_test() {
    printf("[%s] %s\n", __FILE__, __func__);
    array_container_t* AI = array_container_create();
    bitset_container_t* BO = bitset_container_create();

    array_container_negation(AI, BO);

    assert(bitset_container_cardinality(BO) == (1 << 16));

    array_container_free(AI);
    bitset_container_free(BO);
    return 1;
}

int array_negation_test() {
    int ctr = 0;
    printf("[%s] %s\n", __FILE__, __func__);
    array_container_t* AI = array_container_create();
    bitset_container_t* BO = bitset_container_create();

    for (int x = 0; x < (1 << 16); x += 29) {
        array_container_add(AI, (uint16_t)x);
        ++ctr;
    }

    array_container_negation(AI, BO);
    assert(bitset_container_cardinality(BO) == (1 << 16) - ctr);

    for (int x = 0; x < (1 << 16); x++) {
        if (x % 29 == 0) {
            assert(!bitset_container_contains(BO, (uint16_t)x));
        } else {
            assert(bitset_container_contains(BO, (uint16_t)x));
        }
        array_container_add(AI, (uint16_t)x);
        ++ctr;
    }

    array_container_free(AI);
    bitset_container_free(BO);

    return 1;
}

static int array_negation_range_test(int r_start, int r_end, bool is_bitset) {
    bool result_is_bitset;
    int result_size_should_be = 0;

    array_container_t* AI = array_container_create();
    void* BO;  // bitset or array

    for (int x = 0; x < (1 << 16); x += 29) {
        array_container_add(AI, (uint16_t)x);
    }

    for (int x = 0; x < (1 << 16); x++) {
        if (x >= r_start && x < r_end)
            if (x % 29 != 0)
                result_size_should_be++;
            else {
            }
        else if (x % 29 == 0)
            result_size_should_be++;
    }

    result_is_bitset =
        array_container_negation_range(AI, r_start, r_end, (void**)&BO);
    uint8_t result_typecode = (result_is_bitset ? BITSET_CONTAINER_TYPE_CODE
                                                : ARRAY_CONTAINER_TYPE_CODE);

    int result_card = container_get_cardinality(BO, result_typecode);
    printf("I think size should be %d and card is %d\n", result_size_should_be,
           result_card);

    assert(is_bitset == result_is_bitset);
    assert(result_size_should_be == result_card);

    for (int x = 0; x < (1 << 16); x++) {
        bool should_be_present;
        if (x >= r_start && x < r_end)
            should_be_present = (x % 29 != 0);
        else
            should_be_present = (x % 29 == 0);

        if (should_be_present !=
            container_contains(BO, (uint16_t)x, result_typecode))
            printf("oops on %d\n", x);

        // assert(container_contains(BO, (uint16_t)x, result_typecode) ==
        // should_be_present);
    }
    container_free(BO, result_typecode);
    array_container_free(AI);
    return 1;
}

/* result is a bitset.  Range fits neatly in words */
int array_negation_range_test1() {
    printf("[%s] %s\n", __FILE__, __func__);
    return array_negation_range_test(0x4000, 0xc000, true);
}

/* result is a bitset.  Range begins and ends mid word */
int array_negation_range_test1a() {
    printf("[%s] %s\n", __FILE__, __func__);
    return array_negation_range_test(0x4010, 0xc010, true);
}
/* result is an array */
int array_negation_range_test2() {
    printf("[%s] %s\n", __FILE__, __func__);
    return array_negation_range_test(0x7f00, 0x8030, false);
}
/* Empty range.  result is a clone */
int array_negation_range_test3() {
    printf("[%s] %s\n", __FILE__, __func__);
    return array_negation_range_test(0x7800, 0x7800, false);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(array_bitset_and_or_test),
        cmocka_unit_test(array_negation_empty_test),
        cmocka_unit_test(array_negation_test),
        cmocka_unit_test(array_negation_range_test1),
        cmocka_unit_test(array_negation_range_test1a),
        cmocka_unit_test(array_negation_range_test2),
        cmocka_unit_test(array_negation_range_test3),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
