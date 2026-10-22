/*
 * run_container_unit.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/containers/run.h>
#include <roaring/misc/configreport.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
using namespace roaring::internal;
#endif

#include "test.h"

DEFINE_TEST(printf_test) {
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

DEFINE_TEST(add_contains_test) {
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

DEFINE_TEST(and_or_test) {
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

static void add_run_values(run_container_t* run, uint32_t start, uint32_t end) {
    for (uint32_t value = start; value < end; ++value) {
        assert_true(run_container_add(run, (uint16_t)value));
    }
}

DEFINE_TEST(run_intersect_skip_test) {
    run_container_t* runs = run_container_create();
    run_container_t* middle_hit = run_container_create();
    run_container_t* last_hit = run_container_create();
    run_container_t* gap = run_container_create();
    run_container_t* overshoot_left = run_container_create();
    run_container_t* overshoot_right = run_container_create();
    run_container_t* low = run_container_create();
    run_container_t* low_adjacent = run_container_create();
    run_container_t* low_overlap = run_container_create();
    run_container_t* high_before = run_container_create();
    run_container_t* high_adjacent = run_container_create();
    run_container_t* high_overlap = run_container_create();
    run_container_t* high_prefix = run_container_create();
    run_container_t* high_last = run_container_create();

    assert_non_null(runs);
    assert_non_null(middle_hit);
    assert_non_null(last_hit);
    assert_non_null(gap);
    assert_non_null(overshoot_left);
    assert_non_null(overshoot_right);
    assert_non_null(low);
    assert_non_null(low_adjacent);
    assert_non_null(low_overlap);
    assert_non_null(high_before);
    assert_non_null(high_adjacent);
    assert_non_null(high_overlap);
    assert_non_null(high_prefix);
    assert_non_null(high_last);

    for (uint32_t run = 0; run < 32; ++run) {
        uint32_t start = run * 6;
        add_run_values(runs, start, start + 3);
    }
    add_run_values(middle_hit, 16 * 6 + 2, 16 * 6 + 5);
    add_run_values(last_hit, 31 * 6 + 2, 31 * 6 + 5);
    add_run_values(gap, 31 * 6 + 3, 31 * 6 + 6);

    assert_true(run_container_intersect(runs, middle_hit));
    assert_true(run_container_intersect(middle_hit, runs));
    assert_true(run_container_intersect(runs, last_hit));
    assert_true(run_container_intersect(last_hit, runs));
    assert_false(run_container_intersect(runs, gap));
    assert_false(run_container_intersect(gap, runs));

    add_run_values(overshoot_left, 0, 3);
    add_run_values(overshoot_left, 12, 15);
    add_run_values(overshoot_right, 3, 6);
    add_run_values(overshoot_right, 15, 18);
    assert_false(run_container_intersect(overshoot_left, overshoot_right));
    assert_false(run_container_intersect(overshoot_right, overshoot_left));

    add_run_values(low, 0, 3);
    add_run_values(low_adjacent, 3, 6);
    add_run_values(low_overlap, 2, 5);
    assert_false(run_container_intersect(low, low_adjacent));
    assert_true(run_container_intersect(low, low_overlap));

    add_run_values(high_before, 65530, 65533);
    add_run_values(high_adjacent, 65533, 65536);
    assert_true(run_container_add(high_overlap, UINT16_MAX));
    assert_false(run_container_intersect(high_before, high_adjacent));
    assert_false(run_container_intersect(high_adjacent, high_before));
    assert_true(run_container_intersect(high_adjacent, high_overlap));
    assert_true(run_container_intersect(high_overlap, high_adjacent));

    for (uint32_t run = 0; run < 32; ++run) {
        uint32_t start = run * 2114;
        add_run_values(high_prefix, start, start + 2);
    }
    assert_true(run_container_add(high_last, UINT16_MAX));
    assert_true(run_container_intersect(high_prefix, high_last));
    assert_true(run_container_intersect(high_last, high_prefix));

    run_container_free(runs);
    run_container_free(middle_hit);
    run_container_free(last_hit);
    run_container_free(gap);
    run_container_free(overshoot_left);
    run_container_free(overshoot_right);
    run_container_free(low);
    run_container_free(low_adjacent);
    run_container_free(low_overlap);
    run_container_free(high_before);
    run_container_free(high_adjacent);
    run_container_free(high_overlap);
    run_container_free(high_prefix);
    run_container_free(high_last);
}

// returns 0 on error, 1 if ok.
DEFINE_TEST(to_uint32_array_test) {
    for (size_t offset = 1; offset < 128; offset *= 2) {
        run_container_t* B = run_container_create();
        assert_non_null(B);

        for (int k = 0; k < (1 << 16); k += offset) {
            run_container_add(B, k);
        }

        int card = run_container_cardinality(B);
        uint32_t* out = (uint32_t*)malloc(sizeof(uint32_t) * card);
        int nc = run_container_to_uint32_array(out, B, 0);
        assert_int_equal(nc, card);

        for (int k = 1; k < nc; ++k) {
            assert_int_equal(out[k], offset + out[k - 1]);
        }

        free(out);
        run_container_free(B);
    }
}

DEFINE_TEST(select_test) {
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

static inline void _run_container_add_range(run_container_t* run, uint32_t min,
                                            uint32_t max) {
    int32_t nruns_greater = rle16_count_greater(run->runs, run->n_runs, max);
    int32_t nruns_less =
        rle16_count_less(run->runs, run->n_runs - nruns_greater, min);
    run_container_add_range_nruns(run, min, max, nruns_less, nruns_greater);
}

DEFINE_TEST(remove_range_test) {
    run_container_t* run = run_container_create();
    _run_container_add_range(run, 100, 150);
    _run_container_add_range(run, 200, 250);
    _run_container_add_range(run, 300, 350);

    // act on left-most run
    run_container_remove_range(run, 100, 110);
    run_container_remove_range(run, 140, 150);
    run_container_remove_range(run, 120, 130);

    // act on right-most run
    run_container_remove_range(run, 300, 310);
    run_container_remove_range(run, 340, 350);
    run_container_remove_range(run, 320, 330);

    // act on inner run
    run_container_remove_range(run, 200, 210);
    run_container_remove_range(run, 240, 250);
    run_container_remove_range(run, 220, 230);

    // [111..119], [131..139], [211..219], [231..239], [311..319], [331..339]

    // remove entire runs
    run_container_remove_range(run, 111, 119);
    run_container_remove_range(run, 331, 339);
    run_container_remove_range(run, 231, 239);

    // [131..139], [211..219], [311..319]

    assert_true(run_container_contains_range(run, 131, 139 + 1));
    assert_true(run_container_contains_range(run, 211, 219 + 1));
    assert_true(run_container_contains_range(run, 311, 319 + 1));
    assert_true(run_container_cardinality(run) == 27);

    run_container_free(run);
}

int main() {
    tellmeall();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(printf_test),
        cmocka_unit_test(add_contains_test),
        cmocka_unit_test(and_or_test),
        cmocka_unit_test(run_intersect_skip_test),
        cmocka_unit_test(to_uint32_array_test),
        cmocka_unit_test(select_test),
        cmocka_unit_test(remove_range_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
