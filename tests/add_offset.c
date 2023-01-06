#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/roaring.h>
#include <roaring/misc/configreport.h>

// include internal headers for invasive testing
#include <roaring/containers/containers.h>
#include <roaring/roaring_array.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
    using namespace roaring::internal;
#endif

#include "test.h"

#define ARRAY_SIZE(x) sizeof(x)/sizeof(*x)

typedef struct {
    const char *name;
    uint16_t   *values;
    size_t      n_values;
    uint16_t    offset;
    uint8_t     type;
} container_add_offset_test_case_t;

typedef struct {
    container_add_offset_test_case_t test_case;
    container_t *in, *lo, *hi, *lo_only, *hi_only;
} container_add_offset_test_state_t;

static int setup_container_add_offset_test(void **state_) {
    container_add_offset_test_state_t *state = *(container_add_offset_test_state_t**)state_;
    container_add_offset_test_case_t test = state->test_case;
    bitset_container_t *bc;
    array_container_t *ac;
    run_container_t *rc;

    switch (test.type) {
    case BITSET_CONTAINER_TYPE:
        bc = bitset_container_create();
        assert_true(bc != NULL);
        for (size_t i = 0; i < test.n_values; i++) {
            bitset_container_add(bc, test.values[i]);
        }
        state->in = bc;
        break;
    case ARRAY_CONTAINER_TYPE:
        ac = array_container_create();
        assert_true(ac != NULL);
        for (size_t i = 0; i < test.n_values; i++) {
            array_container_add(ac, test.values[i]);
        }
        state->in = ac;
        break;
    case RUN_CONTAINER_TYPE:
        rc = run_container_create();
        assert_true(rc != NULL);
        for (size_t i = 0; i < test.n_values; i++) {
            run_container_add(rc, test.values[i]);
        }
        state->in = rc;
        break;
    default:
        assert_true(false); // To catch buggy tests.
    }

    return 0;
}

static int teardown_container_add_offset_test(void **state_) {
    container_add_offset_test_state_t *state = *(container_add_offset_test_state_t**)state_;
    container_add_offset_test_case_t test = state->test_case;
    if (state->in) {
        container_free(state->in, test.type);
        state->in = NULL;
    }
    if (state->lo) {
        container_free(state->lo, test.type);
        state->lo = NULL;
    }
    if (state->hi) {
        container_free(state->hi, test.type);
        state->hi = NULL;
    }
    if (state->lo_only) {
        container_free(state->lo_only, test.type);
        state->lo_only = NULL;
    }
    if (state->hi_only) {
        container_free(state->hi_only, test.type);
        state->hi_only = NULL;
    }

    return 0;
}

static void container_add_offset_test(void **state_) {
    container_add_offset_test_state_t *state = *(container_add_offset_test_state_t**)state_;
    container_add_offset_test_case_t test = state->test_case;
    uint16_t offset = test.offset;
    uint8_t type = test.type;
    int card_lo = 0, card_hi = 0;

    assert_true(test.n_values > 0);

    container_add_offset(state->in, type, &state->lo, &state->hi, offset);
    container_add_offset(state->in, type, NULL, &state->hi_only, offset);
    container_add_offset(state->in, type, &state->lo_only, NULL, offset);

    if ((int)offset+test.values[0] > UINT16_MAX) {
        assert_null(state->lo);
        assert_null(state->lo_only);
    } else {
        assert_non_null(state->lo);
        assert_non_null(state->lo_only);
        assert_true(container_equals(state->lo, type, state->lo_only, type));
        card_lo = container_get_cardinality(state->lo, type);
    }
    if ((int)offset+test.values[test.n_values-1] <= UINT16_MAX) {
        assert_null(state->hi);
        assert_null(state->hi_only);
    } else {
        assert_non_null(state->hi);
        assert_non_null(state->hi_only);
        assert_true(container_equals(state->hi, type, state->hi_only, type));
        card_hi = container_get_cardinality(state->hi, type);
    }

    assert_int_equal(test.n_values, card_lo+card_hi);

    size_t i = 0;
    for (; i < test.n_values && (int)offset+test.values[i] <= UINT16_MAX; i++) {
        assert_true(container_contains(state->lo, offset+test.values[i], type));
    }
    for (; i < test.n_values; i++) {
        assert_true(container_contains(state->hi, offset+test.values[i], type));
    }
}


typedef struct {
    const char *name;
    uint32_t   *values;
    size_t      n_values;
    int64_t     offset;
} roaring_add_offset_test_case_t;

typedef struct {
    roaring_add_offset_test_case_t test_case;
    roaring_bitmap_t *in, *forward, *back, *neg_forward, *neg_back;
} roaring_add_offset_test_state_t;

static int setup_roaring_add_offset_test(void **state_) {
    roaring_add_offset_test_state_t *state = *(roaring_add_offset_test_state_t**)state_;
    roaring_add_offset_test_case_t test = state->test_case;

    state->in = roaring_bitmap_of_ptr(test.n_values, test.values);
    assert_true(state->in != NULL);

    return 0;
}

static int teardown_roaring_add_offset_test(void **state_) {
    roaring_add_offset_test_state_t *state = *(roaring_add_offset_test_state_t**)state_;
    if (state->in) {
        roaring_bitmap_free(state->in);
        state->in = NULL;
    }
    if (state->forward) {
        roaring_bitmap_free(state->forward);
        state->forward = NULL;
    }
    if (state->back) {
        roaring_bitmap_free(state->back);
        state->back = NULL;
    }
    if (state->neg_forward) {
        roaring_bitmap_free(state->neg_forward);
        state->neg_forward = NULL;
    }
    if (state->neg_back) {
        roaring_bitmap_free(state->neg_back);
        state->neg_back = NULL;
    }

    return 0;
}

static void assert_roaring_offset(const roaring_bitmap_t *in, const roaring_bitmap_t *out, int64_t offset) {
    roaring_uint32_iterator_t it;
    size_t card;

    assert_non_null(out);
    assert_ptr_not_equal(in, out);

    roaring_init_iterator(in, &it);
    card = 0;
    while(it.has_value) {
        if (offset+it.current_value < 0) {
            roaring_advance_uint32_iterator(&it);
            continue;
        }
        if (offset+it.current_value >= UINT32_MAX) {
            roaring_advance_uint32_iterator(&it);
            continue;
        }
        card++;
        assert_true(roaring_bitmap_contains(out, offset+it.current_value));
        roaring_advance_uint32_iterator(&it);
    }
    assert_int_equal(card, roaring_bitmap_get_cardinality(out));
}

static void roaring_add_offset_test(void **state_) {
    roaring_add_offset_test_state_t *state = *(roaring_add_offset_test_state_t**)state_;
    roaring_add_offset_test_case_t test = state->test_case;
    int64_t offset = test.offset;

    state->forward = roaring_bitmap_add_offset(state->in, offset);
    assert_roaring_offset(state->in, state->forward, offset);

    state->back = roaring_bitmap_add_offset(state->forward, -offset);
    assert_roaring_offset(state->forward, state->back, -offset);

    state->neg_forward = roaring_bitmap_add_offset(state->in, -offset);
    assert_roaring_offset(state->in, state->neg_forward, -offset);

    state->neg_back = roaring_bitmap_add_offset(state->neg_forward, offset);
    assert_roaring_offset(state->neg_forward, state->neg_back, offset);
}

#define BITSET_ADD_OFFSET_TEST_CASE(vals, offset) { \
    { \
        "bitset_" #vals "_offset_" #offset, \
        vals, ARRAY_SIZE(vals), \
        offset, \
        BITSET_CONTAINER_TYPE \
    }, NULL, NULL, NULL, NULL, NULL \
}

#define ARRAY_ADD_OFFSET_TEST_CASE(vals, offset) { \
    { \
        "array_" #vals "_offset_" #offset, \
        vals, ARRAY_SIZE(vals), \
        offset, \
        ARRAY_CONTAINER_TYPE \
    }, NULL, NULL, NULL, NULL, NULL \
}

#define RUN_ADD_OFFSET_TEST_CASE(vals, offset) { \
    { \
        "run_" #vals "_offset_" #offset, \
        vals, ARRAY_SIZE(vals), \
        offset, \
        RUN_CONTAINER_TYPE \
    }, NULL, NULL, NULL, NULL, NULL \
}

#define CONTAINER_ADD_OFFSET_TEST(state) { \
    state.test_case.name, \
    container_add_offset_test, \
    setup_container_add_offset_test, \
    teardown_container_add_offset_test, \
    &state}

#define ROARING_ADD_OFFSET_TEST_CASE(vals, offset) { \
    { \
        "roaring_" #vals "_offset_" #offset, \
        vals, ARRAY_SIZE(vals), \
        offset, \
    }, NULL, NULL, NULL, NULL, NULL \
}

#define ROARING_ADD_OFFSET_TEST(state) { \
    state.test_case.name, \
    roaring_add_offset_test, \
    setup_roaring_add_offset_test, \
    teardown_roaring_add_offset_test, \
    &state}

int main() {
    tellmeall();

    static uint16_t range_100_1000[900];
    for (uint16_t i = 0, v = 100; i < 900; i++, v++) {
        range_100_1000[i] = v;
    }
    container_add_offset_test_state_t container_state[] = {
        BITSET_ADD_OFFSET_TEST_CASE(range_100_1000, 123),
        BITSET_ADD_OFFSET_TEST_CASE(range_100_1000, UINT16_MAX),
        BITSET_ADD_OFFSET_TEST_CASE(range_100_1000, UINT16_MAX-500),
        ARRAY_ADD_OFFSET_TEST_CASE(range_100_1000, 123),
        ARRAY_ADD_OFFSET_TEST_CASE(range_100_1000, UINT16_MAX),
        ARRAY_ADD_OFFSET_TEST_CASE(range_100_1000, UINT16_MAX-500),
        RUN_ADD_OFFSET_TEST_CASE(range_100_1000, 123),
        RUN_ADD_OFFSET_TEST_CASE(range_100_1000, UINT16_MAX),
        RUN_ADD_OFFSET_TEST_CASE(range_100_1000, UINT16_MAX-500),
    };

    uint32_t sparse_bitmap[] = {5580, 33722, 44031, 57276, 83097};
    uint32_t dense_bitmap[5 + (200000-100000)/4];
    size_t i, j;

    i = 0;
    dense_bitmap[i++] = 10;
    dense_bitmap[i++] = UINT16_MAX;
    dense_bitmap[i++] = 0x010101;
    for (j = 100000; j < 200000; j += 4) {
        dense_bitmap[i++] = j;
    }
    dense_bitmap[i++] = 400000;
    dense_bitmap[i++] = 1400000;

    assert_true(i == ARRAY_SIZE(dense_bitmap));

    // NB: only add positive offsets, the test function takes care of also
    // running a negative test for that offset.
    roaring_add_offset_test_state_t roaring_state[50] = {
        ROARING_ADD_OFFSET_TEST_CASE(sparse_bitmap, 0),
        ROARING_ADD_OFFSET_TEST_CASE(sparse_bitmap, 100),
        ROARING_ADD_OFFSET_TEST_CASE(sparse_bitmap, 25000),
        ROARING_ADD_OFFSET_TEST_CASE(sparse_bitmap, 83097),
        ROARING_ADD_OFFSET_TEST_CASE(sparse_bitmap, UINT32_MAX),
        ROARING_ADD_OFFSET_TEST_CASE(sparse_bitmap, UINT32_MAX-UINT16_MAX),
        ROARING_ADD_OFFSET_TEST_CASE(sparse_bitmap, UINT32_MAX-UINT16_MAX+1),
        ROARING_ADD_OFFSET_TEST_CASE(sparse_bitmap, ((uint64_t)1) << 50),
        ROARING_ADD_OFFSET_TEST_CASE(sparse_bitmap, 281474976710657L),
    };
    i = 9;
    for (int64_t offset = 3; offset < 1000000; offset *= 3) {
        roaring_add_offset_test_state_t state = ROARING_ADD_OFFSET_TEST_CASE(dense_bitmap, offset);
        roaring_state[i++] = state;
    }
    for (int64_t offset = 1024; offset < 1000000; offset *= 2) {
        roaring_add_offset_test_state_t state = ROARING_ADD_OFFSET_TEST_CASE(dense_bitmap, offset);
        roaring_state[i++] = state;
    }
    assert_true(i <= ARRAY_SIZE(roaring_state));

    i = j = 0;
    struct CMUnitTest tests[ARRAY_SIZE(container_state)+ARRAY_SIZE(roaring_state)];
    memset(tests, 0, sizeof(tests));
    for (; i < ARRAY_SIZE(container_state); i++) {
        struct CMUnitTest test = CONTAINER_ADD_OFFSET_TEST(container_state[i]);
        tests[i] = test;
    }
    for (; j < ARRAY_SIZE(roaring_state); i++, j++) {
        struct CMUnitTest test = ROARING_ADD_OFFSET_TEST(roaring_state[j]);
        tests[i] = test;
    }

    return cmocka_run_group_tests(tests, NULL, NULL);
}
