#include <array>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include <roaring/roaring64.h>

#include "test.h"

using namespace roaring::api;

namespace {

void assert_vector_equal(const std::vector<uint64_t>& lhs,
                         const std::vector<uint64_t>& rhs) {
    assert_int_equal(lhs.size(), rhs.size());
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            printf("Mismatch at %zu\n", i);
            assert_int_equal(lhs[i], rhs[i]);
        }
    }
}

void assert_r64_valid(roaring64_bitmap_t* b) {
    const char* reason = nullptr;
    if (!roaring64_bitmap_internal_validate(b, &reason)) {
        fail_msg("Roaring64 bitmap is invalid: '%s'\n", reason);
    }
}

DEFINE_TEST(test_copy) {
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    assert_r64_valid(r1);

    roaring64_bitmap_add(r1, 0);
    roaring64_bitmap_add(r1, 10000);
    roaring64_bitmap_add(r1, 200000);

    roaring64_bitmap_t* r2 = roaring64_bitmap_copy(r1);
    assert_r64_valid(r1);
    assert_true(roaring64_bitmap_contains(r2, 0));
    assert_true(roaring64_bitmap_contains(r2, 10000));
    assert_true(roaring64_bitmap_contains(r2, 200000));

    roaring64_bitmap_remove(r1, 200000);
    roaring64_bitmap_add(r1, 300000);

    assert_r64_valid(r1);
    assert_true(roaring64_bitmap_contains(r2, 200000));
    assert_false(roaring64_bitmap_contains(r2, 300000));

    roaring64_bitmap_free(r1);
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_from_range) {
    {
        // Step greater than 2 ^ 16.
        roaring64_bitmap_t* r = roaring64_bitmap_from_range(0, 1000000, 200000);
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
        assert_true(roaring64_bitmap_contains(r, (1 << 16) - 1));
        assert_true(roaring64_bitmap_contains(r, (1 << 16) + 0));
        assert_true(roaring64_bitmap_contains(r, (1 << 16) + 1));
        assert_true(roaring64_bitmap_contains(r, (1 << 17) - 1));
        assert_true(roaring64_bitmap_contains(r, (1 << 17) + 0));
        assert_true(roaring64_bitmap_contains(r, (1 << 17) + 1));
        assert_false(roaring64_bitmap_contains(r, (1 << 17) + 2));
        roaring64_bitmap_free(r);
    }
    {
        // Range extending into the max container
        roaring64_bitmap_t* r = roaring64_bitmap_from_range(
            UINT64_MAX - 0x10000 - 10, UINT64_MAX - 0x10000 + 10, 2);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 10);
        assert_int_equal(roaring64_bitmap_minimum(r),
                         UINT64_MAX - 0x10000 - 10);
        assert_int_equal(roaring64_bitmap_maximum(r), UINT64_MAX - 0x10000 + 8);
        roaring64_bitmap_free(r);
    }
    {
        // Range fully in the max container
        roaring64_bitmap_t* r =
            roaring64_bitmap_from_range(UINT64_MAX - 5, UINT64_MAX, 1);
        // From range is exclusive, so UINT64_MAX is not included
        assert_false(roaring64_bitmap_contains(r, UINT64_MAX));
        assert_int_equal(roaring64_bitmap_minimum(r), UINT64_MAX - 5);
        assert_int_equal(roaring64_bitmap_maximum(r), UINT64_MAX - 1);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 5);
        roaring64_bitmap_free(r);
    }
}

DEFINE_TEST(test_of_ptr) {
    std::array<uint64_t, 1000> vals;
    std::iota(vals.begin(), vals.end(), 0);
    roaring64_bitmap_t* r = roaring64_bitmap_of_ptr(vals.size(), vals.data());
    assert_r64_valid(r);
    for (uint64_t i = 0; i < 1000; ++i) {
        assert_true(roaring64_bitmap_contains(r, vals[i]));
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_of) {
    roaring64_bitmap_t* r = roaring64_bitmap_from(1, 20000, 500000);
    assert_r64_valid(r);
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

    assert_r64_valid(r);
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

    assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), end - start + 1);
        roaring64_bitmap_free(r);
    }
    {
        // Range extending into the max container
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, UINT64_MAX - 0x10000 - 10,
                                          UINT64_MAX - 0x10000 + 10);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 21);
        assert_int_equal(roaring64_bitmap_minimum(r),
                         UINT64_MAX - 0x10000 - 10);
        assert_int_equal(roaring64_bitmap_maximum(r),
                         UINT64_MAX - 0x10000 + 10);
        roaring64_bitmap_free(r);
    }
    {
        // Range fully inside max container
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, UINT64_MAX - 5, UINT64_MAX);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 6);
        assert_true(roaring64_bitmap_contains(r, UINT64_MAX - 5));
        assert_true(roaring64_bitmap_contains(r, UINT64_MAX - 4));
        assert_true(roaring64_bitmap_contains(r, UINT64_MAX - 3));
        assert_true(roaring64_bitmap_contains(r, UINT64_MAX - 2));
        assert_true(roaring64_bitmap_contains(r, UINT64_MAX - 1));
        assert_true(roaring64_bitmap_contains(r, UINT64_MAX));
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

DEFINE_TEST(test_contains_range) {
    {
        // Empty bitmap.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        assert_false(roaring64_bitmap_contains_range(r, 1, 10));
        roaring64_bitmap_free(r);
    }
    {
        // Empty range.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range(r, 1, 10);
        assert_true(roaring64_bitmap_contains_range(r, 1, 1));
        roaring64_bitmap_free(r);
    }
    {
        // Range within one container.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range(r, 1, 10);
        assert_true(roaring64_bitmap_contains_range(r, 1, 10));
        assert_false(roaring64_bitmap_contains_range(r, 1, 11));
        roaring64_bitmap_free(r);
    }
    {
        // Range across two containers.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range(r, 1, (1 << 16) + 10);
        assert_true(roaring64_bitmap_contains_range(r, 1, (1 << 16) + 10));
        assert_true(roaring64_bitmap_contains_range(r, 1, (1 << 16) - 1));
        assert_false(roaring64_bitmap_contains_range(r, 1, (1 << 16) + 11));
        assert_false(roaring64_bitmap_contains_range(r, 0, (1 << 16) + 10));
        roaring64_bitmap_free(r);
    }
    {
        // Range across three containers.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range(r, 1, (2 << 16) + 10);
        assert_true(roaring64_bitmap_contains_range(r, 1, (2 << 16) + 10));
        assert_false(roaring64_bitmap_contains_range(r, 1, (2 << 16) + 11));
        roaring64_bitmap_free(r);
    }
    {
        // Container missing from range.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range(r, 1, (1 << 16) - 1);
        roaring64_bitmap_add_range(r, (2 << 16), (3 << 16) - 1);
        assert_false(roaring64_bitmap_contains_range(r, 1, (3 << 16) - 1));
        roaring64_bitmap_free(r);
    }
    {
        // Range larger than bitmap.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range(r, 1, 1 << 16);
        assert_false(roaring64_bitmap_contains_range(r, 1, (1 << 16) + 1));
        roaring64_bitmap_free(r);
    }
    {
        // Range entirely before the bitmap.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add(r, 1 << 16);
        assert_false(roaring64_bitmap_contains_range(r, 1, 10));
        roaring64_bitmap_free(r);
    }
    {
        // Range entirely after the bitmap.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add(r, 1 << 16);
        assert_false(
            roaring64_bitmap_contains_range(r, 2 << 16, (2 << 16) + 1));
        roaring64_bitmap_free(r);
    }
    {
        // Range exactly containing the last value in a container range.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add(r, (1 << 16) - 1);
        assert_true(
            roaring64_bitmap_contains_range(r, (1 << 16) - 1, (1 << 16)));
        assert_false(
            roaring64_bitmap_contains_range(r, (1 << 16) - 1, (1 << 16) + 1));
        roaring64_bitmap_free(r);
    }
    {
        // Range exactly containing the first value in a container range.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add(r, (1 << 16));
        assert_true(
            roaring64_bitmap_contains_range(r, (1 << 16), (1 << 16) + 1));
        roaring64_bitmap_free(r);
    }
    {
        // Range extending into the max container
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        assert_false(roaring64_bitmap_contains_range(
            r, UINT64_MAX - 0x10000 - 10, UINT64_MAX - 0x10000 + 10));
        roaring64_bitmap_add_range(r, UINT64_MAX - 0x10000 - 10,
                                   UINT64_MAX - 0x10000 + 10);
        assert_true(roaring64_bitmap_contains_range(
            r, UINT64_MAX - 0x10000 - 10, UINT64_MAX - 0x10000 + 10));
        assert_false(roaring64_bitmap_contains_range(
            r, UINT64_MAX - 0x10000 - 10, UINT64_MAX));
        roaring64_bitmap_free(r);
    }
    {
        // Range fully inside max container
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        assert_false(
            roaring64_bitmap_contains_range(r, UINT64_MAX - 2, UINT64_MAX));
        roaring64_bitmap_add(r, UINT64_MAX - 1);
        assert_false(
            roaring64_bitmap_contains_range(r, UINT64_MAX - 2, UINT64_MAX));
        roaring64_bitmap_add(r, UINT64_MAX - 2);
        // contains_range is exclusive, so UINT64_MAX is not required
        assert_true(
            roaring64_bitmap_contains_range(r, UINT64_MAX - 2, UINT64_MAX));
        roaring64_bitmap_free(r);
    }
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
    assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
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
        assert_r64_valid(r);
        assert_true(roaring64_bitmap_is_empty(r));
        roaring64_bitmap_free(r);
    }
    {
        // Range extending into the max container
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, UINT64_MAX - 0x10000 - 10,
                                          UINT64_MAX - 0x10000 + 10);
        roaring64_bitmap_remove_range_closed(r, UINT64_MAX - 0x10000 - 5,
                                             UINT64_MAX - 0x10000 + 5);
        assert_false(roaring64_bitmap_intersect_with_range(
            r, UINT64_MAX - 0x10000 - 5, UINT64_MAX - 0x10000 + 6));
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 10);
        assert_int_equal(roaring64_bitmap_minimum(r),
                         UINT64_MAX - 0x10000 - 10);
        assert_int_equal(roaring64_bitmap_maximum(r),
                         UINT64_MAX - 0x10000 + 10);
        roaring64_bitmap_free(r);
    }
    {
        // Range fully inside max container
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        // It's fine to remove a range that isn't in the bitmap
        roaring64_bitmap_remove_range_closed(r, UINT64_MAX - 5, UINT64_MAX);

        roaring64_bitmap_add_range_closed(r, UINT64_MAX - 10, UINT64_MAX);
        roaring64_bitmap_remove_range_closed(r, UINT64_MAX - 5, UINT64_MAX);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 5);
        assert_int_equal(roaring64_bitmap_minimum(r), UINT64_MAX - 10);
        assert_int_equal(roaring64_bitmap_maximum(r), UINT64_MAX - 6);
        roaring64_bitmap_free(r);
    }
    {
        // Remove a huge range
        roaring64_bitmap_t* r = roaring64_bitmap_from(1, UINT64_MAX - 1);
        roaring64_bitmap_remove_range_closed(r, 0, UINT64_MAX);
        roaring64_bitmap_free(r);
    }
}

DEFINE_TEST(test_get_cardinality) {
    {
        roaring64_bitmap_t* r = roaring64_bitmap_create();

        roaring64_bitmap_add(r, 0);
        roaring64_bitmap_add(r, 100000);
        roaring64_bitmap_add(r, 100001);
        roaring64_bitmap_add(r, 100002);
        roaring64_bitmap_add(r, 200000);

        assert_int_equal(roaring64_bitmap_get_cardinality(r), 5);

        roaring64_bitmap_free(r);
    }
    {
        // Max depth ART.
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        for (int i = 0; i < 7; ++i) {
            roaring64_bitmap_add(r, 1ULL << (i * 8 + 8));
        }
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 7);
        roaring64_bitmap_free(r);
    }
}

DEFINE_TEST(test_range_cardinality) {
    {
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
    {
        // Range extending into the max container
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        roaring64_bitmap_add_range_closed(r, UINT64_MAX - 0x10000 - 10,
                                          UINT64_MAX - 0x10000 + 10);
        assert_int_equal(roaring64_bitmap_range_cardinality(
                             r, UINT64_MAX - 0x20000, UINT64_MAX),
                         21);
        assert_int_equal(
            roaring64_bitmap_range_cardinality(r, UINT64_MAX - 0x10000 - 10,
                                               UINT64_MAX - 0x1000 + 11),
            21);
        roaring64_bitmap_free(r);
    }
    {
        // Range fully inside max container
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        uint64_t start = UINT64_MAX - 1000;

        roaring64_bitmap_add(r, start + 0);
        roaring64_bitmap_add(r, start + 100);
        roaring64_bitmap_add(r, start + 101);
        roaring64_bitmap_add(r, start + 201);

        assert_int_equal(
            roaring64_bitmap_range_cardinality(r, start, start + 100), 1);
        assert_int_equal(
            roaring64_bitmap_range_cardinality(r, start, UINT64_MAX), 4);
        roaring64_bitmap_add(r, UINT64_MAX);
        // range is exclusive, so UINT64_MAX is not included
        assert_int_equal(
            roaring64_bitmap_range_cardinality(r, start, UINT64_MAX), 4);

        roaring64_bitmap_free(r);
    }
}

DEFINE_TEST(test_is_empty) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    assert_r64_valid(r);
    assert_true(roaring64_bitmap_is_empty(r));
    roaring64_bitmap_add(r, 1);
    assert_r64_valid(r);
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
    assert_r64_valid(r);

    for (uint64_t i = 0; i < 30000; ++i) {
        roaring64_bitmap_add(r, i);
    }
    assert_true(roaring64_bitmap_run_optimize(r));
    assert_r64_valid(r);

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

    assert_r64_valid(r3);
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

        assert_r64_valid(r1);
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
        assert_r64_valid(r1);
        assert_true(roaring64_bitmap_is_empty(r1));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
    {
        // In-place should be the same as not-in-place.
        uint64_t start = 0x0FFFF;
        uint64_t end = 0x20001;

        roaring64_bitmap_t* r1 = roaring64_bitmap_from_range(start, end, 1);
        roaring64_bitmap_add(r1, 0xFFFF0000);

        roaring64_bitmap_t* r2 = roaring64_bitmap_from_range(start, end, 1);

        uint64_t and_cardinality = roaring64_bitmap_and_cardinality(r1, r2);
        assert_int_equal(and_cardinality, end - start);

        roaring64_bitmap_t* r3 = roaring64_bitmap_and(r1, r2);
        assert_int_equal(roaring64_bitmap_get_cardinality(r3), and_cardinality);

        roaring64_bitmap_and_inplace(r1, r2);
        assert_int_equal(roaring64_bitmap_get_cardinality(r1), and_cardinality);
        assert_true(roaring64_bitmap_equals(r1, r3));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
        roaring64_bitmap_free(r3);
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

DEFINE_TEST(test_intersect_with_range) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 50000);
    roaring64_bitmap_add(r, 100000);
    roaring64_bitmap_add(r, 100001);
    roaring64_bitmap_add(r, 300000);

    assert_false(roaring64_bitmap_intersect_with_range(r, 0, 50000));
    assert_true(roaring64_bitmap_intersect_with_range(r, 0, 50001));
    assert_true(roaring64_bitmap_intersect_with_range(r, 50000, 50001));
    assert_false(roaring64_bitmap_intersect_with_range(r, 50001, 100000));
    assert_true(roaring64_bitmap_intersect_with_range(r, 50001, 100001));
    assert_false(roaring64_bitmap_intersect_with_range(r, 300001, UINT64_MAX));

    roaring64_bitmap_free(r);
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

    assert_r64_valid(r3);
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

    assert_r64_valid(r1);
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

    assert_r64_valid(r3);
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

    assert_r64_valid(r1);
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

    assert_r64_valid(r3);
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

        assert_r64_valid(r1);
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
        assert_r64_valid(r1);
        assert_true(roaring64_bitmap_is_empty(r1));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
}

DEFINE_TEST(test_flip) {
    {
        // Flipping an empty bitmap should result in a non-empty range.
        roaring64_bitmap_t* r1 = roaring64_bitmap_create();
        roaring64_bitmap_t* r2 = roaring64_bitmap_flip(r1, 10, 100000);
        assert_r64_valid(r2);
        assert_true(roaring64_bitmap_contains_range(r2, 10, 100000));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
    {
        // Only the specified range should be flipped.
        roaring64_bitmap_t* r1 = roaring64_bitmap_from(1, 3, 6);
        roaring64_bitmap_t* r2 = roaring64_bitmap_flip(r1, 2, 5);
        assert_r64_valid(r2);
        roaring64_bitmap_t* r3 = roaring64_bitmap_from(1, 2, 4, 6);
        assert_true(roaring64_bitmap_equals(r2, r3));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
        roaring64_bitmap_free(r3);
    }
    {
        // An empty range does nothing.
        roaring64_bitmap_t* r1 = roaring64_bitmap_from(1, 3, 6);
        roaring64_bitmap_t* r2 = roaring64_bitmap_flip(r1, 3, 3);
        assert_r64_valid(r2);
        assert_true(roaring64_bitmap_equals(r2, r1));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
    {
        // A bitmap with values in all affected containers.
        roaring64_bitmap_t* r1 =
            roaring64_bitmap_from((2 << 16), (3 << 16) + 1, (4 << 16) + 3);
        roaring64_bitmap_t* r2 =
            roaring64_bitmap_flip(r1, (2 << 16), (4 << 16) + 4);
        roaring64_bitmap_t* r3 =
            roaring64_bitmap_from_range((2 << 16) + 1, (4 << 16) + 3, 1);
        roaring64_bitmap_remove(r3, (3 << 16) + 1);
        assert_r64_valid(r2);
        assert_r64_valid(r3);
        assert_true(roaring64_bitmap_equals(r2, r3));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
        roaring64_bitmap_free(r3);
    }
}

DEFINE_TEST(test_flip_inplace) {
    {
        // Flipping an empty bitmap should result in a non-empty range.
        roaring64_bitmap_t* r1 = roaring64_bitmap_create();
        roaring64_bitmap_flip_inplace(r1, 10, 100000);
        assert_r64_valid(r1);
        assert_true(roaring64_bitmap_contains_range(r1, 10, 100000));

        roaring64_bitmap_free(r1);
    }
    {
        // Only the specified range should be flipped.
        roaring64_bitmap_t* r1 = roaring64_bitmap_from(1, 3, 6);
        roaring64_bitmap_flip_inplace(r1, 2, 5);
        roaring64_bitmap_t* r2 = roaring64_bitmap_from(1, 2, 4, 6);
        assert_r64_valid(r1);
        assert_true(roaring64_bitmap_equals(r1, r2));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
    {
        // An empty range does nothing.
        roaring64_bitmap_t* r1 = roaring64_bitmap_from(1, 3, 6);
        roaring64_bitmap_flip_inplace(r1, 3, 3);
        roaring64_bitmap_t* r2 = roaring64_bitmap_from(1, 3, 6);
        assert_r64_valid(r1);
        assert_true(roaring64_bitmap_equals(r1, r2));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
    {
        // A bitmap with values in all affected containers.
        roaring64_bitmap_t* r1 =
            roaring64_bitmap_from((2 << 16), (3 << 16) + 1, (4 << 16) + 3);
        roaring64_bitmap_flip_inplace(r1, (2 << 16), (4 << 16) + 4);
        roaring64_bitmap_t* r2 =
            roaring64_bitmap_from_range((2 << 16) + 1, (4 << 16) + 3, 1);
        roaring64_bitmap_remove(r2, (3 << 16) + 1);
        assert_r64_valid(r1);
        assert_true(roaring64_bitmap_equals(r1, r2));

        roaring64_bitmap_free(r1);
        roaring64_bitmap_free(r2);
    }
}

void check_portable_serialization(const roaring64_bitmap_t* r1) {
    size_t serialized_size = roaring64_bitmap_portable_size_in_bytes(r1);
    std::vector<char> buf(serialized_size, 0);
    size_t serialized = roaring64_bitmap_portable_serialize(r1, buf.data());
    assert_int_equal(serialized, serialized_size);
    size_t deserialized_size =
        roaring64_bitmap_portable_deserialize_size(buf.data(), SIZE_MAX);
    assert_int_equal(deserialized_size, serialized_size);
    roaring64_bitmap_t* r2 =
        roaring64_bitmap_portable_deserialize_safe(buf.data(), serialized_size);
    assert_r64_valid(r2);
    assert_true(roaring64_bitmap_equals(r2, r1));
    roaring64_bitmap_free(r2);
}

DEFINE_TEST(test_portable_serialize) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    check_portable_serialization(r);

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, 1);
    roaring64_bitmap_add(r, 1ULL << 16);
    roaring64_bitmap_add(r, 1ULL << 32);
    roaring64_bitmap_add(r, 1ULL << 48);
    roaring64_bitmap_add(r, 1ULL << 60);
    roaring64_bitmap_add(r, UINT64_MAX);
    check_portable_serialization(r);

    roaring64_bitmap_add_range(r, 1ULL << 16, 1ULL << 32);
    check_portable_serialization(r);

    roaring64_bitmap_free(r);
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

DEFINE_TEST(test_to_uint64_array) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    std::vector<uint64_t> a1 = {0, 1ULL << 35, (1Ull << 35) + 1,
                                (1Ull << 35) + 2, 1Ull << 36};
    for (uint64_t val : a1) {
        roaring64_bitmap_add(r, val);
    }

    std::vector<uint64_t> a2(a1.size(), 0);
    roaring64_bitmap_to_uint64_array(r, a2.data());
    assert_vector_equal(a2, a1);

    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_iterator_create) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    {
        roaring64_iterator_t* it = roaring64_iterator_create(r);
        assert_false(roaring64_iterator_has_value(it));
        roaring64_iterator_free(it);
    }
    roaring64_bitmap_add(r, 0);
    {
        roaring64_iterator_t* it = roaring64_iterator_create(r);
        assert_true(roaring64_iterator_has_value(it));
        assert_int_equal(roaring64_iterator_value(it), 0);
        roaring64_iterator_free(it);
    }
    roaring64_bitmap_add(r, (1ULL << 40) + 1234);
    {
        roaring64_iterator_t* it = roaring64_iterator_create(r);
        assert_true(roaring64_iterator_has_value(it));
        assert_int_equal(roaring64_iterator_value(it), 0);
        roaring64_iterator_free(it);
    }
    roaring64_bitmap_remove(r, 0);
    {
        roaring64_iterator_t* it = roaring64_iterator_create(r);
        assert_true(roaring64_iterator_has_value(it));
        assert_int_equal(roaring64_iterator_value(it), ((1ULL << 40) + 1234));
        roaring64_iterator_free(it);
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_iterator_create_last) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    {
        roaring64_iterator_t* it = roaring64_iterator_create_last(r);
        assert_false(roaring64_iterator_has_value(it));
        roaring64_iterator_free(it);
    }
    roaring64_bitmap_add(r, 0);
    {
        roaring64_iterator_t* it = roaring64_iterator_create_last(r);
        assert_true(roaring64_iterator_has_value(it));
        assert_int_equal(roaring64_iterator_value(it), 0);
        roaring64_iterator_free(it);
    }
    roaring64_bitmap_add(r, (1ULL << 40) + 1234);
    {
        roaring64_iterator_t* it = roaring64_iterator_create_last(r);
        assert_true(roaring64_iterator_has_value(it));
        assert_int_equal(roaring64_iterator_value(it), ((1ULL << 40) + 1234));
        roaring64_iterator_free(it);
    }
    roaring64_bitmap_remove(r, 0);
    {
        roaring64_iterator_t* it = roaring64_iterator_create_last(r);
        assert_true(roaring64_iterator_has_value(it));
        assert_int_equal(roaring64_iterator_value(it), ((1ULL << 40) + 1234));
        roaring64_iterator_free(it);
    }
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_iterator_reinit) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, 1ULL << 35);
    roaring64_bitmap_add(r, (1ULL << 35) + 1);
    roaring64_bitmap_add(r, (1ULL << 35) + 2);
    roaring64_bitmap_add(r, (1ULL << 36));

    roaring64_iterator_t* it = roaring64_iterator_create(r);
    assert_true(roaring64_iterator_advance(it));
    assert_true(roaring64_iterator_advance(it));
    assert_true(roaring64_iterator_advance(it));
    assert_true(roaring64_iterator_previous(it));
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), ((1ULL << 35) + 1));

    roaring64_iterator_reinit(r, it);
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), 0);

    roaring64_iterator_free(it);
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_iterator_reinit_last) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, 1ULL << 35);
    roaring64_bitmap_add(r, (1ULL << 35) + 1);
    roaring64_bitmap_add(r, (1ULL << 35) + 2);
    roaring64_bitmap_add(r, (1ULL << 36));

    roaring64_iterator_t* it = roaring64_iterator_create(r);
    assert_true(roaring64_iterator_advance(it));
    assert_true(roaring64_iterator_advance(it));
    assert_true(roaring64_iterator_advance(it));
    assert_true(roaring64_iterator_previous(it));
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), ((1ULL << 35) + 1));

    roaring64_iterator_reinit_last(r, it);
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), (1ULL << 36));

    roaring64_iterator_free(it);
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_iterator_copy) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, 1ULL << 35);
    roaring64_bitmap_add(r, (1ULL << 35) + 1);
    roaring64_bitmap_add(r, (1ULL << 35) + 2);
    roaring64_bitmap_add(r, (1ULL << 36));

    roaring64_iterator_t* it1 = roaring64_iterator_create(r);
    assert_true(roaring64_iterator_advance(it1));
    assert_true(roaring64_iterator_advance(it1));
    assert_true(roaring64_iterator_advance(it1));
    assert_true(roaring64_iterator_previous(it1));
    assert_true(roaring64_iterator_has_value(it1));
    assert_int_equal(roaring64_iterator_value(it1), ((1ULL << 35) + 1));

    roaring64_iterator_t* it2 = roaring64_iterator_copy(it1);
    assert_true(roaring64_iterator_has_value(it2));
    assert_int_equal(roaring64_iterator_value(it2), ((1ULL << 35) + 1));
    assert_true(roaring64_iterator_advance(it2));
    assert_true(roaring64_iterator_has_value(it2));
    assert_int_equal(roaring64_iterator_value(it2), ((1ULL << 35) + 2));

    roaring64_iterator_free(it1);
    roaring64_iterator_free(it2);
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_iterator_advance) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    std::vector<uint64_t> values;
    values.reserve(1000);
    roaring64_bulk_context_t context{};
    for (uint64_t i = 0; i < 1000; ++i) {
        uint64_t v = i * 10000;
        values.push_back(v);
        roaring64_bitmap_add_bulk(r, &context, v);
    }
    size_t i = 0;
    roaring64_iterator_t* it = roaring64_iterator_create(r);
    do {
        assert_int_equal(roaring64_iterator_value(it), values[i]);
        i++;
    } while (roaring64_iterator_advance(it));
    assert_int_equal(i, values.size());

    // Check that we can move backward from after the last entry.
    assert_true(roaring64_iterator_previous(it));
    i--;
    assert_int_equal(roaring64_iterator_value(it), values[i]);

    // Check that we can't move forward again.
    assert_false(roaring64_iterator_advance(it));

    roaring64_iterator_free(it);
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_iterator_previous) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    std::vector<uint64_t> values;
    values.reserve(1000);
    roaring64_bulk_context_t context{};
    for (uint64_t i = 0; i < 1000; ++i) {
        uint64_t v = i * 10000;
        values.push_back(v);
        roaring64_bitmap_add_bulk(r, &context, v);
    }
    int i = ((int)values.size()) - 1;
    roaring64_iterator_t* it = roaring64_iterator_create_last(r);
    do {
        assert_int_equal(roaring64_iterator_value(it), values[i]);
        i--;
    } while (roaring64_iterator_previous(it));
    assert_int_equal(i, -1);

    // Check that we can move forward from before the first entry.
    assert_true(roaring64_iterator_advance(it));
    i++;
    assert_int_equal(roaring64_iterator_value(it), values[i]);

    // Check that we can't move backward again.
    assert_false(roaring64_iterator_previous(it));

    roaring64_iterator_free(it);
    roaring64_bitmap_free(r);
}

DEFINE_TEST(test_iterator_move_equalorlarger) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();

    roaring64_bitmap_add(r, 0);
    roaring64_bitmap_add(r, 1ULL << 35);
    roaring64_bitmap_add(r, (1ULL << 35) + 1);
    roaring64_bitmap_add(r, (1ULL << 35) + 2);
    roaring64_bitmap_add(r, (1ULL << 36));

    roaring64_iterator_t* it = roaring64_iterator_create(r);
    assert_true(roaring64_iterator_move_equalorlarger(it, 0));
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), 0);

    assert_true(roaring64_iterator_move_equalorlarger(it, 0));
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), 0);

    assert_true(roaring64_iterator_move_equalorlarger(it, 1));
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), (1ULL << 35));

    roaring64_iterator_reinit(r, it);

    assert_true(roaring64_iterator_move_equalorlarger(it, (1ULL << 35) + 2));
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), ((1ULL << 35) + 2));

    assert_true(roaring64_iterator_move_equalorlarger(it, (1ULL << 35) + 3));
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), (1ULL << 36));

    assert_false(roaring64_iterator_move_equalorlarger(it, (1ULL << 36) + 1));
    assert_false(roaring64_iterator_has_value(it));

    // Check that we can move backward from after the last entry.
    assert_true(roaring64_iterator_previous(it));
    assert_int_equal(roaring64_iterator_value(it), (1ULL << 36));

    // Check that we can move backward using move_equalorlarger.
    assert_true(roaring64_iterator_move_equalorlarger(it, (1ULL << 35) - 1));
    assert_true(roaring64_iterator_has_value(it));
    assert_int_equal(roaring64_iterator_value(it), (1ULL << 35));

    roaring64_iterator_free(it);
    roaring64_bitmap_free(r);
}

// Reads all elements from the iterator, `step` values at a time, and compares
// the elements with `values`.
void readCompare(const std::vector<uint64_t>& values,
                 const roaring64_bitmap_t* r, uint64_t step) {
    roaring64_iterator_t* it = roaring64_iterator_create(r);
    std::vector<uint64_t> buffer(values.size(), 0);
    uint64_t read = 0;
    while (read < values.size()) {
        assert_true(roaring64_iterator_has_value(it));
        uint64_t step_read = roaring64_iterator_read(it, buffer.data(), step);
        assert_int_equal(step_read, std::min(step, values.size() - read));
        for (size_t i = 0; i < step_read; ++i) {
            assert_int_equal(values[read + i], buffer[i]);
        }
        read += step_read;
    }
    assert_false(roaring64_iterator_has_value(it));
    roaring64_iterator_free(it);
}

DEFINE_TEST(test_iterator_read) {
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    std::vector<uint64_t> values;
    values.reserve(1000);
    roaring64_bulk_context_t context{};
    for (uint64_t i = 0; i < 1000; ++i) {
        uint64_t v = i * 10000;
        values.push_back(v);
        roaring64_bitmap_add_bulk(r, &context, v);
    }

    {
        // Check that a zero count results in zero elements read.
        roaring64_iterator_t* it = roaring64_iterator_create(r);
        uint64_t buf[1];
        assert_int_equal(roaring64_iterator_read(it, buf, 0), 0);
        roaring64_iterator_free(it);
    }

    readCompare(values, r, 1);
    readCompare(values, r, 2);
    readCompare(values, r, values.size() - 1);
    readCompare(values, r, values.size());
    readCompare(values, r, values.size() + 1);

    {
        // A count of UINT64_MAX.
        roaring64_iterator_t* it = roaring64_iterator_create(r);
        std::vector<uint64_t> buf(values.size(), 0);
        assert_int_equal(roaring64_iterator_read(it, buf.data(), UINT64_MAX),
                         1000);
        assert_vector_equal(buf, values);
        roaring64_iterator_free(it);
    }
    {
        // A count that becomes zero if cast to uint32.
        roaring64_iterator_t* it = roaring64_iterator_create(r);
        std::vector<uint64_t> buf(values.size(), 0);
        assert_int_equal(
            roaring64_iterator_read(it, buf.data(), 0xFFFFFFFF00000000), 1000);
        assert_vector_equal(buf, values);
        roaring64_iterator_free(it);
    }

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
        cmocka_unit_test(test_contains_range),
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
        cmocka_unit_test(test_intersect_with_range),
        cmocka_unit_test(test_or),
        cmocka_unit_test(test_or_cardinality),
        cmocka_unit_test(test_or_inplace),
        cmocka_unit_test(test_xor),
        cmocka_unit_test(test_xor_cardinality),
        cmocka_unit_test(test_xor_inplace),
        cmocka_unit_test(test_andnot),
        cmocka_unit_test(test_andnot_cardinality),
        cmocka_unit_test(test_andnot_inplace),
        cmocka_unit_test(test_flip),
        cmocka_unit_test(test_flip_inplace),
        cmocka_unit_test(test_portable_serialize),
        cmocka_unit_test(test_iterate),
        cmocka_unit_test(test_to_uint64_array),
        cmocka_unit_test(test_iterator_create),
        cmocka_unit_test(test_iterator_create_last),
        cmocka_unit_test(test_iterator_reinit),
        cmocka_unit_test(test_iterator_reinit_last),
        cmocka_unit_test(test_iterator_copy),
        cmocka_unit_test(test_iterator_advance),
        cmocka_unit_test(test_iterator_previous),
        cmocka_unit_test(test_iterator_move_equalorlarger),
        cmocka_unit_test(test_iterator_read),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
