/*
 * container_comparison_unit.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/containers.h>
#include <roaring/containers/mixed_equal.h>
#include <roaring/containers/mixed_subset.h>
#include <roaring/containers/run.h>
#include <roaring/misc/configreport.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
    using namespace roaring::internal;
#endif

#include "test.h"


static inline void container_checked_add(container_t *container, uint16_t val,
                                         uint8_t typecode) {
    uint8_t new_type;
    container_t *new_container = container_add(container, val, typecode, &new_type);
    assert_int_equal(typecode, new_type);
    assert_true(container == new_container);
}

static inline void delegated_add(container_t *container, uint8_t typecode,
                                 uint16_t val) {
    switch(typecode) {
        case BITSET_CONTAINER_TYPE:
            bitset_container_add(CAST_bitset(container), val);
            break;
        case ARRAY_CONTAINER_TYPE:
            array_container_add(CAST_array(container), val);
            break;
        case RUN_CONTAINER_TYPE:
            run_container_add(CAST_run(container), val);
            break;
        default:
            assert(false);
            roaring_unreachable;
    }
}

static inline container_t *container_create(uint8_t typecode) {
    container_t *result = NULL;
    switch (typecode) {
        case BITSET_CONTAINER_TYPE:
            result = bitset_container_create();
            break;
        case ARRAY_CONTAINER_TYPE:
            result = array_container_create();
            break;
        case RUN_CONTAINER_TYPE:
            result = run_container_create();
            break;
        default:
            assert(false);
            roaring_unreachable;
    }
    assert_non_null(result);
    return result;
}

void generic_equal_test(uint8_t type1, uint8_t type2) {
    container_t *container1 = container_create(type1);
    container_t *container2 = container_create(type2);
    assert_true(container_equals(container1, type1, container2, type2));
    for (int i = 0; i < 100; i++) {
        container_checked_add(container1, i * 10, type1);
        container_checked_add(container2, i * 10, type2);
        assert_true(container_equals(container1, type1, container2, type2));
    }
    container_checked_add(container1, 273, type1);
    assert_false(container_equals(container1, type1, container2, type2));
    container_checked_add(container2, 854, type2);
    assert_false(container_equals(container1, type1, container2, type2));
    container_checked_add(container1, 854, type1);
    assert_false(container_equals(container1, type1, container2, type2));
    container_checked_add(container2, 273, type2);
    assert_true(container_equals(container1, type1, container2, type2));
    container_free(container1, type1);
    container_free(container2, type2);

    // full container
    container1 = container_create(type1);
    container2 = container_create(type2);
    for (uint32_t i = 0; i < 65536; i++) {
        delegated_add(container1, type1, i);
        delegated_add(container2, type2, i);
    }
    assert_true(container_equals(container1, type1, container2, type2));
    container_free(container1, type1);
    container_free(container2, type2);

    // first elements differ
    container1 = container_create(type1);
    container2 = container_create(type2);
    for (int i = 0; i < 65536; i++) {
        if (i != 0) delegated_add(container1, type1, i);
        if (i != 1) delegated_add(container2, type2, i);
    }
    assert_false(container_equals(container1, type1, container2, type2));
    container_free(container1, type1);
    container_free(container2, type2);

    // last elements differ
    container1 = container_create(type1);
    container2 = container_create(type2);
    for (int i = 0; i < 65536; i++) {
        if (i != 65534) delegated_add(container1, type1, i);
        if (i != 65535) delegated_add(container2, type2, i);
    }
    assert_false(container_equals(container1, type1, container2, type2));
    container_free(container1, type1);
    container_free(container2, type2);
}

DEFINE_TEST(equal_array_array_test) {
    generic_equal_test(ARRAY_CONTAINER_TYPE, ARRAY_CONTAINER_TYPE);
}

DEFINE_TEST(equal_bitset_bitset_test) {
    generic_equal_test(BITSET_CONTAINER_TYPE, BITSET_CONTAINER_TYPE);
}

DEFINE_TEST(equal_run_run_test) {
    generic_equal_test(RUN_CONTAINER_TYPE, RUN_CONTAINER_TYPE);
}

DEFINE_TEST(equal_array_bitset_test) {
    generic_equal_test(ARRAY_CONTAINER_TYPE, BITSET_CONTAINER_TYPE);
}

DEFINE_TEST(equal_bitset_array_test) {
    generic_equal_test(BITSET_CONTAINER_TYPE, ARRAY_CONTAINER_TYPE);
}

DEFINE_TEST(equal_array_run_test) {
    generic_equal_test(ARRAY_CONTAINER_TYPE, RUN_CONTAINER_TYPE);
}

DEFINE_TEST(equal_run_array_test) {
    generic_equal_test(RUN_CONTAINER_TYPE, ARRAY_CONTAINER_TYPE);
}

DEFINE_TEST(equal_bitset_run_test) {
    generic_equal_test(BITSET_CONTAINER_TYPE, RUN_CONTAINER_TYPE);
}

DEFINE_TEST(equal_run_bitset_test) {
    generic_equal_test(RUN_CONTAINER_TYPE, BITSET_CONTAINER_TYPE);
}

void generic_subset_test(uint8_t type1, uint8_t type2) {
    container_t *container1 = container_create(type1);
    container_t *container2 = container_create(type2);
    assert_true(container_is_subset(container1, type1, container2, type2));
    for (int i = 0; i < 100; i++) {
        container_checked_add(container1, i * 11, type1);
        container_checked_add(container2, i * 11, type2);
        assert_true(container_is_subset(container1, type1, container2, type2));
    }
    for (int i = 0; i < 100; i++) {
        container_checked_add(container2, i * 7, type2);
        assert_true(container_is_subset(container1, type1, container2, type2));
    }
    for (int i = 0; i < 100; i++) {
        if (i % 7 == 0 || i % 11 == 0) continue;
        container_checked_add(container1, i * 5, type1);
        assert_false(container_is_subset(container1, type1, container2, type2));
        container_checked_add(container2, i * 5, type2);
        assert_true(container_is_subset(container1, type1, container2, type2));
    }
    container_free(container1, type1);
    container_free(container2, type2);
}

DEFINE_TEST(subset_array_array_test) {
    generic_subset_test(ARRAY_CONTAINER_TYPE, ARRAY_CONTAINER_TYPE);
}

DEFINE_TEST(subset_bitset_bitset_test) {
    generic_subset_test(BITSET_CONTAINER_TYPE, BITSET_CONTAINER_TYPE);
}

DEFINE_TEST(subset_run_run_test) {
    generic_subset_test(RUN_CONTAINER_TYPE, RUN_CONTAINER_TYPE);
}

DEFINE_TEST(subset_array_bitset_test) {
    generic_subset_test(ARRAY_CONTAINER_TYPE, BITSET_CONTAINER_TYPE);
}

DEFINE_TEST(subset_array_run_test) {
    generic_subset_test(ARRAY_CONTAINER_TYPE, RUN_CONTAINER_TYPE);
}

DEFINE_TEST(subset_run_array_test) {
    generic_subset_test(RUN_CONTAINER_TYPE, ARRAY_CONTAINER_TYPE);
}

DEFINE_TEST(subset_bitset_run_test) {
    generic_subset_test(BITSET_CONTAINER_TYPE, RUN_CONTAINER_TYPE);
}

DEFINE_TEST(subset_run_bitset_test) {
    generic_subset_test(RUN_CONTAINER_TYPE, BITSET_CONTAINER_TYPE);
}

int main() {
    tellmeall();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(equal_array_array_test),
        cmocka_unit_test(equal_bitset_bitset_test),
        cmocka_unit_test(equal_run_run_test),
        cmocka_unit_test(equal_array_bitset_test),
        cmocka_unit_test(equal_bitset_array_test),
        cmocka_unit_test(equal_array_run_test),
        cmocka_unit_test(equal_run_array_test),
        cmocka_unit_test(equal_bitset_run_test),
        cmocka_unit_test(equal_run_bitset_test),
        cmocka_unit_test(subset_array_array_test),
        cmocka_unit_test(subset_bitset_bitset_test),
        cmocka_unit_test(subset_run_run_test),
        cmocka_unit_test(subset_array_bitset_test),
        cmocka_unit_test(subset_array_run_test),
        cmocka_unit_test(subset_run_array_test),
        cmocka_unit_test(subset_bitset_run_test),
        cmocka_unit_test(subset_run_bitset_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
