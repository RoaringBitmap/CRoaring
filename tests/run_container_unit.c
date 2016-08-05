/*
 * run_container_unit.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/containers/run.h>

#include "test.h"

void printf_test() {
    run_container_t* B = run_container_create();

    assert_non_null(B);

    run_container_add(B, 1);
    run_container_add(B, 2);
    run_container_add(B, 3);
    run_container_add(B, 10);
    run_container_add(B, 10000);

    run_container_printf(B);
    printf("\n");

    run_container_free(B);
}

void add_contains_test() {
    run_container_t* B = run_container_create();
    assert_non_null(B);

    int expected_card = 0;
    for (size_t x = 0; x < 1 << 16; x += 3) {
        assert_true(run_container_add(B, x));
        assert_true(run_container_contains(B, x));
        assert_int_equal(run_container_cardinality(B), ++expected_card);
        assert_true(run_container_cardinality(B) <= B->capacity);
    }

    for (size_t x = 0; x < 1 << 16; x++) {
        assert_int_equal(run_container_contains(B, x), (x / 3 * 3 == x));
    }

    assert_int_equal(run_container_cardinality(B), (1 << 16) / 3 + 1);

    for (size_t x = 0; x < 1 << 16; x += 3) {
        assert_true(run_container_contains(B, x));
        assert_true(run_container_remove(B, x));
        assert_int_equal(run_container_cardinality(B), --expected_card);
        assert_false(run_container_contains(B, x));
    }

    assert_int_equal(run_container_cardinality(B), 0);

    for (int x = 65535; x >= 0; x -= 3) {
        assert_true(run_container_add(B, x));
        assert_true(run_container_contains(B, x));
        assert_int_equal(run_container_cardinality(B), ++expected_card);
        assert_true(run_container_cardinality(B) <= B->capacity);
    }

    assert_int_equal(run_container_cardinality(B), (1 << 16) / 3 + 1);

    for (size_t x = 0; x < 1 << 16; x++) {
        assert_int_equal(run_container_contains(B, x), (x / 3 * 3 == x));
    }

    for (size_t x = 0; x < 1 << 16; x += 3) {
        assert_true(run_container_contains(B, x));
        assert_true(run_container_remove(B, x));
        assert_int_equal(run_container_cardinality(B), --expected_card);
        assert_false(run_container_contains(B, x));
    }

    run_container_free(B);
}

void and_or_test() {
    run_container_t* B1 = run_container_create();
    run_container_t* B2 = run_container_create();
    run_container_t* BI = run_container_create();
    run_container_t* BO = run_container_create();
    run_container_t* TMP = run_container_create();

    assert_non_null(B1);
    assert_non_null(B2);
    assert_non_null(BI);
    assert_non_null(BO);
    assert_non_null(TMP);

    for (size_t x = 0; x < (1 << 16); x += 3) {
        run_container_add(B1, x);
        run_container_add(BI, x);
    }

    // important: 62 is not divisible by 3
    for (size_t x = 0; x < (1 << 16); x += 62) {
        run_container_add(B2, x);
        run_container_add(BI, x);
    }

    for (size_t x = 0; x < (1 << 16); x += 62 * 3) {
        run_container_add(BO, x);
    }

    run_container_intersection(B1, B2, TMP);
    assert_true(run_container_equals(BO, TMP));

    run_container_union(B1, B2, TMP);
    assert_true(run_container_equals(BI, TMP));

    run_container_free(B1);
    run_container_free(B2);
    run_container_free(BO);
    run_container_free(BI);
    run_container_free(TMP);
}

// returns 0 on error, 1 if ok.
void to_uint32_array_test() {
    for (size_t offset = 1; offset < 128; offset *= 2) {
        run_container_t* B = run_container_create();
        assert_non_null(B);

        for (int k = 0; k < (1 << 16); k += offset) {
            run_container_add(B, k);
        }

        int card = run_container_cardinality(B);
        uint32_t* out = malloc(sizeof(uint32_t) * card);
        int nc = run_container_to_uint32_array(out, B, 0);
        assert_int_equal(nc, card);

        for (int k = 1; k < nc; ++k) {
            assert_int_equal(out[k], offset + out[k - 1]);
        }

        free(out);
        run_container_free(B);
    }
}

void select_test() {
    run_container_t* B = run_container_create();
    assert_non_null(B);
    uint16_t base = 27;
    for (uint16_t value = base; value < base + 200; value += 5) {
        run_container_add(B, value);
    }
    uint32_t i = 0;
    uint32_t element = 0;
    uint32_t start_rank;
    for (uint16_t value = base; value < base + 200; value += 5) {
        start_rank = 12;
        assert_true(run_container_select(B, &start_rank, i + 12, &element));
        assert_int_equal(element, value);
        i++;
    }
    start_rank = 12;
    assert_false(run_container_select(B, &start_rank, i + 12, &element));
    assert_int_equal(start_rank, i + 12);
    run_container_free(B);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(printf_test), cmocka_unit_test(add_contains_test),
        cmocka_unit_test(and_or_test), cmocka_unit_test(to_uint32_array_test),
        cmocka_unit_test(select_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
