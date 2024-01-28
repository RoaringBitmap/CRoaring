/*
 * array_container_unit.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/mixed_equal.h>
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

/* This is a fixed-increment version of Java 8's SplittableRandom generator
   See http://dx.doi.org/10.1145/2714064.2660195 and
   http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html */

// state for splitmix64
uint64_t splitmix64_x; /* The state can be seeded with any value. */

// call this one before calling splitmix64
static inline void splitmix64_seed(uint64_t seed) { splitmix64_x = seed; }

// floor( ( (1+sqrt(5))/2 ) * 2**64 MOD 2**64)
#define GOLDEN_GAMMA UINT64_C(0x9E3779B97F4A7C15)

// returns random number, modifies seed[0]
// compared with D. Lemire against
// http://grepcode.com/file/repository.grepcode.com/java/root/jdk/openjdk/8-b132/java/util/SplittableRandom.java#SplittableRandom.0gamma
static inline uint64_t splitmix64_r(uint64_t* seed) {
    uint64_t z = (*seed += GOLDEN_GAMMA);
    // David Stafford's Mix13 for MurmurHash3's 64-bit finalizer
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

static inline uint64_t splitmix64() { return splitmix64_r(&splitmix64_x); }

size_t populate(uint16_t* buffer, size_t maxsize) {
    size_t length = splitmix64() % maxsize;
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (uint16_t)splitmix64();
    }
    return length;
}

DEFINE_TEST(mini_fuzz_array_container_intersection_inplace) {
    splitmix64_seed(12345);
    uint16_t* buffer1 = (uint16_t*)malloc(DEFAULT_MAX_SIZE * sizeof(uint16_t));
    uint16_t* buffer2 = (uint16_t*)malloc(DEFAULT_MAX_SIZE * sizeof(uint16_t));
    uint16_t* buffer3 = (uint16_t*)malloc(DEFAULT_MAX_SIZE * sizeof(uint16_t));
    for (size_t z = 0; z < 3000; z++) {
        array_container_t* array1 = array_container_create();
        array_container_t* array2 = array_container_create();
        array_container_t* array3 = array_container_create();

        bitset_container_t* bitset1 = bitset_container_create();
        bitset_container_t* bitset2 = bitset_container_create();
        bitset_container_t* bitset3 = bitset_container_create();
        size_t l1 = populate(buffer1, DEFAULT_MAX_SIZE);
        size_t l2 = populate(buffer2, DEFAULT_MAX_SIZE);
        size_t l3 = populate(buffer3, DEFAULT_MAX_SIZE);

        for (uint32_t i = 0; i < l1; i++) {
            array_container_add(array1, buffer1[i]);
            bitset_container_set(bitset1, buffer1[i]);
        }
        for (uint32_t i = 0; i < l2; i++) {
            array_container_add(array2, buffer2[i]);
            bitset_container_set(bitset2, buffer2[i]);
        }
        for (uint32_t i = 0; i < l3; i++) {
            array_container_add(array3, buffer3[i]);
            bitset_container_set(bitset3, buffer3[i]);
        }
        bitset1->cardinality = BITSET_UNKNOWN_CARDINALITY;

        array_container_intersection_inplace(array1, array2);
        bitset_container_and_nocard(bitset1, bitset2, bitset1);
        assert_true(array_container_equal_bitset(array1, bitset1));

        array_container_intersection_inplace(array1, array3);
        bitset_container_and_nocard(bitset1, bitset3, bitset1);
        assert_true(array_container_equal_bitset(array1, bitset1));

        for (uint32_t i = 0; i < l1; i++) {
            array_container_add(array1, buffer1[i]);
            bitset_container_set(bitset1, buffer1[i]);
        }
        bitset1->cardinality = BITSET_UNKNOWN_CARDINALITY;
        assert_true(array_container_equal_bitset(array1, bitset1));

        array_container_intersection_inplace(array1, array2);
        bitset_container_and_nocard(bitset1, bitset2, bitset1);
        assert_true(array_container_equal_bitset(array1, bitset1));

        array_container_intersection_inplace(array1, array3);
        bitset_container_and_nocard(bitset1, bitset3, bitset1);
        assert_true(array_container_equal_bitset(array1, bitset1));
        array_container_free(array1);
        array_container_free(array2);
        array_container_free(array3);
        bitset_container_free(bitset1);
        bitset_container_free(bitset2);
        bitset_container_free(bitset3);
    }
    free(buffer1);
    free(buffer2);
    free(buffer3);
}

DEFINE_TEST(mini_fuzz_recycle_array_container_intersection_inplace) {
    splitmix64_seed(12345);
    uint16_t* buffer1 = (uint16_t*)malloc(DEFAULT_MAX_SIZE * sizeof(uint16_t));
    uint16_t* buffer2 = (uint16_t*)malloc(DEFAULT_MAX_SIZE * sizeof(uint16_t));
    uint16_t* buffer3 = (uint16_t*)malloc(DEFAULT_MAX_SIZE * sizeof(uint16_t));
    array_container_t* array1 = array_container_create();
    array_container_t* array2 = array_container_create();
    array_container_t* array3 = array_container_create();

    bitset_container_t* bitset1 = bitset_container_create();
    bitset_container_t* bitset2 = bitset_container_create();
    bitset_container_t* bitset3 = bitset_container_create();
    for (size_t z = 0; z < 3000; z++) {
        bitset_container_clear(bitset1);
        bitset_container_clear(bitset2);
        bitset_container_clear(bitset3);
        array1->cardinality = 0;
        array2->cardinality = 0;
        array3->cardinality = 0;
        size_t l1 = populate(buffer1, DEFAULT_MAX_SIZE);
        size_t l2 = populate(buffer2, DEFAULT_MAX_SIZE);
        size_t l3 = populate(buffer3, DEFAULT_MAX_SIZE);

        for (uint32_t i = 0; i < l1; i++) {
            array_container_add(array1, buffer1[i]);
            bitset_container_set(bitset1, buffer1[i]);
        }
        for (uint32_t i = 0; i < l2; i++) {
            array_container_add(array2, buffer2[i]);
            bitset_container_set(bitset2, buffer2[i]);
        }
        for (uint32_t i = 0; i < l3; i++) {
            array_container_add(array3, buffer3[i]);
            bitset_container_set(bitset3, buffer3[i]);
        }
        bitset1->cardinality = BITSET_UNKNOWN_CARDINALITY;

        array_container_intersection_inplace(array1, array2);
        bitset_container_and_nocard(bitset1, bitset2, bitset1);
        assert_true(array_container_equal_bitset(array1, bitset1));

        array_container_intersection_inplace(array1, array3);
        bitset_container_and_nocard(bitset1, bitset3, bitset1);
        assert_true(array_container_equal_bitset(array1, bitset1));

        for (uint32_t i = 0; i < l1; i++) {
            array_container_add(array1, buffer1[i]);
            bitset_container_set(bitset1, buffer1[i]);
        }
        bitset1->cardinality = BITSET_UNKNOWN_CARDINALITY;
        assert_true(array_container_equal_bitset(array1, bitset1));

        array_container_intersection_inplace(array1, array2);
        bitset_container_and_nocard(bitset1, bitset2, bitset1);
        assert_true(array_container_equal_bitset(array1, bitset1));

        array_container_intersection_inplace(array1, array3);
        bitset_container_and_nocard(bitset1, bitset3, bitset1);
        assert_true(array_container_equal_bitset(array1, bitset1));
    }
    array_container_free(array1);
    array_container_free(array2);
    array_container_free(array3);
    bitset_container_free(bitset1);
    bitset_container_free(bitset2);
    bitset_container_free(bitset3);

    free(buffer1);
    free(buffer2);
    free(buffer3);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(mini_fuzz_array_container_intersection_inplace),
        cmocka_unit_test(
            mini_fuzz_recycle_array_container_intersection_inplace),
        cmocka_unit_test(printf_test),
        cmocka_unit_test(add_contains_test),
        cmocka_unit_test(and_or_test),
        cmocka_unit_test(to_uint32_array_test),
        cmocka_unit_test(select_test),
        cmocka_unit_test(capacity_test)};

    return cmocka_run_group_tests(tests, NULL, NULL);
}
