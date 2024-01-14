#include <roaring/roaring64.h>

#include <array>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "test.h"

using namespace roaring::api;

namespace {

DEFINE_TEST(test_copy) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 0);
    roaring64_bitmap_add(r1, 10000);
    roaring64_bitmap_add(r1, 200000);

    roaring64_bitmap_t* r2 = roaring64_bitmap_copy(r1);
    assert_true(roaring64_bitmap_contains(r2, 0));
    assert_true(roaring64_bitmap_contains(r2, 10000));
    assert_true(roaring64_bitmap_contains(r2, 200000));

    roaring64_bitmap_remove(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    assert_true(roaring64_bitmap_contains(r2, 200000));
    assert_false(roaring64_bitmap_contains(r2, 300000));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_from_range) {
    {
        // Step greater than 2 ^ 16.
        roaring64_bitmap_t* r = roaring64_bitmap_from_range(0, 1000000, 200000);
        assert_true(roaring64_bitmap_contains(r, 0));
        assert_true(roaring64_bitmap_contains(r, 200000));
        assert_true(roaring64_bitmap_contains(r, 400000));
        assert_true(roaring64_bitmap_contains(r, 600000));
        assert_true(roaring64_bitmap_contains(r, 800000));
        assert_false(roaring64_bitmap_contains(r, 1000000));
        roaring64_bitmap_free(r);
    }
    {
        // Step less than 2 ^ 16 and within one container.
        roaring64_bitmap_t* r = roaring64_bitmap_from_range(0, 100, 20);
        assert_true(roaring64_bitmap_contains(r, 0));
        assert_true(roaring64_bitmap_contains(r, 20));
        assert_true(roaring64_bitmap_contains(r, 40));
        assert_true(roaring64_bitmap_contains(r, 60));
        assert_true(roaring64_bitmap_contains(r, 80));
        assert_false(roaring64_bitmap_contains(r, 100));
        roaring64_bitmap_free(r);
    }
    {
        // Step less than 2 ^ 16 and across two containers.
        roaring64_bitmap_t* r =
            roaring64_bitmap_from_range((1 << 16) - 1, (1 << 16) + 5, 2);
        assert_true(roaring64_bitmap_contains(r, (1 << 16) - 1));
        assert_true(roaring64_bitmap_contains(r, (1 << 16) + 1));
        assert_true(roaring64_bitmap_contains(r, (1 << 16) + 3));
        assert_false(roaring64_bitmap_contains(r, (1 << 16) + 5));
        roaring64_bitmap_free(r);
    }
    {
        // Step less than 2 ^ 16 and across multiple containers.
        roaring64_bitmap_t* r =
            roaring64_bitmap_from_range((1 << 16) - 1, (1 << 17) + 2, 1);
        assert_true(roaring64_bitmap_contains(r, (1 << 16) - 1));
        assert_true(roaring64_bitmap_contains(r, (1 << 16) + 0));
        assert_true(roaring64_bitmap_contains(r, (1 << 16) + 1));
        assert_true(roaring64_bitmap_contains(r, (1 << 17) - 1));
        assert_true(roaring64_bitmap_contains(r, (1 << 17) + 0));
        assert_true(roaring64_bitmap_contains(r, (1 << 17) + 1));
        assert_false(roaring64_bitmap_contains(r, (1 << 17) + 2));
        roaring64_bitmap_free(r);
    }
}

DEFINE_TEST(test_of_ptr) {
    std::array<uint64_t, 1000> vals;
    std::iota(vals.begin(), vals.end(), 0);
    roaring64_bitmap_t* r = roaring64_bitmap_of_ptr(vals.size(), vals.data());
    for (uint64_t i = 0; i < 1000; ++i) {
        assert_true(roaring64_bitmap_contains(r, vals[i]));
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_of) {
    roaring64_bitmap_t* r = roaring64_bitmap_of(3, 1ULL, 20000ULL, 500000ULL);
    assert_true(roaring64_bitmap_contains(r, 1));
    assert_true(roaring64_bitmap_contains(r, 20000));
    assert_true(roaring64_bitmap_contains(r, 500000));
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_add) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, 10000);
    roaring64_bitmap_add(r, 200000);

    assert_true(roaring64_bitmap_contains(r, 0));
    assert_true(roaring64_bitmap_contains(r, 10000));
    assert_true(roaring64_bitmap_contains(r, 200000));

    assert_false(roaring64_bitmap_contains(r, 1));

    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_add_checked) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    assert_true(roaring64_bitmap_add_checked(r, 0));
    assert_false(roaring64_bitmap_add_checked(r, 0));
    assert_true(roaring64_bitmap_add_checked(r, 10000));
    assert_false(roaring64_bitmap_add_checked(r, 10000));
    assert_true(roaring64_bitmap_add_checked(r, 200000));
    assert_false(roaring64_bitmap_add_checked(r, 200000));

    assert_true(roaring64_bitmap_contains(r, 0));
    assert_true(roaring64_bitmap_contains(r, 10000));
    assert_true(roaring64_bitmap_contains(r, 200000));

    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_add_bulk) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bulk_context_t context{};
    for (uint64_t i = 0; i < 10000; ++i) {
        roaring64_bitmap_add_bulk(r, &context, i * 10000);
    }
    for (uint64_t i = 0; i < 10000; ++i) {
        assert_true(roaring64_bitmap_contains(r, i * 10000));
    }

    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_add_many) {
    {
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        std::array<uint64_t, 1000> vals;
        std::iota(vals.begin(), vals.end(), 0);

        roaring64_bitmap_add_many(r, vals.size(), vals.data());
        for (uint64_t i = 0; i < 1000; ++i) {
            assert_true(roaring64_bitmap_contains(r, vals[i]));
        }

        roaring64_bitmap_free(r);
    }

    {
        // Add many_where value already exists
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        uint64_t value = 0;
        roaring64_bitmap_add(r, value);
        assert_true(roaring64_bitmap_contains(r, value));
        roaring64_bitmap_add_many(r, 1, &value);
        assert_true(roaring64_bitmap_contains(r, value));
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 1);
        roaring64_bitmap_free(r);
    }
}

DEFINE_TEST(test_add_range_closed) {
    {
        // Entire range within one container.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, 10, 20);
        roaring64_bulk_context_t context{};
        assert_false(roaring64_bitmap_contains_bulk(r, &context, 9));
        for (uint64_t i = 10; i <= 20; ++i) {
            assert_true(roaring64_bitmap_contains_bulk(r, &context, i));
        }
        assert_false(roaring64_bitmap_contains_bulk(r, &context, 21));
        roaring64_bitmap_free(r);
    }
    {
        // Range spans two containers.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, (1 << 16) - 10, (1 << 16) + 10);
        roaring64_bulk_context_t context{};
        assert_false(
            roaring64_bitmap_contains_bulk(r, &context, (1 << 16) - 11));
        for (uint64_t i = (1 << 16) - 10; i <= (1 << 16) + 10; ++i) {
            assert_true(roaring64_bitmap_contains_bulk(r, &context, i));
        }
        assert_false(
            roaring64_bitmap_contains_bulk(r, &context, (1 << 16) + 11));
        roaring64_bitmap_free(r);
    }
    {
        // Range spans more than two containers.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, 100, 300000);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 300000 - 100 + 1);
        roaring64_bulk_context_t context{};
        assert_false(roaring64_bitmap_contains_bulk(r, &context, 99));
        for (uint64_t i = 100; i <= 300000; ++i) {
            assert_true(roaring64_bitmap_contains_bulk(r, &context, i));
        }
        assert_false(roaring64_bitmap_contains_bulk(r, &context, 300001));
        roaring64_bitmap_free(r);
    }
    {
        // Add range to existing container
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add(r, 100);
        roaring64_bitmap_add_range_closed(r, 0, 0);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 2);
        assert_true(roaring64_bitmap_contains(r, 0));
        assert_true(roaring64_bitmap_contains(r, 100));
        roaring64_bitmap_free(r);
    }
    {
        // Add a range that spans multiple ART levels (end >> 16 == 0x0101)
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        uint64_t end = 0x101ffff;
        uint64_t start = 0;
        roaring64_bitmap_add_range_closed(r, start, end);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), end - start + 1);
        roaring64_bitmap_free(r);
    }
}

DEFINE_TEST(test_contains_bulk) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    roaring64_bulk_context_t context{};
    for (uint64_t i = 0; i < 10000; ++i) {
        roaring64_bitmap_add_bulk(r, &context, i * 1000);
    }
    context = {};
    for (uint64_t i = 0; i < 10000; ++i) {
        assert_true(roaring64_bitmap_contains_bulk(r, &context, i * 1000));
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_select) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (uint64_t i = 0; i < 100; ++i) {
        roaring64_bitmap_add(r, i * 1000);
    }
    uint64_t element = 0;
    for (uint64_t i = 0; i < 100; ++i) {
        assert_true(roaring64_bitmap_select(r, i, &element));
        assert_int_equal(element, i * 1000);
    }
    assert_false(roaring64_bitmap_select(r, 100, &element));
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_rank) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (uint64_t i = 0; i < 100; ++i) {
        roaring64_bitmap_add(r, i * 1000);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        assert_int_equal(roaring64_bitmap_rank(r, i * 1000), i + 1);
        assert_int_equal(roaring64_bitmap_rank(r, i * 1000 + 1), i + 1);
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_get_index) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (uint64_t i = 0; i < 100; ++i) {
        roaring64_bitmap_add(r, i * 1000);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        uint64_t index = 0;
        assert_true(roaring64_bitmap_get_index(r, i * 1000, &index));
        assert_int_equal(index, i);
        assert_false(roaring64_bitmap_get_index(r, i * 1000 + 1, &index));
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_remove) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (uint64_t i = 0; i < 100; ++i) {
        roaring64_bitmap_add(r, i * 10000);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        assert_true(roaring64_bitmap_contains(r, i * 10000));
    }
    for (uint64_t i = 0; i < 100; ++i) {
        roaring64_bitmap_remove(r, i * 10000);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        assert_false(roaring64_bitmap_contains(r, i * 10000));
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_remove_checked) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (uint64_t i = 0; i < 100; ++i) {
        roaring64_bitmap_add(r, i * 10000);
    }
    for (uint64_t i = 0; i < 100; ++i) {
        assert_true(roaring64_bitmap_remove_checked(r, i * 10000));
        assert_false(roaring64_bitmap_remove_checked(r, i * 10000));
    }
    for (uint64_t i = 0; i < 100; ++i) {
        assert_false(roaring64_bitmap_contains(r, i * 10000));
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_remove_bulk) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    roaring64_bulk_context_t context{};
    for (uint64_t i = 0; i < 10000; ++i) {
        roaring64_bitmap_add_bulk(r, &context, i * 1000);
    }
    context = {};
    for (uint64_t i = 1; i < 9999; ++i) {
        roaring64_bitmap_remove_bulk(r, &context, i * 1000);
    }
    context = {};
    assert_true(roaring64_bitmap_contains_bulk(r, &context, 0));
    for (uint64_t i = 1; i < 9999; ++i) {
        assert_false(roaring64_bitmap_contains_bulk(r, &context, i * 1000));
    }
    assert_true(roaring64_bitmap_contains_bulk(r, &context, 9999000));
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_remove_many) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    std::array<uint64_t, 1000> vals;
    std::iota(vals.begin(), vals.end(), 0);

    roaring64_bitmap_add_many(r, vals.size(), vals.data());
    roaring64_bitmap_remove_many(r, vals.size(), vals.data());
    for (uint64_t i = 0; i < 1000; ++i) {
        assert_false(roaring64_bitmap_contains(r, vals[i]));
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_remove_range_closed) {
    {
        // Entire range within one container.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, 10, 20);
        roaring64_bitmap_remove_range_closed(r, 11, 21);
        roaring64_bulk_context_t context{};
        assert_true(roaring64_bitmap_contains_bulk(r, &context, 10));
        for (uint64_t i = 11; i <= 21; ++i) {
            assert_false(roaring64_bitmap_contains_bulk(r, &context, i));
        }
        roaring64_bitmap_free(r);
    }
    {
        // Range spans two containers.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, (1 << 16) - 10, (1 << 16) + 10);
        roaring64_bitmap_remove_range_closed(r, (1 << 16) - 9, (1 << 16) + 9);
        roaring64_bulk_context_t context{};
        assert_true(
            roaring64_bitmap_contains_bulk(r, &context, (1 << 16) - 10));
        for (uint64_t i = (1 << 16) - 9; i <= (1 << 16) + 9; ++i) {
            assert_false(roaring64_bitmap_contains_bulk(r, &context, i));
        }
        assert_true(
            roaring64_bitmap_contains_bulk(r, &context, (1 << 16) + 10));
        roaring64_bitmap_free(r);
    }
    {
        // Range spans more than two containers.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, 100, 300000);
        roaring64_bitmap_remove_range_closed(r, 101, 299999);
        roaring64_bulk_context_t context{};
        assert_true(roaring64_bitmap_contains_bulk(r, &context, 100));
        for (uint64_t i = 101; i <= 299999; ++i) {
            assert_false(roaring64_bitmap_contains_bulk(r, &context, i));
        }
        assert_true(roaring64_bitmap_contains_bulk(r, &context, 300000));
        roaring64_bitmap_free(r);
    }
    {
        // Range completely clears the bitmap.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        // array container
        roaring64_bitmap_add(r, 1);
        // range container
        roaring64_bitmap_add_range_closed(r, 0x10000, 0x20000);
        // bitmap container
        for (int i = 0x20000; i < 0x25000; i += 2) {
            roaring64_bitmap_add(r, i);
        }
        roaring64_bitmap_remove_range_closed(r, 0, 0x30000);
        assert_true(roaring64_bitmap_is_empty(r));
        roaring64_bitmap_free(r);
    }
}

DEFINE_TEST(test_get_cardinality) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, 100000);
    roaring64_bitmap_add(r, 100001);
    roaring64_bitmap_add(r, 100002);
    roaring64_bitmap_add(r, 200000);

    assert_int_equal(roaring64_bitmap_get_cardinality(r), 5);

    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_range_cardinality) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, 100000);
    roaring64_bitmap_add(r, 100001);
    roaring64_bitmap_add(r, 100002);
    roaring64_bitmap_add(r, 200000);

    assert_int_equal(roaring64_bitmap_range_cardinality(r, 0, 0), 0);
    assert_int_equal(roaring64_bitmap_range_cardinality(r, 0, 100000), 1);
    assert_int_equal(roaring64_bitmap_range_cardinality(r, 1, 100001), 1);
    assert_int_equal(roaring64_bitmap_range_cardinality(r, 0, 200001), 5);

    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_is_empty) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    assert_true(roaring64_bitmap_is_empty(r));
    roaring64_bitmap_add(r, 1);
    assert_false(roaring64_bitmap_is_empty(r));
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_minimum) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    assert_int_equal(roaring64_bitmap_minimum(r), UINT64_MAX);

    roaring64_bitmap_add(r, (1ULL << 34) + 1);
    roaring64_bitmap_add(r, (1ULL << 35) + 1);
    roaring64_bitmap_add(r, (1ULL << 35) + 2);

    assert_int_equal(roaring64_bitmap_minimum(r), ((1ULL << 34) + 1));

    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_maximum) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    assert_int_equal(roaring64_bitmap_maximum(r), 0);

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, (1ULL << 34) + 1);
    roaring64_bitmap_add(r, (1ULL << 35) + 1);
    roaring64_bitmap_add(r, (1ULL << 35) + 2);

    assert_int_equal(roaring64_bitmap_maximum(r), ((1ULL << 35) + 2));

    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_run_optimize) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 20000);
    assert_false(roaring64_bitmap_run_optimize(r));

    for (uint64_t i = 0; i < 30000; ++i) {
        roaring64_bitmap_add(r, i);
    }
    assert_true(roaring64_bitmap_run_optimize(r));

    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_equals) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    assert_true(roaring64_bitmap_equals(r1, r2));

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100000);
    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    assert_false(roaring64_bitmap_equals(r1, r2));

    roaring64_bitmap_add(r1, 400000);
    roaring64_bitmap_remove(r1, 300000);

    assert_true(roaring64_bitmap_equals(r1, r2));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_is_subset) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    assert_true(roaring64_bitmap_is_subset(r1, r2));

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100000);
    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    assert_false(roaring64_bitmap_is_subset(r1, r2));
    assert_false(roaring64_bitmap_is_subset(r2, r1));

    roaring64_bitmap_remove(r1, 300000);

    assert_true(roaring64_bitmap_is_subset(r1, r2));
    assert_false(roaring64_bitmap_is_subset(r2, r1));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_is_strict_subset) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    assert_false(roaring64_bitmap_is_strict_subset(r1, r2));

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100000);
    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    assert_false(roaring64_bitmap_is_strict_subset(r1, r2));
    assert_false(roaring64_bitmap_is_strict_subset(r2, r1));

    roaring64_bitmap_remove(r1, 300000);

    assert_true(roaring64_bitmap_is_strict_subset(r1, r2));
    assert_false(roaring64_bitmap_is_strict_subset(r2, r1));

    roaring64_bitmap_add(r1, 400000);

    assert_false(roaring64_bitmap_is_strict_subset(r1, r2));
    assert_false(roaring64_bitmap_is_strict_subset(r2, r1));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_and) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    roaring64_bitmap_t* r3 = roaring64_bitmap_and(r1, r2);

    assert_false(roaring64_bitmap_contains(r3, 100000));
    assert_true(roaring64_bitmap_contains(r3, 100001));
    assert_true(roaring64_bitmap_contains(r3, 200000));
    assert_false(roaring64_bitmap_contains(r3, 300000));
    assert_false(roaring64_bitmap_contains(r3, 400000));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
    roaring64_bitmap_free(r3);
}

DEFINE_TEST(test_and_cardinality) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    assert_int_equal(roaring64_bitmap_and_cardinality(r1, r2), 2);

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_and_inplace) {
    {
        roaring64_bitmap_t* r1 = roaring64_bitmap_create();
        roaring64_bitmap_t* r2 = roaring64_bitmap_create();

        roaring64_bitmap_add(r1, 50000);
        roaring64_bitmap_add(r1, 100000);
        roaring64_bitmap_add(r1, 100001);
        roaring64_bitmap_add(r1, 200000);
        roaring64_bitmap_add(r1, 300000);

        roaring64_bitmap_add(r2, 100001);
        roaring64_bitmap_add(r2, 200000);
        roaring64_bitmap_add(r2, 400000);

        roaring64_bitmap_and_inplace(r1, r2);

        assert_false(roaring64_bitmap_contains(r1, 50000));
        assert_false(roaring64_bitmap_contains(r1, 100000));
        assert_true(roaring64_bitmap_contains(r1, 100001));
        assert_true(roaring64_bitmap_contains(r1, 200000));
        assert_false(roaring64_bitmap_contains(r1, 300000));
        assert_false(roaring64_bitmap_contains(r1, 400000));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
    {
        // No intersection.
        roaring64_bitmap_t* r1 = roaring64_bitmap_from_range(0, 100, 1);
        roaring64_bitmap_t* r2 = roaring64_bitmap_from_range(100, 200, 1);

        roaring64_bitmap_and_inplace(r1, r2);
        assert_true(roaring64_bitmap_is_empty(r1));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
}

DEFINE_TEST(test_intersect) {
    {
        roaring64_bitmap_t* r1 = roaring64_bitmap_create();
        roaring64_bitmap_t* r2 = roaring64_bitmap_create();

        roaring64_bitmap_add(r1, 50000);
        roaring64_bitmap_add(r1, 100000);
        roaring64_bitmap_add(r1, 100001);
        roaring64_bitmap_add(r1, 200000);
        roaring64_bitmap_add(r1, 300000);

        roaring64_bitmap_add(r1, 40000);
        roaring64_bitmap_add(r2, 100001);
        roaring64_bitmap_add(r1, 400000);

        assert_true(roaring64_bitmap_intersect(r1, r2));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
    {
        roaring64_bitmap_t* r1 = roaring64_bitmap_create();
        roaring64_bitmap_t* r2 = roaring64_bitmap_create();

        roaring64_bitmap_add(r1, 50000);
        roaring64_bitmap_add(r1, 100000);
        roaring64_bitmap_add(r1, 100001);
        roaring64_bitmap_add(r1, 200000);
        roaring64_bitmap_add(r1, 300000);

        roaring64_bitmap_add(r1, 40000);
        roaring64_bitmap_add(r1, 400000);

        assert_false(roaring64_bitmap_intersect(r1, r2));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
}

DEFINE_TEST(test_or) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    roaring64_bitmap_t* r3 = roaring64_bitmap_or(r1, r2);

    assert_true(roaring64_bitmap_contains(r3, 100000));
    assert_true(roaring64_bitmap_contains(r3, 100001));
    assert_true(roaring64_bitmap_contains(r3, 200000));
    assert_true(roaring64_bitmap_contains(r3, 300000));
    assert_true(roaring64_bitmap_contains(r3, 400000));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
    roaring64_bitmap_free(r3);
}

DEFINE_TEST(test_or_cardinality) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    assert_int_equal(roaring64_bitmap_or_cardinality(r1, r2), 5);

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_or_inplace) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    roaring64_bitmap_or_inplace(r1, r2);

    assert_true(roaring64_bitmap_contains(r1, 100000));
    assert_true(roaring64_bitmap_contains(r1, 100001));
    assert_true(roaring64_bitmap_contains(r1, 200000));
    assert_true(roaring64_bitmap_contains(r1, 300000));
    assert_true(roaring64_bitmap_contains(r1, 400000));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_xor) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    roaring64_bitmap_t* r3 = roaring64_bitmap_xor(r1, r2);

    assert_true(roaring64_bitmap_contains(r3, 100000));
    assert_false(roaring64_bitmap_contains(r3, 100001));
    assert_false(roaring64_bitmap_contains(r3, 200000));
    assert_true(roaring64_bitmap_contains(r3, 300000));
    assert_true(roaring64_bitmap_contains(r3, 400000));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
    roaring64_bitmap_free(r3);
}

DEFINE_TEST(test_xor_cardinality) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    assert_int_equal(roaring64_bitmap_xor_cardinality(r1, r2), 3);

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_xor_inplace) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    roaring64_bitmap_xor_inplace(r1, r2);

    assert_true(roaring64_bitmap_contains(r1, 100000));
    assert_false(roaring64_bitmap_contains(r1, 100001));
    assert_false(roaring64_bitmap_contains(r1, 200000));
    assert_true(roaring64_bitmap_contains(r1, 300000));
    assert_true(roaring64_bitmap_contains(r1, 400000));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_andnot) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    roaring64_bitmap_t* r3 = roaring64_bitmap_andnot(r1, r2);

    assert_true(roaring64_bitmap_contains(r3, 100000));
    assert_false(roaring64_bitmap_contains(r3, 100001));
    assert_false(roaring64_bitmap_contains(r3, 200000));
    assert_true(roaring64_bitmap_contains(r3, 300000));
    assert_false(roaring64_bitmap_contains(r3, 400000));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
    roaring64_bitmap_free(r3);
}

DEFINE_TEST(test_andnot_cardinality) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    roaring64_bitmap_t* r2 = roaring64_bitmap_create();

    roaring64_bitmap_add(r1, 100000);
    roaring64_bitmap_add(r1, 100001);
    roaring64_bitmap_add(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    roaring64_bitmap_add(r2, 100001);
    roaring64_bitmap_add(r2, 200000);
    roaring64_bitmap_add(r2, 400000);

    assert_int_equal(roaring64_bitmap_andnot_cardinality(r1, r2), 2);

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_andnot_inplace) {
    {
        roaring64_bitmap_t* r1 = roaring64_bitmap_create();
        roaring64_bitmap_t* r2 = roaring64_bitmap_create();

        roaring64_bitmap_add(r1, 100000);
        roaring64_bitmap_add(r1, 100001);
        roaring64_bitmap_add(r1, 200000);
        roaring64_bitmap_add(r1, 300000);

        roaring64_bitmap_add(r2, 100001);
        roaring64_bitmap_add(r2, 200000);
        roaring64_bitmap_add(r2, 400000);

        roaring64_bitmap_andnot_inplace(r1, r2);

        assert_true(roaring64_bitmap_contains(r1, 100000));
        assert_false(roaring64_bitmap_contains(r1, 100001));
        assert_false(roaring64_bitmap_contains(r1, 200000));
        assert_true(roaring64_bitmap_contains(r1, 300000));
        assert_false(roaring64_bitmap_contains(r1, 400000));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
    {
        // Two identical bitmaps.
        roaring64_bitmap_t* r1 = roaring64_bitmap_from_range(0, 100, 1);
        roaring64_bitmap_t* r2 = roaring64_bitmap_from_range(0, 100, 1);

        roaring64_bitmap_andnot_inplace(r1, r2);
        assert_true(roaring64_bitmap_is_empty(r1));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
}

bool roaring_iterator64_sumall(uint64_t value, void* param) {
    *(uint64_t*)param += value;
    return true;
}

DEFINE_TEST(test_iterate) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, 1ULL << 35);
    roaring64_bitmap_add(r, (1Ull << 35) + 1);
    roaring64_bitmap_add(r, (1Ull << 35) + 2);
    roaring64_bitmap_add(r, (1Ull << 36));

    uint64_t sum = 0;
    assert_true(roaring64_bitmap_iterate(r, roaring_iterator64_sumall, &sum));
    assert_int_equal(sum, ((1ULL << 35) + (1ULL << 35) + 1 + (1ULL << 35) + 2 +
                           (1ULL << 36)));

    roaring64_bitmap_free(r);
}

}  // namespace

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_copy),
        cmocka_unit_test(test_from_range),
        cmocka_unit_test(test_of_ptr),
        cmocka_unit_test(test_of),
        cmocka_unit_test(test_add),
        cmocka_unit_test(test_add_checked),
        cmocka_unit_test(test_add_bulk),
        cmocka_unit_test(test_add_many),
        cmocka_unit_test(test_add_range_closed),
        cmocka_unit_test(test_contains_bulk),
        cmocka_unit_test(test_select),
        cmocka_unit_test(test_rank),
        cmocka_unit_test(test_get_index),
        cmocka_unit_test(test_remove),
        cmocka_unit_test(test_remove_checked),
        cmocka_unit_test(test_remove_bulk),
        cmocka_unit_test(test_remove_many),
        cmocka_unit_test(test_remove_range_closed),
        cmocka_unit_test(test_get_cardinality),
        cmocka_unit_test(test_range_cardinality),
        cmocka_unit_test(test_is_empty),
        cmocka_unit_test(test_minimum),
        cmocka_unit_test(test_maximum),
        cmocka_unit_test(test_run_optimize),
        cmocka_unit_test(test_equals),
        cmocka_unit_test(test_is_subset),
        cmocka_unit_test(test_is_strict_subset),
        cmocka_unit_test(test_and),
        cmocka_unit_test(test_and_cardinality),
        cmocka_unit_test(test_and_inplace),
        cmocka_unit_test(test_intersect),
        cmocka_unit_test(test_or),
        cmocka_unit_test(test_or_cardinality),
        cmocka_unit_test(test_or_inplace),
        cmocka_unit_test(test_xor),
        cmocka_unit_test(test_xor_cardinality),
        cmocka_unit_test(test_xor_inplace),
        cmocka_unit_test(test_andnot),
        cmocka_unit_test(test_andnot_cardinality),
        cmocka_unit_test(test_andnot_inplace),
        cmocka_unit_test(test_iterate),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

