/**
 * Unit tests for the C++ roaring::Roaring64 class (ART-based 64-bit bitmap).
 */
#include <cstdint>
#include <type_traits>
#include <vector>

#include <roaring/roaring64.h>  // C API for comparison
#include <roaring/roaring64.hh>

#include "roaring64_checked.hh"
#include "test.h"

using roaring::Roaring64;

static_assert(std::is_nothrow_move_constructible<Roaring64>::value,
              "Expected Roaring64 to be nothrow move constructible");

DEFINE_TEST(test_cpp_r64_default_empty) {
    Roaring64 r;
    assert_true(r.isEmpty());
    assert_int_equal(r.cardinality(), 0);
}

DEFINE_TEST(test_cpp_r64_copy_and_move) {
    Roaring64 a;
    a.add(uint64_t(1) << 40);
    Roaring64 b(a);  // copy ctor
    assert_int_equal(b.cardinality(), 1);
    Roaring64 c(std::move(a));  // move ctor
    assert_int_equal(c.cardinality(), 1);
    // verify moved-from a is valid for reuse
    a = Roaring64{};
    a.add(99);
    assert_int_equal(a.cardinality(), 1);
    Roaring64 d;
    d = c;  // copy assign
    assert_int_equal(d.cardinality(), 1);
    Roaring64 e;
    e = std::move(c);  // move assign
    assert_int_equal(e.cardinality(), 1);
    // verify moved-from c is valid for reuse
    c = Roaring64{};
    c.add(42);
    assert_int_equal(c.cardinality(), 1);
    e.swap(d);
    assert_int_equal(e.cardinality(), 1);
}

DEFINE_TEST(test_cpp_r64_add_remove_contains_clear) {
    Roaring64 r;
    const uint64_t v = uint64_t(1) << 50;
    assert_false(r.contains(v));
    r.add(v);
    assert_true(r.contains(v));
    assert_int_equal(r.cardinality(), 1);
    r.remove(v);
    assert_false(r.contains(v));
    assert_true(r.isEmpty());
    r.add(1);
    r.add(2);
    r.clear();
    assert_true(r.isEmpty());
}

DEFINE_TEST(test_cpp_r64_construction_helpers) {
    const uint64_t vals[] = {1, uint64_t(1) << 33, uint64_t(1) << 33, 7};
    Roaring64 r;
    r.addMany(4, vals);
    assert_int_equal(r.cardinality(), 3);  // dup collapses

    Roaring64 il{1, 2, 3, 2};
    assert_int_equal(il.cardinality(), 3);

    Roaring64 bof =
        Roaring64::bitmapOf(3, uint64_t(10), uint64_t(20), uint64_t(30));
    assert_int_equal(bof.cardinality(), 3);
    assert_true(bof.contains(20));

    // ctor from a (n, data) array.
    const uint64_t data[] = {100, uint64_t(1) << 60, 100, 7};
    Roaring64 from_data(4, data);
    assert_int_equal(from_data.cardinality(), 3);  // dup collapses
    assert_true(from_data.contains(uint64_t(1) << 60));
    assert_true(from_data.contains(7));
}

DEFINE_TEST(test_cpp_r64_minmax_equals) {
    // Empty bitmap returns sentinel values.
    Roaring64 empty;
    assert_true(empty.minimum() == UINT64_MAX);
    assert_true(empty.maximum() == 0);

    Roaring64 a{5, uint64_t(1) << 40, 7};
    assert_int_equal(a.minimum(), 5);
    assert_int_equal(a.maximum(), uint64_t(1) << 40);

    Roaring64 b{7, 5, uint64_t(1) << 40};
    assert_true(a == b);

    b.add(9);
    assert_false(a == b);
}

DEFINE_TEST(test_cpp_r64_set_ops) {
    const uint64_t K1 = uint64_t(1) << 16;
    const uint64_t K2 = uint64_t(1) << 32;

    Roaring64 a{1, 2, 7, K1 + 1, K1 + 5, K2 + 1};
    Roaring64 b{3, 4, 7, K1 + 5, K1 + 9, K2 + 2};
    //   a only: 1, 2, K1+1, K2+1
    //   b only: 3, 4, K1+9, K2+2
    //   both:   7, K1+5

    Roaring64 a_and_b = a & b;
    assert_true((a_and_b == Roaring64{7, K1 + 5}));

    Roaring64 a_or_b = a | b;
    assert_true((a_or_b == Roaring64{1, 2, 3, 4, 7, K1 + 1, K1 + 5, K1 + 9,
                                     K2 + 1, K2 + 2}));

    Roaring64 a_xor_b = a ^ b;
    assert_true(
        (a_xor_b == Roaring64{1, 2, 3, 4, K1 + 1, K1 + 9, K2 + 1, K2 + 2}));

    Roaring64 a_minus_b = a - b;
    assert_true((a_minus_b == Roaring64{1, 2, K1 + 1, K2 + 1}));

    Roaring64 c = a;
    c &= b;
    assert_true(c == a_and_b);
    c = a;
    c |= b;
    assert_true(c == a_or_b);
    c = a;
    c ^= b;
    assert_true(c == a_xor_b);
    c = a;
    c -= b;
    assert_true(c == a_minus_b);
}

DEFINE_TEST(test_cpp_r64_iteration) {
    Roaring64 r{uint64_t(1) << 40, 5, 7, 5};

    std::vector<uint64_t> seen;
    for (auto it = r.begin(); it != r.end(); ++it) {
        seen.push_back(*it);
    }
    // Iterates in ascending order, duplicates collapsed.
    std::vector<uint64_t> expected = {5, 7, uint64_t(1) << 40};
    // Also test iteration with postfix ++
    size_t postfix_count = 0;
    for (auto it = r.begin(); it != r.end(); it++) {
        ++postfix_count;
    }
    assert_int_equal(postfix_count, expected.size());
    assert_int_equal(seen.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert_int_equal(seen[i], expected[i]);
    }

    // Range-based for loop yields the same ascending results.
    std::vector<uint64_t> ranged;
    for (uint64_t x : r) {
        ranged.push_back(x);
    }
    assert_int_equal(ranged.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert_int_equal(ranged[i], expected[i]);
    }

    std::vector<uint64_t> arr(r.cardinality());
    r.toArray(arr.data());
    assert_int_equal(arr.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert_int_equal(arr[i], expected[i]);
    }

    // Empty bitmap: begin() == end().
    Roaring64 empty;
    assert_true(empty.begin() == empty.end());

    // Iterator copy assignment and move operations.
    Roaring64::const_iterator it1 = r.begin();  // at 5
    Roaring64::const_iterator it2 = r.end();
    it2 = it1;  // copy assignment
    assert_true(it2 == it1);
    assert_int_equal(*it2, uint64_t(5));

    Roaring64::const_iterator it3 = r.begin();
    ++it3;                                            // at 7
    Roaring64::const_iterator moved(std::move(it3));  // move construction
    assert_int_equal(*moved, uint64_t(7));

    Roaring64::const_iterator it4 = r.end();
    it4 = std::move(moved);  // move assignment
    assert_int_equal(*it4, uint64_t(7));
}

DEFINE_TEST(test_cpp_r64_random_vs_set) {
    doublechecked::Roaring64 r;
    std::vector<uint64_t> added;  // values that have been added

    uint64_t seed = 987654321;
    auto next = [&seed]() {
        seed = 6364136223846793005ULL * seed + 1442695040888963407ULL;
        return seed;
    };

    const uint64_t seedvals[] = {1, uint64_t(1) << 40, uint64_t(1) << 50,
                                 12345};
    r.addMany(4, seedvals);
    for (uint64_t v : seedvals) added.push_back(v);

    for (size_t i = 0; i < 5000; ++i) {
        if (!added.empty() && (next() & 3) == 0) {  // 25%
            // Remove an existing value.
            uint64_t v = added[next() % added.size()];
            r.remove(v);
        } else {  // 75%
            // Add a new value.
            uint64_t high48 =
                next() & 0xFFFFFFFFFFFFULL;    // 48-bit container key
            uint64_t low16 = next() & 0xFFFF;  // 16-bit container offset
            uint64_t v = (high48 << 16) | low16;
            r.add(v);
            added.push_back(v);
        }
        if ((i % 500) == 0) {
            (void)r.cardinality();
            if (!added.empty()) {
                (void)r.contains(added[next() % added.size()]);
            }
            (void)r.isEmpty();
        }
    }
    r.validate();  // manually validate

    // exercise clear
    r.clear();
    assert_true(r.isEmpty());
    r.validate();
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cpp_r64_default_empty),
        cmocka_unit_test(test_cpp_r64_copy_and_move),
        cmocka_unit_test(test_cpp_r64_add_remove_contains_clear),
        cmocka_unit_test(test_cpp_r64_construction_helpers),
        cmocka_unit_test(test_cpp_r64_minmax_equals),
        cmocka_unit_test(test_cpp_r64_set_ops),
        cmocka_unit_test(test_cpp_r64_iteration),
        cmocka_unit_test(test_cpp_r64_random_vs_set),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
