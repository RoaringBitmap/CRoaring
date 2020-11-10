/*
 * array_container_unit.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/containers/array.h>
#include <roaring/misc/configreport.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
    using namespace roaring::internal;
#endif

#include "test.h"


DEFINE_TEST(printf_test) {
    array_container_t* B = array_container_create();
    assert_non_null(B);

    array_container_add(B, 1U);
    array_container_add(B, 2U);
    array_container_add(B, 3U);
    array_container_add(B, 10U);
    array_container_add(B, 10000U);

    array_container_printf(B);
    printf("\n");

    array_container_free(B);
}

DEFINE_TEST(add_contains_test) {
    array_container_t* B = array_container_create();
    assert_non_null(B);

    int expected_card = 0;

    for (size_t x = 0; x < 1 << 16; x += 3) {
        assert_true(array_container_add(B, x));
        assert_true(array_container_contains(B, x));
        assert_int_equal(B->cardinality, ++expected_card);
        assert_false(B->cardinality > B->capacity);
    }

    for (size_t x = 0; x < 1 << 16; x++) {
        assert_int_equal(array_container_contains(B, x), (x / 3 * 3 == x));
    }

    assert_int_equal(array_container_cardinality(B), (1 << 16) / 3 + 1);

    for (size_t x = 0; x < 1 << 16; x += 3) {
        assert_true(array_container_contains(B, x));
        assert_true(array_container_remove(B, x));
        assert_int_equal(B->cardinality, --expected_card);
        assert_false(array_container_contains(B, x));
    }

    assert_int_equal(array_container_cardinality(B), 0);

    for (int x = 65535; x >= 0; x -= 3) {
        assert_true(array_container_add(B, x));
        assert_true(array_container_contains(B, x));
        assert_int_equal(B->cardinality, ++expected_card);
        assert_false(B->cardinality > B->capacity);
    }

    assert_int_equal(array_container_cardinality(B), expected_card);

    for (size_t x = 0; x < 1 << 16; x++) {
        assert_int_equal(array_container_contains(B, x), (x / 3 * 3 == x));
    }

    for (size_t x = 0; x < 1 << 16; x += 3) {
        assert_true(array_container_contains(B, x));
        assert_true(array_container_remove(B, x));
        assert_int_equal(B->cardinality, --expected_card);
        assert_false(array_container_contains(B, x));
    }

    array_container_free(B);
}

DEFINE_TEST(and_or_test) {
    DESCRIBE_TEST;

    array_container_t* B1 = array_container_create();
    array_container_t* B2 = array_container_create();
    array_container_t* BI = array_container_create();
    array_container_t* BO = array_container_create();
    array_container_t* TMP = array_container_create();

    assert_non_null(B1);
    assert_non_null(B2);
    assert_non_null(BI);
    assert_non_null(BO);
    assert_non_null(TMP);

    for (size_t x = 0; x < (1 << 16); x += 17) {
        array_container_add(B1, x);
        array_container_add(BI, x);
    }

    // important: 62 is not divisible by 3
    for (size_t x = 0; x < (1 << 16); x += 62) {
        array_container_add(B2, x);
        array_container_add(BI, x);
    }

    for (size_t x = 0; x < (1 << 16); x += 62 * 17) {
        array_container_add(BO, x);
    }

    const int card_inter = array_container_cardinality(BO);
    const int card_union = array_container_cardinality(BI);

    array_container_intersection(B1, B2, TMP);
    assert_int_equal(card_inter, array_container_cardinality(TMP));
    assert_true(array_container_equals(BO, TMP));

    array_container_union(B1, B2, TMP);
    assert_int_equal(card_union, array_container_cardinality(TMP));
    assert_true(array_container_equals(BI, TMP));

    array_container_free(B1);
    array_container_free(B2);
    array_container_free(BI);
    array_container_free(BO);
    array_container_free(TMP);
}

DEFINE_TEST(to_uint32_array_test) {
    for (size_t offset = 1; offset < 128; offset *= 2) {
        array_container_t* B = array_container_create();
        assert_non_null(B);

        for (size_t k = 0; k < (1 << 16); k += offset) {
            assert_true(array_container_add(B, k));
        }

        int card = array_container_cardinality(B);
        uint32_t* out = (uint32_t*)malloc(sizeof(uint32_t) * card);
        assert_non_null(out);
        int nc = array_container_to_uint32_array(out, B, 0);

        assert_int_equal(card, nc);

        for (int k = 1; k < nc; ++k) {
            assert_int_equal(out[k], offset + out[k - 1]);
        }

        free(out);
        array_container_free(B);
    }
}

DEFINE_TEST(select_test) {
    array_container_t* B = array_container_create();
    assert_non_null(B);
    uint16_t base = 27;
    for (uint16_t value = base; value < base + 200; value += 5) {
        array_container_add(B, value);
    }
    uint32_t i = 0;
    uint32_t element = 0;
    uint32_t start_rank;
    for (uint16_t value = base; value < base + 200; value += 5) {
        start_rank = 12;
        assert_true(array_container_select(B, &start_rank, i + 12, &element));
        assert_int_equal(element, value);
        i++;
    }
    start_rank = 12;
    assert_false(array_container_select(B, &start_rank, i + 12, &element));
    assert_int_equal(start_rank, i + 12);
    array_container_free(B);
}

DEFINE_TEST(capacity_test) {
    array_container_t* array = array_container_create();
    for (uint32_t i = 0; i < DEFAULT_MAX_SIZE; i++) {
        array_container_add(array, (uint16_t)i);
        assert_true(array->capacity <= DEFAULT_MAX_SIZE);
    }
    for (uint32_t i = DEFAULT_MAX_SIZE; i < 65536; i++) {
        array_container_add(array, (uint16_t)i);
        assert_true(array->capacity <= 65536);
    }
    array_container_free(array);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(printf_test), cmocka_unit_test(add_contains_test),
        cmocka_unit_test(and_or_test), cmocka_unit_test(to_uint32_array_test),
        cmocka_unit_test(select_test),
        cmocka_unit_test(capacity_test)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
