#include <assert.h>
#include <stdio.h>
#include <time.h>

#include "roaring.h"

void show_structure(roaring_array_t *);  // debug

// arrays expected to both be sorted.
static int array_equals(uint32_t *a1, int32_t size1, uint32_t *a2,
                        int32_t size2) {
    if (size1 != size2) return 0;
    for (int i = 0; i < size1; ++i)
        if (a1[i] != a2[i]) {
            printf("array_equals a1[%d] is %d != a2[%d] is %d\n", i, a1[i], i,
                   a2[i]);
            return 0;
        }
    return 1;
}

int test_printf() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 =
        roaring_bitmap_of(8, 1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    roaring_bitmap_printf(r1);  // does it crash?
    roaring_bitmap_free(r1);
    printf("\n");
    return 1;
}

#ifdef __GNUC__
#define VARIABLE_IS_NOT_USED __attribute__((unused))
#else
#define VARIABLE_IS_NOT_USED
#endif

void dummy_iterator(uint32_t VARIABLE_IS_NOT_USED value, void *param) {
    uint32_t *num = (uint32_t *)param;

    (*num)++;
}

int test_iterate() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 =
        roaring_bitmap_of(8, 1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    uint32_t num = 0;

    /* Add some values to the bitmap */
    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);

    roaring_iterate(r1, dummy_iterator, (void *)&num);

    assert(roaring_bitmap_get_cardinality(r1) == num);
    roaring_bitmap_free(r1);
    return (1);
}

// serialization as in Java and Go
int test_portable_serialize() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 =
        roaring_bitmap_of(8, 1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    uint32_t serialize_len;
    roaring_bitmap_t *r2;

    /* Add some values to the bitmap */
    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);
    uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    char *serialized = malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert(serialize_len == expectedsize);
    r2 = roaring_bitmap_portable_deserialize(serialized);
    assert(r2);
    printf("Serialization len: %u [%.1f bit/element]\n", serialize_len,
           ((float)(8 * serialize_len)) /
               ((float)roaring_bitmap_get_cardinality(r2)));

    uint32_t card1, card2;
    uint32_t *arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    uint32_t *arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert(array_equals(arr1, card1, arr2, card2));
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
    assert(serialize_len == expectedsize);

    printf("Serialization len: %u [%.1f bit/element]\n", serialize_len,
           ((float)(8 * serialize_len)) /
               ((float)roaring_bitmap_get_cardinality(r1)));
    r2 = roaring_bitmap_portable_deserialize(serialized);
    assert(r2);

    arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert(array_equals(arr1, card1, arr2, card2));
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
    expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    serialized = malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert(serialize_len == expectedsize);

    printf("Serialization len: %u [%.4f bit/element]\n", serialize_len,
           ((float)(8 * serialize_len)) /
               ((float)roaring_bitmap_get_cardinality(r1)));
    r2 = roaring_bitmap_portable_deserialize(serialized);
    assert(r2);

    arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert(array_equals(arr1, card1, arr2, card2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    return 1;
}

int test_serialize() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 =
        roaring_bitmap_of(8, 1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    uint32_t serialize_len;
    char *serialized;
    roaring_bitmap_t *r2;

    /* Add some values to the bitmap */
    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);

    serialized = roaring_bitmap_serialize(r1, &serialize_len);
    r2 = roaring_bitmap_deserialize(serialized, serialize_len);
    assert(r2);
    printf("Serialization len: %u [%.1f bit/element]\n", serialize_len,
           ((float)(8 * serialize_len)) /
               ((float)roaring_bitmap_get_cardinality(r2)));

    uint32_t card1, card2;
    uint32_t *arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    uint32_t *arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert(array_equals(arr1, card1, arr2, card2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    run_container_t *run = run_container_create_given_capacity(1024);
    for (int i = 0; i < 768; i++) run_container_add(run, 3 * i);

    serialize_len = run_container_serialization_len(run);
    char rbuf[serialize_len];
    assert((int32_t)serialize_len == run_container_serialize(run, rbuf));
    run_container_t *run1 = run_container_deserialize(rbuf, serialize_len);

    run_container_free(run);
    run_container_free(run1);

    r1 = roaring_bitmap_of(6, 2946000, 2997491, 10478289, 10490227, 10502444,
                           19866827);
    serialized = roaring_bitmap_serialize(r1, &serialize_len);
    printf("Serialization len: %u [%.4f bit/element]\n", serialize_len,
           ((float)(8 * serialize_len)) /
               ((float)roaring_bitmap_get_cardinality(r1)));
    r2 = roaring_bitmap_deserialize(serialized, serialize_len);

    arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert(array_equals(arr1, card1, arr2, card2));
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
    printf("Serialization len: %u [%.4f bit/element]\n", serialize_len,
           ((float)(8 * serialize_len)) /
               ((float)roaring_bitmap_get_cardinality(r1)));
    r2 = roaring_bitmap_deserialize(serialized, serialize_len);

    arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
    arr2 = roaring_bitmap_to_uint32_array(r2, &card2);

    assert(array_equals(arr1, card1, arr2, card2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    return 1;
}

int test_add() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert(r1);

    for (uint32_t i = 0; i < 10000; ++i) {
        assert(roaring_bitmap_get_cardinality(r1) == i);
        roaring_bitmap_add(r1, 200 * i);
        assert(roaring_bitmap_get_cardinality(r1) == i + 1);
    }
    roaring_bitmap_free(r1);
    return 1;
}

int test_contains() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert(r1);

    for (uint32_t i = 0; i < 10000; ++i) {
        assert(roaring_bitmap_get_cardinality(r1) == i);
        roaring_bitmap_add(r1, 200 * i);
        assert(roaring_bitmap_get_cardinality(r1) == i + 1);
    }
    for (uint32_t i = 0; i < 200 * 10000; ++i) {
        bool isset = (i % 200 == 0);
        bool rset = roaring_bitmap_contains(r1, i);
        assert(isset == rset);
    }
    roaring_bitmap_free(r1);
    return 1;
}

int test_intersection_array_x_array() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert(r1);
    assert(r2);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);

        assert(roaring_bitmap_get_cardinality(r2) == 2 * (i + 1));
        assert(roaring_bitmap_get_cardinality(r1) == 2 * (i + 1));
    }

    roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    assert(roaring_bitmap_get_cardinality(r1_and_r2) == 2 * 34);
    roaring_bitmap_free(r1_and_r2);
    return 1;
}

int test_intersection_array_x_array_inplace() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert(r1);
    assert(r2);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);

        assert(roaring_bitmap_get_cardinality(r2) == 2 * (i + 1));
        assert(roaring_bitmap_get_cardinality(r1) == 2 * (i + 1));
    }

    roaring_bitmap_and_inplace(r1, r2);
    roaring_bitmap_free(r2);
    assert(roaring_bitmap_get_cardinality(r1) == 2 * 34);
    roaring_bitmap_free(r1);
    return 1;
}

int test_intersection_bitset_x_bitset() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert(r1);
    assert(r2);

    for (uint32_t i = 0; i < 20000; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r2, 3 * i + 1);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i + 1);

        assert(roaring_bitmap_get_cardinality(r1) == 2 * (i + 1));
        assert(roaring_bitmap_get_cardinality(r2) == 4 * (i + 1));
    }

    roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    // printf("resultant cardinality is
    // %d\n",roaring_bitmap_get_cardinality(r1_and_r2));

    assert(roaring_bitmap_get_cardinality(r1_and_r2) ==
           26666);  // NOT analytically determined but seems reasonable
    roaring_bitmap_free(r1_and_r2);
    return 1;
}

int test_intersection_bitset_x_bitset_inplace() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert(r1);
    assert(r2);

    for (uint32_t i = 0; i < 20000; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r2, 3 * i + 1);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i + 1);

        assert(roaring_bitmap_get_cardinality(r1) == 2 * (i + 1));
        assert(roaring_bitmap_get_cardinality(r2) == 4 * (i + 1));
    }

    roaring_bitmap_and_inplace(r1, r2);
    roaring_bitmap_free(r2);
    printf("resultant cardinality is %d\n", roaring_bitmap_get_cardinality(r1));

    assert(roaring_bitmap_get_cardinality(r1) == 26666);
    roaring_bitmap_free(r1);
    return 1;
}

int test_union() {
    printf("[%s] %s\n", __FILE__, __func__);
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert(r1);
    assert(r2);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        assert(roaring_bitmap_get_cardinality(r2) == i + 1);
        assert(roaring_bitmap_get_cardinality(r1) == i + 1);
    }

    roaring_bitmap_t *r1_or_r2 = roaring_bitmap_or(r1, r2);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    assert(roaring_bitmap_get_cardinality(r1_or_r2) == 166);
    roaring_bitmap_free(r1_or_r2);
    return 1;
}

static roaring_bitmap_t *make_roaring_from_array(uint32_t *a, int len) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    for (int i = 0; i < len; ++i) roaring_bitmap_add(r1, a[i]);
    return r1;
}

int test_conversion_to_int_array() {
    printf("[%s] %s\n", __FILE__, __func__);
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

    printf("arrays have %d and %d\n", card, ans_ctr);
    show_structure(r1->high_low_container);
    assert(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
    return 1;
}

int test_conversion_to_int_array_with_runoptimize() {
    printf("[%s] %s\n", __FILE__, __func__);
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
    bool b = roaring_bitmap_run_optimize(r1);
    assert(b);

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    printf("arrays have %d and %d\n", card, ans_ctr);
    show_structure(r1->high_low_container);
    assert(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
    return 1;
}

int test_array_to_run() {
    printf("[%s] %s\n", __FILE__, __func__);
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // array container  (best done with runs)
    for (uint32_t i = 0; i < 500; ++i) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    bool b = roaring_bitmap_run_optimize(r1);
    assert(b);

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    show_structure(r1->high_low_container);
    assert(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
    return 1;
}

int test_array_to_self() {
    int ans_ctr = 0;
    printf("[%s] %s\n", __FILE__, __func__);

    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // array container  (best not done with runs)
    for (uint32_t i = 0; i < 500; i += 2) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    bool b = roaring_bitmap_run_optimize(r1);
    assert(!b);

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    show_structure(r1->high_low_container);
    assert(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
    return 1;
}

int test_bitset_to_self() {
    printf("[%s] %s\n", __FILE__, __func__);
    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // bitset container  (best not done with runs)
    for (uint32_t i = 0; i < 50000; i += 2) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    bool b = roaring_bitmap_run_optimize(r1);
    assert(!b);

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    show_structure(r1->high_low_container);
    assert(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
    return 1;
}

int test_bitset_to_run() {
    printf("[%s] %s\n", __FILE__, __func__);

    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // bitset container  (best done with runs)
    for (uint32_t i = 0; i < 50000; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    bool b = roaring_bitmap_run_optimize(r1);
    assert(b);

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    show_structure(r1->high_low_container);
    assert(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
    return 1;
}

// not sure how to get containers that are runcontainers but not efficient

int test_run_to_self() {
    printf("[%s] %s\n", __FILE__, __func__);

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
    assert(b);  // still true there is a runcontainer

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    show_structure(r1->high_low_container);
    assert(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
    return 1;
}

int test_remove_run_to_bitset() {
    printf("[%s] %s\n", __FILE__, __func__);
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
    b = roaring_bitmap_remove_run_compression(r1);
    assert(b);  // removal done
    b = roaring_bitmap_run_optimize(r1);
    assert(b);  // there is again a run container

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    show_structure(r1->high_low_container);
    assert(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
    return 1;
}

int test_remove_run_to_array() {
    printf("[%s] %s\n", __FILE__, __func__);

    int ans_ctr = 0;
    uint32_t *ans = calloc(100000, sizeof(int32_t));

    // array  (best done with runs)
    for (uint32_t i = 0; i < 500; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    bool b = roaring_bitmap_run_optimize(r1);  // will make a run container
    b = roaring_bitmap_remove_run_compression(r1);
    assert(b);  // removal done
    b = roaring_bitmap_run_optimize(r1);
    assert(b);  // there is again a run container

    uint32_t card;
    uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

    show_structure(r1->high_low_container);
    assert(array_equals(arr, card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
    return 1;
}

int main() {
    int passed = 0;
    passed += test_printf();
    passed += test_serialize();
    passed += test_portable_serialize();
    passed += test_iterate();
    passed += test_add();
    passed += test_contains();
    passed += test_intersection_array_x_array();
    passed += test_intersection_array_x_array_inplace();
    passed += test_intersection_bitset_x_bitset();
    passed += test_intersection_bitset_x_bitset_inplace();
    passed += test_union();
    passed += test_conversion_to_int_array();
    passed += test_array_to_run();
    passed += test_array_to_self();
    passed += test_bitset_to_run();
    passed += test_bitset_to_self();
    passed += test_run_to_self();
    // passed +=  test_run_to_bitset();  not sure how to do this
    // passed += test_run_to_array();
    passed += test_remove_run_to_bitset();
    passed += test_remove_run_to_array();
    passed += test_conversion_to_int_array_with_runoptimize();
    printf("done %d toplevel tests\n", passed);
}
