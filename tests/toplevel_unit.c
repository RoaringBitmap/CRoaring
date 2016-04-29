#include <assert.h>
#include <stdio.h>
#include <time.h>

#include "roaring.h"

#include "test.h"

void show_structure(roaring_array_t *);  // debug

// arrays expected to both be sorted.
static int array_equals(uint32_t *a1, int32_t size1, uint32_t *a2,
                        int32_t size2) {
    if (size1 != size2) return 0;
    for (int i = 0; i < size1; ++i)
        if (a1[i] != a2[i]) {
            return 0;
        }
    return 1;
}

void roaring_iterator_sumall(uint32_t value, void *param) {
    *(uint32_t *)param += value;
}

void test_example() {
    // create a new empty bitmap
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    // then we can add values
    for (uint32_t i = 100; i < 1000; i++) {
        roaring_bitmap_add(r1, i);
    }

    // check whether a value is contained
    assert_true(roaring_bitmap_contains(r1, 500));

    // compute how many bits there are:
    uint32_t cardinality = roaring_bitmap_get_cardinality(r1);
    printf("Cardinality = %d \n", cardinality);

    // if your bitmaps have long runs, you can compress them by calling
    // run_optimize
    uint32_t size = roaring_bitmap_portable_size_in_bytes(r1);
    roaring_bitmap_run_optimize(r1);
    uint32_t compact_size = roaring_bitmap_portable_size_in_bytes(r1);

    printf("size before run optimize %d bytes, and after %d bytes\n", size,
           compact_size);

    // create a new bitmap with varargs
    roaring_bitmap_t *r2 = roaring_bitmap_of(5, 1, 2, 3, 5, 6);
    assert_non_null(r2);
    roaring_bitmap_printf(r2);

    // we can also create a bitmap from a pointer to 32-bit integers
    const uint32_t values[] = {2, 3, 4};
    roaring_bitmap_t *r3 = roaring_bitmap_of_ptr(3, values);

    // we can also go in reverse and go from arrays to bitmaps
    uint32_t card1;
    uint32_t *arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    assert_non_null(arr1);

    roaring_bitmap_t *r1f = roaring_bitmap_of_ptr(card1, arr1);
    free(arr1);
    assert_non_null(r1f);

    // bitmaps shall be equal
    assert_true(roaring_bitmap_equals(r1, r1f));
    roaring_bitmap_free(r1f);

    // we can copy and compare bitmaps
    roaring_bitmap_t *z = roaring_bitmap_copy(r3);
    assert_true(roaring_bitmap_equals(r3, z));
    roaring_bitmap_free(z);

    // we can compute union two-by-two
    roaring_bitmap_t *r1_2_3 = roaring_bitmap_or(r1, r2);
    roaring_bitmap_or_inplace(r1_2_3, r3);

    // we can compute a big union
    const roaring_bitmap_t *allmybitmaps[] = {r1, r2, r3};
    roaring_bitmap_t *bigunion = roaring_bitmap_or_many(3, allmybitmaps);
    assert_true(roaring_bitmap_equals(r1_2_3, bigunion));
    roaring_bitmap_t *bigunionheap = roaring_bitmap_or_many_heap(3, allmybitmaps);
    assert_true(roaring_bitmap_equals(r1_2_3, bigunionheap));
    roaring_bitmap_free(r1_2_3);
    roaring_bitmap_free(bigunion);
    roaring_bitmap_free(bigunionheap);



    // we can compute intersection two-by-two
    roaring_bitmap_t *i1_2 = roaring_bitmap_and(r1, r2);
    roaring_bitmap_free(i1_2);

    // we can write a bitmap to a pointer and recover it later
    uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    char *serializedbytes = malloc(expectedsize);
    roaring_bitmap_portable_serialize(r1, serializedbytes);
    roaring_bitmap_t *t = roaring_bitmap_portable_deserialize(serializedbytes);
    assert_true(roaring_bitmap_equals(r1, t));
    roaring_bitmap_free(t);
    free(serializedbytes);

    // we can iterate over all values using custom functions
    uint32_t counter = 0;
    roaring_iterate(r1, roaring_iterator_sumall, &counter);
    /**
     * void roaring_iterator_sumall(uint32_t value, void *param) {
     *        *(uint32_t *) param += value;
     *  }
     *
     */

    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r3);
}

void test_printf() {
    roaring_bitmap_t *r1 =
        roaring_bitmap_of(8, 1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    assert_non_null(r1);
    roaring_bitmap_printf(r1);
    roaring_bitmap_free(r1);
    printf("\n");
}

void test_printf_withbitmap() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    roaring_bitmap_printf(r1);
    /* Add some values to the bitmap */
    for (int i = 0, top_val = 4097; i < top_val; i++)
        roaring_bitmap_add(r1, 2 * i);
    roaring_bitmap_printf(r1);
    roaring_bitmap_free(r1);
    printf("\n");
}

void test_printf_withrun() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    roaring_bitmap_printf(r1);
    /* Add some values to the bitmap */
    for (int i = 100, top_val = 200; i < top_val; i++)
        roaring_bitmap_add(r1, i);
    roaring_bitmap_run_optimize(r1);
    roaring_bitmap_printf(r1);  // does it crash?
    roaring_bitmap_free(r1);
    printf("\n");
}

void dummy_iterator(uint32_t value, void *param) {
    (void)value;

    uint32_t *num = (uint32_t *)param;
    (*num)++;
}

void test_iterate() {
    roaring_bitmap_t *r1 =
        roaring_bitmap_of(8, 1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    assert_non_null(r1);

    uint32_t num = 0;
    /* Add some values to the bitmap */
    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);

    roaring_iterate(r1, dummy_iterator, (void *)&num);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), num);
    roaring_bitmap_free(r1);
}

void test_iterate_empty() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    uint32_t num = 0;

    roaring_iterate(r1, dummy_iterator, (void *)&num);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), 0);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), num);
    roaring_bitmap_free(r1);
}

void test_iterate_withbitmap() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    /* Add some values to the bitmap */
    for (int i = 0, top_val = 4097; i < top_val; i++)
        roaring_bitmap_add(r1, 2 * i);
    uint32_t num = 0;

    roaring_iterate(r1, dummy_iterator, (void *)&num);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), num);
    roaring_bitmap_free(r1);
}

void test_iterate_withrun() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    /* Add some values to the bitmap */
    for (int i = 100, top_val = 200; i < top_val; i++)
        roaring_bitmap_add(r1, i);
    roaring_bitmap_run_optimize(r1);
    uint32_t num = 0;
    roaring_iterate(r1, dummy_iterator, (void *)&num);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), num);
    roaring_bitmap_free(r1);
}

void test_portable_serialize() {
    roaring_bitmap_t *r1 =
        roaring_bitmap_of(8, 1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    assert_non_null(r1);

    uint32_t serialize_len;
    roaring_bitmap_t *r2;

    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);

    uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    char *serialized = malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert_int_equal(serialize_len, expectedsize);
    r2 = roaring_bitmap_portable_deserialize(serialized);
    assert_non_null(r2);

    uint32_t card1, card2;
    uint32_t *arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    uint32_t *arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    r1 = roaring_bitmap_of(6, 2946000, 2997491, 10478289, 10490227, 10502444,
                           19866827);
    expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    serialized = malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert_int_equal(serialize_len, expectedsize);

    r2 = roaring_bitmap_portable_deserialize(serialized);
    assert_non_null(r2);

    arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t k = 100; k < 100000; ++k) {
        roaring_bitmap_add(r1, k);
    }

    roaring_bitmap_run_optimize(r1);
    expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    serialized = malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert_int_equal(serialize_len, expectedsize);

    r2 = roaring_bitmap_portable_deserialize(serialized);
    assert_non_null(r2);

    arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert(array_equals(arr1, card1, arr2, card2));
    assert(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
}

void test_serialize() {
    roaring_bitmap_t *r1 =
        roaring_bitmap_of(8, 1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    assert_non_null(r1);

    uint32_t serialize_len;
    char *serialized;
    roaring_bitmap_t *r2;

    /* Add some values to the bitmap */
    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);

    serialized = roaring_bitmap_serialize(r1, &serialize_len);
    r2 = roaring_bitmap_deserialize(serialized, serialize_len);
    assert_non_null(r2);

    uint32_t card1, card2;
    uint32_t *arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    uint32_t *arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    run_container_t *run = run_container_create_given_capacity(1024);
    assert_non_null(run);
    for (int i = 0; i < 768; i++) run_container_add(run, 3 * i);

    serialize_len = run_container_serialization_len(run);
    char rbuf[serialize_len];
    assert_int_equal((int32_t)serialize_len,
                     run_container_serialize(run, rbuf));
    run_container_t *run1 = run_container_deserialize(rbuf, serialize_len);

    run_container_free(run);
    run_container_free(run1);

    r1 = roaring_bitmap_of(6, 2946000, 2997491, 10478289, 10490227, 10502444,
                           19866827);
    serialized = roaring_bitmap_serialize(r1, &serialize_len);
    r2 = roaring_bitmap_deserialize(serialized, serialize_len);
    assert_non_null(r2);
    arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    assert_non_null(arr1);
    arr2 = roaring_bitmap_to_uint32_array(r2, &card2);
    assert_non_null(arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    r1 = roaring_bitmap_create();
    for (uint32_t k = 100; k < 100000; ++k) {
        roaring_bitmap_add(r1, k);
    }
    roaring_bitmap_run_optimize(r1);
    serialized = roaring_bitmap_serialize(r1, &serialize_len);
    r2 = roaring_bitmap_deserialize(serialized, serialize_len);

    arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
}

void test_add() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 10000; ++i) {
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i);
        roaring_bitmap_add(r1, 200 * i);
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i + 1);
    }

    roaring_bitmap_free(r1);
}

void test_contains() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 10000; ++i) {
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i);
        roaring_bitmap_add(r1, 200 * i);
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i + 1);
    }

    for (uint32_t i = 0; i < 200 * 10000; ++i) {
        assert_int_equal(roaring_bitmap_contains(r1, i), (i % 200 == 0));
    }

    roaring_bitmap_free(r1);
}

void test_intersection_array_x_array() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert_non_null(r2);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);

        assert_int_equal(roaring_bitmap_get_cardinality(r2), 2 * (i + 1));
        assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * (i + 1));
    }

    roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
    assert_non_null(r1_and_r2);
    assert_int_equal(roaring_bitmap_get_cardinality(r1_and_r2), 2 * 34);

    roaring_bitmap_free(r1_and_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

void test_intersection_array_x_array_inplace() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert(r2);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);

        assert_int_equal(roaring_bitmap_get_cardinality(r2), 2 * (i + 1));
        assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * (i + 1));
    }

    roaring_bitmap_and_inplace(r1, r2);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * 34);

    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

void test_intersection_bitset_x_bitset() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert(r2);

    for (uint32_t i = 0; i < 20000; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r2, 3 * i + 1);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i + 1);

        assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * (i + 1));
        assert_int_equal(roaring_bitmap_get_cardinality(r2), 4 * (i + 1));
    }

    roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
    assert_non_null(r1_and_r2);

    // NOT analytically determined but seems reasonable
    assert_int_equal(roaring_bitmap_get_cardinality(r1_and_r2), 26666);

    roaring_bitmap_free(r1_and_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

void test_intersection_bitset_x_bitset_inplace() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert(r2);

    for (uint32_t i = 0; i < 20000; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r2, 3 * i + 1);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i + 1);

        assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * (i + 1));
        assert_int_equal(roaring_bitmap_get_cardinality(r2), 4 * (i + 1));
    }

    roaring_bitmap_and_inplace(r1, r2);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), 26666);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

void test_union() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert(r2);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        assert_int_equal(roaring_bitmap_get_cardinality(r2), i + 1);
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i + 1);
    }

    roaring_bitmap_t *r1_or_r2 = roaring_bitmap_or(r1, r2);
    assert_int_equal(roaring_bitmap_get_cardinality(r1_or_r2), 166);

    roaring_bitmap_free(r1_or_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

static roaring_bitmap_t *make_roaring_from_array(uint32_t *a, int len) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    for (int i = 0; i < len; ++i) roaring_bitmap_add(r1, a[i]);
    return r1;
}

void test_conversion_to_int_array() {
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // a dense bitmap container  (best done with runs)
    for (uint32_t i = 0; i < 50000; ++i) {
        if (i != 30000) {  // making 2 runs
            ans[ans_ctr++] = i;
        }
    }

    // a sparse one
    for (uint32_t i = 70000; i < 130000; i += 17) {
        ans[ans_ctr++] = i;
    }

    // a dense one but not good for runs

    for (uint32_t i = 65536 * 3; i < 65536 * 4; i++) {
        if (i % 3 != 0) {
            ans[ans_ctr++] = i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    assert_true(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

void test_conversion_to_int_array_with_runoptimize() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // a dense bitmap container  (best done with runs)
    for (uint32_t i = 0; i < 50000; ++i) {
        if (i != 30000) {  // making 2 runs
            ans[ans_ctr++] = i;
        }
    }

    // a sparse one
    for (uint32_t i = 70000; i < 130000; i += 17) {
        ans[ans_ctr++] = i;
    }

    // a dense one but not good for runs

    for (uint32_t i = 65536 * 3; i < 65536 * 4; i++) {
        if (i % 3 != 0) {
            ans[ans_ctr++] = i;
        }
    }
    roaring_bitmap_free(r1);

    r1 = make_roaring_from_array(ans, ans_ctr);
    assert_true(roaring_bitmap_run_optimize(r1));

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    assert_true(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

void test_array_to_run() {
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // array container  (best done with runs)
    for (uint32_t i = 0; i < 500; ++i) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_true(roaring_bitmap_run_optimize(r1));

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    assert_true(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

void test_array_to_self() {
    int ans_ctr = 0;

    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // array container  (best not done with runs)
    for (uint32_t i = 0; i < 500; i += 2) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_false(roaring_bitmap_run_optimize(r1));

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    assert_true(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

void test_bitset_to_self() {
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // bitset container  (best not done with runs)
    for (uint32_t i = 0; i < 50000; i += 2) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_false(roaring_bitmap_run_optimize(r1));

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    assert_true(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

void test_bitset_to_run() {
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // bitset container  (best done with runs)
    for (uint32_t i = 0; i < 50000; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert(roaring_bitmap_run_optimize(r1));

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    assert_true(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

// not sure how to get containers that are runcontainers but not efficient

void test_run_to_self() {
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // bitset container  (best done with runs)
    for (uint32_t i = 0; i < 50000; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    bool b = roaring_bitmap_run_optimize(r1);  // will make a run container
    b = roaring_bitmap_run_optimize(r1);       // we hope it will keep it
    assert_true(b);  // still true there is a runcontainer

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    assert_true(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

void test_remove_run_to_bitset() {
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // bitset container  (best done with runs)
    for (uint32_t i = 0; i < 50000; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_true(roaring_bitmap_run_optimize(r1));  // will make a run container
    assert_true(roaring_bitmap_remove_run_compression(r1));  // removal done
    assert_true(
        roaring_bitmap_run_optimize(r1));  // there is again a run container

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    assert_true(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

void test_remove_run_to_array() {
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // array  (best done with runs)
    for (uint32_t i = 0; i < 500; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_true(roaring_bitmap_run_optimize(r1));  // will make a run container
    assert_true(roaring_bitmap_remove_run_compression(r1));  // removal done
    assert_true(
        roaring_bitmap_run_optimize(r1));  // there is again a run container

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    assert_true(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_example),
        cmocka_unit_test(test_printf),
        cmocka_unit_test(test_printf_withbitmap),
        cmocka_unit_test(test_printf_withrun),
        cmocka_unit_test(test_iterate),
        cmocka_unit_test(test_iterate_empty),
        cmocka_unit_test(test_iterate_withbitmap),
        cmocka_unit_test(test_iterate_withrun),
        cmocka_unit_test(test_serialize),
        cmocka_unit_test(test_portable_serialize),
        cmocka_unit_test(test_add),
        cmocka_unit_test(test_contains),
        cmocka_unit_test(test_intersection_array_x_array),
        cmocka_unit_test(test_intersection_array_x_array_inplace),
        cmocka_unit_test(test_intersection_bitset_x_bitset),
        cmocka_unit_test(test_intersection_bitset_x_bitset_inplace),
        cmocka_unit_test(test_union),
        cmocka_unit_test(test_conversion_to_int_array),
        cmocka_unit_test(test_array_to_run),
        cmocka_unit_test(test_array_to_self),
        cmocka_unit_test(test_bitset_to_self),
        cmocka_unit_test(test_conversion_to_int_array_with_runoptimize),
        cmocka_unit_test(test_run_to_self),
        cmocka_unit_test(test_remove_run_to_bitset),
        cmocka_unit_test(test_remove_run_to_array),
        // cmocka_unit_test(test_run_to_bitset),
        // cmocka_unit_test(test_run_to_array),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
