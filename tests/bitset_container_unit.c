/*
 * bitset_container_unit.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/containers/bitset.h>
#include <roaring/misc/configreport.h>
#include <roaring/bitset_util.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
    using namespace roaring::internal;
#endif

#include "test.h"


DEFINE_TEST(test_bitset_lenrange_cardinality) {
  uint64_t words[] = {~UINT64_C(0), ~UINT64_C(0), ~UINT64_C(0), ~UINT64_C(0), 0, 0, 0, 0};
  for(int k = 0; k < 64 * 4; k++) {
    assert_true(bitset_lenrange_cardinality(words, 0, k) == k + 1); // ok
  }
  for(int k = 64 * 4; k < 64 * 8; k++) {
      assert_true(bitset_lenrange_cardinality(words, 0, k) == 4 * 64); // ok
  }
}

DEFINE_TEST(test_bitset_compute_cardinality) {
    // check that overflow doesn't happen
    bitset_container_t *b = bitset_container_create();
    bitset_container_add_from_range(b, 0, 0x10000, 1);
    assert_true(bitset_container_compute_cardinality(b) == 0x10000);
    bitset_container_free(b);
}

DEFINE_TEST(printf_test) {
    bitset_container_t* B = bitset_container_create();
    assert_non_null(B);

    bitset_container_set(B, 1U);
    bitset_container_set(B, 2U);
    bitset_container_set(B, 3U);
    bitset_container_set(B, 10U);
    bitset_container_set(B, 10000U);

    bitset_container_printf(B);  // does it crash?
    printf("\n");

    bitset_container_free(B);
}

DEFINE_TEST(set_get_test) {
    bitset_container_t* B = bitset_container_create();
    assert_non_null(B);

    for (size_t x = 0; x < 1 << 16; x++) {
        assert_false(bitset_container_get(B, x));
    }

    for (size_t x = 0; x < 1 << 16; x += 3) {
        assert_int_equal(bitset_container_cardinality(B), x / 3);

        assert_false(bitset_container_get(B, x));
        bitset_container_set(B, x);
        assert_true(bitset_container_get(B, x));

        assert_int_equal(bitset_container_cardinality(B), x / 3 + 1);
    }

    for (size_t x = 0; x < 1 << 16; x++) {
        assert_int_equal(bitset_container_get(B, x), (x / 3 * 3 == x));
    }

    assert_int_equal(bitset_container_cardinality(B), (1 << 16) / 3 + 1);
    assert_int_equal(bitset_container_compute_cardinality(B),
                     (1 << 16) / 3 + 1);

    for (size_t x = 0; x < 1 << 16; x += 3) {
        bitset_container_unset(B, x);
    }

    assert_int_equal(bitset_container_cardinality(B), 0);
    assert_int_equal(bitset_container_compute_cardinality(B), 0);

    bitset_container_free(B);
}

DEFINE_TEST(and_or_test) {
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BI = bitset_container_create();
    bitset_container_t* BO = bitset_container_create();

    assert_non_null(B1);
    assert_non_null(B2);
    assert_non_null(BI);
    assert_non_null(BO);

    for (size_t x = 0; x < (1 << 16); x += 3) {
        bitset_container_set(B1, x);
        bitset_container_set(BI, x);
    }

    // important: 62 is not divisible by 3
    for (size_t x = 0; x < (1 << 16); x += 62) {
        bitset_container_set(B2, x);
        bitset_container_set(BI, x);
    }

    for (size_t x = 0; x < (1 << 16); x += 62 * 3) {
        bitset_container_set(BO, x);
    }

    const int card_union = bitset_container_compute_cardinality(BI);
    const int card_inter = bitset_container_compute_cardinality(BO);

    bitset_container_and_nocard(B1, B2, BI);
    assert_int_not_equal(bitset_container_compute_cardinality(BI), card_union);
    assert_int_not_equal(bitset_container_and(B1, B2, BI), card_union);

    bitset_container_or_nocard(B1, B2, BO);
    assert_int_not_equal(bitset_container_compute_cardinality(BO), card_inter);
    assert_int_not_equal(bitset_container_or(B1, B2, BO), card_inter);

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(BI);
    bitset_container_free(BO);
}

DEFINE_TEST(xor_test) {
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BI = bitset_container_create();
    bitset_container_t* TMP = bitset_container_create();

    assert_non_null(B1);
    assert_non_null(B2);
    assert_non_null(BI);
    assert_non_null(TMP);

    for (size_t x = 0; x < (1 << 16); x += 3) {
        bitset_container_set(B1, x);
        bitset_container_set(BI, x);
    }

    // important: 62 is not divisible by 3
    for (size_t x = 0; x < (1 << 16); x += 62) {
        bitset_container_set(B2, x);
        bitset_container_set(BI, x);
    }

    for (size_t x = 0; x < (1 << 16); x += 62 * 3) {
        bitset_container_unset(BI, x);
    }

    bitset_container_xor(B1, B2, TMP);
    assert_true(bitset_container_equals(TMP, BI));

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(BI);
    bitset_container_free(TMP);
}

DEFINE_TEST(andnot_test) {
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BI = bitset_container_create();
    bitset_container_t* TMP = bitset_container_create();

    assert_non_null(B1);
    assert_non_null(B2);
    assert_non_null(BI);
    assert_non_null(TMP);

    for (size_t x = 0; x < (1 << 16); x += 3) {
        bitset_container_set(B1, x);
        bitset_container_set(BI, x);
    }

    // important: 62 is not divisible by 3
    for (size_t x = 0; x < (1 << 16); x += 62) {
        bitset_container_set(B2, x);
        bitset_container_unset(BI, x);
    }

    const int expected = bitset_container_compute_cardinality(BI);

    bitset_container_andnot_nocard(B1, B2, TMP);

    assert_int_equal(expected, bitset_container_compute_cardinality(TMP));
    assert_true(bitset_container_equals(BI, TMP));

    assert_int_equal(expected, bitset_container_andnot(B1, B2, TMP));
    assert_true(bitset_container_equals(BI, TMP));

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(BI);
    bitset_container_free(TMP);
}

DEFINE_TEST(to_uint32_array_test) {
    for (size_t offset = 1; offset < 128; offset *= 2) {
        bitset_container_t* B = bitset_container_create();
        assert_non_null(B);

        for (size_t k = 0; k < (1 << 16); k += offset) {
            bitset_container_set(B, k);
        }

        int card = bitset_container_cardinality(B);

        uint32_t* out = (uint32_t*)malloc(sizeof(uint32_t) * card);
        assert_non_null(out);

        int nc = bitset_container_to_uint32_array(out, B, 0);

        assert_int_equal(card, nc);

        for (int k = 1; k < nc; ++k) {
            assert_int_equal(out[k], offset + out[k - 1]);
        }

        free(out);
        bitset_container_free(B);
    }
}

DEFINE_TEST(select_test) {
    bitset_container_t* B = bitset_container_create();
    assert_non_null(B);
    uint16_t base = 27;
    for (uint16_t value = base; value < base + 200; value += 5) {
        bitset_container_add(B, value);
    }
    uint32_t i = 0;
    uint32_t element = 0;
    uint32_t start_rank;
    for (uint16_t value = base; value < base + 200; value += 5) {
        start_rank = 12;
        assert_true(bitset_container_select(B, &start_rank, i + 12, &element));
        assert_int_equal(element, value);
        i++;
    }
    start_rank = 12;
    assert_false(bitset_container_select(B, &start_rank, i + 12, &element));
    assert_int_equal(start_rank, i + 12);
    bitset_container_free(B);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bitset_lenrange_cardinality),
        cmocka_unit_test(printf_test), cmocka_unit_test(set_get_test),
        cmocka_unit_test(and_or_test), cmocka_unit_test(xor_test),
        cmocka_unit_test(andnot_test), cmocka_unit_test(to_uint32_array_test),
        cmocka_unit_test(select_test),
        cmocka_unit_test(test_bitset_compute_cardinality),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
