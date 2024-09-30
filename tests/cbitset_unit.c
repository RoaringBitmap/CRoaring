#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/bitset/bitset.h>

#include "test.h"

int compute_cardinality(bitset_t *b) {
    size_t k = 0;
    for (size_t i = 0; bitset_next_set_bit(b, &i); i++) {
        k += 1;
    }
    return k;
}

void test_iterate() {
    bitset_t *b = bitset_create();
    for (int k = 0; k < 1000; ++k) bitset_set(b, 3 * k);
    assert_true(bitset_count(b) == 1000);
    assert_true(compute_cardinality(b) == 1000);
    size_t k = 0;
    for (size_t i = 0; bitset_next_set_bit(b, &i); i++) {
        assert_true(i == k);
        k += 3;
    }
    assert_true(k == 3000);
    bitset_free(b);
}

bool increment(size_t value, void *param) {
    size_t k;
    memcpy(&k, param, sizeof(size_t));
    assert_true(value == k);
    k += 3;
    memcpy(param, &k, sizeof(size_t));
    return true;
}

void test_iterate2() {
    bitset_t *b = bitset_create();
    for (int k = 0; k < 1000; ++k) bitset_set(b, 3 * k);
    assert_true(compute_cardinality(b) == 1000);
    assert_true(bitset_count(b) == 1000);
    size_t k = 0;
    bitset_for_each(b, increment, &k);
    assert_true(k == 3000);
    bitset_free(b);
}

void test_construct() {
    bitset_t *b = bitset_create();
    for (int k = 0; k < 1000; ++k) bitset_set(b, 3 * k);
    assert_true(compute_cardinality(b) == 1000);
    assert_true(bitset_count(b) == 1000);
    for (int k = 0; k < 3 * 1000; ++k)
        assert_true(bitset_get(b, k) == (k / 3 * 3 == k));
    bitset_free(b);
}

void test_max_min() {
    bitset_t *b = bitset_create();
    assert_true(bitset_empty(b));
    for (size_t k = 100; k < 1000; ++k) {
        bitset_set(b, 3 * k);
        assert_true(bitset_minimum(b) == 3 * 100);
        assert_true(bitset_maximum(b) == 3 * k);
    }
    bitset_free(b);
}

void test_shift_left() {
    for (size_t sh = 0; sh < 256; sh++) {
        bitset_t *b = bitset_create();
        int power = 3;
        size_t s1 = 100;
        size_t s2 = 5000;
        for (size_t k = s1; k < s2; ++k) {
            bitset_set(b, power * k);
        }
        int mycount = bitset_count(b);
        assert_true(compute_cardinality(b) == mycount);
        bitset_shift_left(b, sh);
        assert_true(bitset_count(b) == (size_t)mycount);
        assert_true(compute_cardinality(b) == mycount);
        for (size_t k = s1; k < s2; ++k) {
            assert_true(bitset_get(b, power * k + sh));
        }
        bitset_free(b);
    }
}

void test_set_to_val() {
    bitset_t *b = bitset_create();
    bitset_set_to_value(b, 1, true);
    bitset_set_to_value(b, 1, false);
    bitset_set_to_value(b, 10, false);
    bitset_set_to_value(b, 10, true);
    assert_true(bitset_get(b, 10));
    assert_true(!bitset_get(b, 1));
    bitset_free(b);
}

void test_shift_right() {
    for (size_t sh = 0; sh < 256; sh++) {
        bitset_t *b = bitset_create();
        int power = 3;
        size_t s1 = 100 + sh;
        size_t s2 = s1 + 5000;
        for (size_t k = s1; k < s2; ++k) {
            bitset_set(b, power * k);
        }
        size_t mycount = bitset_count(b);
        bitset_shift_right(b, sh);
        assert_true(bitset_count(b) == mycount);
        for (size_t k = s1; k < s2; ++k) {
            assert_true(bitset_get(b, power * k - sh));
        }
        bitset_free(b);
    }
}

void test_union_intersection() {
    bitset_t *b1 = bitset_create();
    bitset_t *b2 = bitset_create();

    for (int k = 0; k < 1000; ++k) {
        bitset_set(b1, 2 * k);
        bitset_set(b2, 2 * k + 1);
    }
    // calling xor twice should leave things unchanged
    bitset_inplace_symmetric_difference(b1, b2);
    assert_true(bitset_count(b1) == 2000);
    bitset_inplace_symmetric_difference(b1, b2);
    assert_true(bitset_count(b1) == 1000);
    bitset_inplace_difference(b1, b2);  // should make no difference
    assert_true(bitset_count(b1) == 1000);
    bitset_inplace_union(b1, b2);
    assert_true(bitset_count(b1) == 2000);
    bitset_inplace_intersection(b1, b2);
    assert_true(bitset_count(b1) == 1000);
    bitset_inplace_difference(b1, b2);
    assert_true(bitset_count(b1) == 0);
    bitset_inplace_union(b1, b2);
    bitset_inplace_difference(b2, b1);
    assert_true(bitset_count(b2) == 0);
    bitset_free(b1);
    bitset_free(b2);
}

void test_counts() {
    bitset_t *b1 = bitset_create();
    bitset_t *b2 = bitset_create();

    for (int k = 0; k < 1000; ++k) {
        bitset_set(b1, 2 * k);
        bitset_set(b2, 3 * k);
    }
    assert_true(bitset_intersection_count(b1, b2) == 334);
    assert_true(bitset_union_count(b1, b2) == 1666);
    bitset_free(b1);
    bitset_free(b2);
}

/* Creates 2 bitsets, one containing even numbers the other odds.
Checks bitsets_disjoint() returns that they are disjoint, then sets a common
bit between both sets and checks that they are no longer disjoint. */
void test_disjoint() {
    bitset_t *evens = bitset_create();
    bitset_t *odds = bitset_create();

    for (int i = 0; i < 1000; i++) {
        if (i % 2 == 0)
            bitset_set(evens, i);
        else
            bitset_set(odds, i);
    }

    assert_true(bitsets_disjoint(evens, odds));

    bitset_set(evens, 501);
    bitset_set(odds, 501);

    assert_true(!bitsets_disjoint(evens, odds));

    bitset_free(evens);
    bitset_free(odds);
}

/* Creates 2 bitsets, one containing even numbers the other odds.
Checks that bitsets_intersect() returns that they do not intersect, then sets
a common bit and checks that they now intersect. */
void test_intersects() {
    bitset_t *evens = bitset_create();
    bitset_t *odds = bitset_create();

    for (int i = 0; i < 1000; i++) {
        if (i % 2 == 0)
            bitset_set(evens, i);
        else
            bitset_set(odds, i);
    }

    assert_true(!bitsets_intersect(evens, odds));

    bitset_set(evens, 1001);
    bitset_set(odds, 1001);

    assert_true(bitsets_intersect(evens, odds));

    bitset_free(evens);
    bitset_free(odds);
}
/* Create 2 bitsets with different capacity, where the bigger superset
contains the subset bits plus additional bits after the subset arraysize.
Checks that the bitset_contains_all() returns false when checking if
the superset contains all the subset bits, and true in the opposite case. */
void test_contains_all_different_sizes() {
    const size_t superset_size = 10;
    const size_t subset_size = 5;

    bitset_t *superset = bitset_create_with_capacity(superset_size);
    bitset_t *subset = bitset_create_with_capacity(subset_size);

    bitset_set(superset, 1);
    bitset_set(superset, subset_size - 1);
    bitset_set(superset, subset_size + 1);

    bitset_set(subset, 1);
    bitset_set(subset, subset_size - 1);

    assert_true(bitset_contains_all(superset, subset));
    assert_true(!bitset_contains_all(subset, superset));

    bitset_free(superset);
    bitset_free(subset);
}

/* Creates 2 bitsets, one with all bits from 0->1000 set, the other with only
even bits set in the same range. Checks that the bitset_contains_all()
returns true, then sets a single bit at 1001 in the prior subset and checks that
bitset_contains_all() returns false. */
void test_contains_all() {
    bitset_t *superset = bitset_create();
    bitset_t *subset = bitset_create();

    for (int i = 0; i < 1000; i++) {
        bitset_set(superset, i);
        if (i % 2 == 0) bitset_set(subset, i);
    }

    assert_true(bitset_contains_all(superset, subset));
    assert_true(!bitset_contains_all(subset, superset));

    bitset_set(subset, 1001);

    assert_true(!bitset_contains_all(superset, subset));
    assert_true(!bitset_contains_all(subset, superset));

    bitset_free(superset);
    bitset_free(subset);
}

int main() {
    test_set_to_val();
    test_construct();
    test_union_intersection();
    test_iterate();
    test_iterate2();
    test_max_min();
    test_counts();
    test_shift_right();
    test_shift_left();
    test_disjoint();
    test_intersects();
    test_contains_all();
    test_contains_all_different_sizes();
    printf("All asserts passed. Code is probably ok.\n");
}
