#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include <roaring/portability.h>
#include <roaring/roaring64.h>

#include "config.h"
#include "test.h"

using namespace roaring::api;

namespace {

// Returns true if deserialization is successful.
bool test_serialization(const std::string& filename) {
    std::ifstream in(TEST_DATA_DIR + filename, std::ios::binary);
    std::vector<char> buf1(std::istreambuf_iterator<char>(in), {});

    const char* reason = nullptr;
    // Deserialize.
    size_t deserialized_size =
        roaring64_bitmap_portable_deserialize_size(buf1.data(), buf1.size());
    if (deserialized_size != 0) {
        assert_int_equal(deserialized_size, buf1.size());
    }
    roaring64_bitmap_t* r = roaring64_bitmap_portable_deserialize_safe(
        buf1.data(), deserialized_size);
    if (r == nullptr) {
        return false;
    }
    if (!roaring64_bitmap_internal_validate(r, &reason)) {
        fail_msg("Validation failed: %s", reason);
    }
    roaring64_bitmap_t* rf = roaring64_bitmap_portable_deserialize_frozen(
        buf1.data(), deserialized_size);
#if CROARING_IS_BIG_ENDIAN
    // No in-place view of a little-endian format on a big-endian host.
    if (rf != nullptr) {
        fail_msg("portable frozen view should be unavailable on big-endian");
    }
#else
    if (rf == nullptr) {
        fail_msg("portable frozen view failed on %s", filename.c_str());
    }
    if (!roaring64_bitmap_internal_validate(rf, &reason)) {
        fail_msg("Frozen validation failed: %s", reason);
    }
    if (!roaring64_bitmap_equals(r, rf)) {
        fail_msg("portable frozen view disagrees with copy deserialize");
    }
    roaring64_bitmap_free(rf);
#endif

    // Reserialize.
    size_t serialized_size = roaring64_bitmap_portable_size_in_bytes(r);
    assert_int_equal(serialized_size, deserialized_size);
    std::vector<char> buf2(serialized_size, 0);
    size_t serialized = roaring64_bitmap_portable_serialize(r, buf2.data());
    assert_int_equal(serialized, serialized_size);

    // Check that serialized buffers are the same.
    assert_int_equal(deserialized_size, serialized_size);
    assert_true(memcmp(buf1.data(), buf2.data(), deserialized_size) == 0);

    roaring64_bitmap_free(r);
    return true;
}

DEFINE_TEST(test_64map32bitvals) {
    assert_true(test_serialization("64map32bitvals.bin"));
}

DEFINE_TEST(test_64mapempty) {
    assert_true(test_serialization("64mapempty.bin"));
}

DEFINE_TEST(test_64mapemptyinput) {
    assert_false(test_serialization("64mapemptyinput.bin"));
}

DEFINE_TEST(test_64maphighvals) {
    assert_true(test_serialization("64maphighvals.bin"));
}

DEFINE_TEST(test_64mapinvalidsize) {
    assert_false(test_serialization("64mapinvalidsize.bin"));
}

DEFINE_TEST(test_64mapkeytoosmall) {
    assert_false(test_serialization("64mapkeytoosmall.bin"));
}

DEFINE_TEST(test_64mapsizetoosmall) {
    assert_false(test_serialization("64mapsizetoosmall.bin"));
}

DEFINE_TEST(test_64mapspreadvals) {
    assert_true(test_serialization("64mapspreadvals.bin"));
}

DEFINE_TEST(test_64deseroverlappingkeys) {
    // clang-format off
    char simple_bitmap[] = {
        // Number of 32 bit bitmaps
        1, 0, 0, 0, 0, 0, 0, 0,
        // Top 32 bits of the first bitmap
        0, 0, 0, 0,
        // Serial Cookie
        0x3B, 0x30,
        // Container count - 1
        1, 0,
        // Run Flag Bitset (no runs)
        0,
        // Upper 16 bits of the first container
        0, 0,
        // Cardinality - 1 of the first container
        0, 0,
        // Upper 16 bits of the second container - DUPLICATE
        0, 0,
        // Cardinality - 1 of the second container
        0, 0,
        // Only value of first container
        0, 0,
        // Only value of second container
        0, 0,
    };
    // clang-format on

    roaring64_bitmap_t* r = roaring64_bitmap_portable_deserialize_safe(
        simple_bitmap, sizeof(simple_bitmap));
    const char* reason = nullptr;
    if (r != nullptr) {
        if (roaring64_bitmap_internal_validate(r, &reason)) {
            fail_msg(
                "Validation must fail if a bitmap was returned, duplicate keys "
                "are not allowed.");
        }
        roaring64_bitmap_free(r);
    }
}

DEFINE_TEST(test_64deseroverlappingupper32) {
    // Two bitmaps, with a single array container each, at the same key (0).
    // clang-format off
    char simple_bitmap[] = {
        // Number of 32 bit bitmaps
        2, 0, 0, 0, 0, 0, 0, 0,
        // Top 32 bits of the first bitmap
        0,  0,  0, 0,
        // Serial Cookie
        0x3B, 0x30,
        // Container count - 1
        0, 0,
        // Run Flag Bitset (not a run)
        0,
        // Upper 16 bits of the only container
        0, 0,
        // Cardinality - 1 of the only container
        0, 0,
        // Only value of first container
        0, 0,
        // Top 32 bits of the second bitmap
        0, 0, 0, 0,
        // Serial Cookie
        0x3B, 0x30,
        // Container count - 1
        0, 0,
        // Run Flag Bitset (not a run)
        0,
        // Upper 16 bits of the only container
        0, 0,
        // Cardinality - 1 of the only container
        0, 0,
        // Only value of only container
        0, 0,
    };
    // clang-format on

    roaring64_bitmap_t* r = roaring64_bitmap_portable_deserialize_safe(
        simple_bitmap, sizeof(simple_bitmap));
    const char* reason = nullptr;
    if (r != nullptr) {
        if (roaring64_bitmap_internal_validate(r, &reason)) {
            fail_msg(
                "Validation must fail if a bitmap was returned, duplicate keys "
                "are not allowed.");
        }
        roaring64_bitmap_free(r);
    }
}

std::vector<char> portable_bytes(const roaring64_bitmap_t* r) {
    std::vector<char> buf(roaring64_bitmap_portable_size_in_bytes(r));
    size_t written = roaring64_bitmap_portable_serialize(r, buf.data());
    assert_int_equal(written, buf.size());
    return buf;
}

void assert_valid64(const roaring64_bitmap_t* r) {
    const char* reason = nullptr;
    if (!roaring64_bitmap_internal_validate(r, &reason)) {
        fail_msg("Validation failed: %s", reason);
    }
}

// Every strict prefix of a valid serialized bitmap must be rejected.
DEFINE_TEST(test_64truncated_every_prefix) {
    roaring64_bitmap_t* source =
        roaring64_bitmap_from(1, UINT64_C(1) << 32, UINT64_MAX);
    roaring64_bitmap_add_range(source, UINT64_C(1) << 40,
                               (UINT64_C(1) << 40) + 5000);
    roaring64_bitmap_run_optimize(source);
    std::vector<char> buf = portable_bytes(source);
    assert_int_equal(
        roaring64_bitmap_portable_deserialize_size(buf.data(), buf.size()),
        buf.size());
    for (size_t k = 0; k < buf.size(); k++) {
        // Copy the prefix so that ASan catches any over-read.
        std::vector<char> prefix(buf.begin(), buf.begin() + k);
        assert_int_equal(
            roaring64_bitmap_portable_deserialize_size(prefix.data(), k), 0);
        roaring64_bitmap_t* r =
            roaring64_bitmap_portable_deserialize_safe(prefix.data(), k);
        if (r != nullptr) {
            fail_msg("prefix of length %zu was accepted", k);
        }
        roaring64_bitmap_t* rf =
            roaring64_bitmap_portable_deserialize_frozen(prefix.data(), k);
        if (rf != nullptr) {
            fail_msg("frozen prefix of length %zu was accepted", k);
        }
    }
    roaring64_bitmap_t* r =
        roaring64_bitmap_portable_deserialize_safe(buf.data(), buf.size());
    assert_non_null(r);
    assert_true(roaring64_bitmap_equals(r, source));
    roaring64_bitmap_free(r);
    roaring64_bitmap_free(source);
}

// A run ending exactly at 65535 is valid; one ending at 65536 is not.
DEFINE_TEST(test_64run_past_container_end) {
    roaring64_bitmap_t* source = roaring64_bitmap_create();
    roaring64_bitmap_add_range(source, 65000, 65536);
    assert_true(roaring64_bitmap_run_optimize(source));
    std::vector<char> buf = portable_bytes(source);
    // The last two bytes hold the length-1 of the only run (little endian).
    size_t n = buf.size();
    assert_int_equal((uint8_t)buf[n - 2] | ((uint8_t)buf[n - 1] << 8), 535);
    roaring64_bitmap_t* r =
        roaring64_bitmap_portable_deserialize_safe(buf.data(), n);
    assert_non_null(r);
    assert_valid64(r);
    assert_int_equal(roaring64_bitmap_get_cardinality(r), 536);
    assert_true(roaring64_bitmap_contains(r, 65535));
    roaring64_bitmap_free(r);

    buf[n - 2] = (char)(536 & 0xFF);
    buf[n - 1] = (char)(536 >> 8);
    r = roaring64_bitmap_portable_deserialize_safe(buf.data(), n);
    if (r != nullptr) {
        const char* reason = nullptr;
        if (roaring64_bitmap_internal_validate(r, &reason)) {
            fail_msg("a run past the end of the container must be rejected");
        }
        roaring64_bitmap_free(r);
    }
    roaring64_bitmap_free(source);
}

// Bucket keys must be strictly increasing.
DEFINE_TEST(test_64deserdecreasingkeys) {
    // clang-format off
    char data[] = {
        // Number of 32 bit bitmaps
        2, 0, 0, 0, 0, 0, 0, 0,
        // Top 32 bits of the first bitmap: 2
        2, 0, 0, 0,
        // No-run cookie, 1 container
        0x3A, 0x30, 0, 0, 1, 0, 0, 0,
        // key 0, cardinality-1 0, offset 16
        0, 0, 0, 0, 16, 0, 0, 0,
        // value 3
        3, 0,
        // Top 32 bits of the second bitmap: 1 (decreasing)
        1, 0, 0, 0,
        0x3A, 0x30, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 16, 0, 0, 0,
        // value 4
        4, 0,
    };
    // clang-format on
    roaring64_bitmap_t* r =
        roaring64_bitmap_portable_deserialize_safe(data, sizeof(data));
    assert_null(r);
    // Sanity check: with increasing keys the same buffer is accepted.
    data[8] = 1;
    data[8 + 4 + 18] = 2;
    r = roaring64_bitmap_portable_deserialize_safe(data, sizeof(data));
    assert_non_null(r);
    assert_valid64(r);
    assert_int_equal(roaring64_bitmap_get_cardinality(r), 2);
    assert_true(roaring64_bitmap_contains(r, (UINT64_C(1) << 32) + 3));
    assert_true(roaring64_bitmap_contains(r, (UINT64_C(2) << 32) + 4));
    roaring64_bitmap_free(r);
}

// An empty 32-bit bucket is accepted and dropped.
DEFINE_TEST(test_64deseremptybucket) {
    // clang-format off
    const char data[] = {
        // Number of 32 bit bitmaps
        2, 0, 0, 0, 0, 0, 0, 0,
        // Top 32 bits: 0, then an empty bitmap
        0, 0, 0, 0,
        0x3A, 0x30, 0, 0, 0, 0, 0, 0,
        // Top 32 bits: 1, then {7}
        1, 0, 0, 0,
        0x3A, 0x30, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 16, 0, 0, 0,
        7, 0,
    };
    // clang-format on
    assert_int_equal(
        roaring64_bitmap_portable_deserialize_size(data, sizeof(data)),
        sizeof(data));
    roaring64_bitmap_t* r =
        roaring64_bitmap_portable_deserialize_safe(data, sizeof(data));
    assert_non_null(r);
    assert_valid64(r);
    assert_int_equal(roaring64_bitmap_get_cardinality(r), 1);
    assert_true(roaring64_bitmap_contains(r, (UINT64_C(1) << 32) + 7));
    std::vector<char> again = portable_bytes(r);
    // The empty bucket (4 byte key + 8 byte bitmap) is not re-serialized.
    assert_int_equal(again.size(), sizeof(data) - 12);
    assert_int_equal(again[0], 1);
    roaring64_bitmap_free(r);
}

// Round trip after a remove converts a bitset container to an array.
DEFINE_TEST(test_64roundtrip_after_remove_changes_type) {
    for (uint64_t base : {UINT64_C(0), UINT64_C(1) << 40, UINT64_C(1) << 63}) {
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        for (uint64_t i = 0; i < 4097; i++) {
            roaring64_bitmap_add(r, base + 2 * i);
        }
        roaring64_bitmap_remove(r, base);
        assert_int_equal(roaring64_bitmap_get_cardinality(r), 4096);
        std::vector<char> buf = portable_bytes(r);
        roaring64_bitmap_t* back =
            roaring64_bitmap_portable_deserialize_safe(buf.data(), buf.size());
        assert_non_null(back);
        assert_valid64(back);
        assert_true(roaring64_bitmap_equals(r, back));
        assert_false(roaring64_bitmap_contains(back, base));
        assert_true(roaring64_bitmap_contains(back, base + 2));
        roaring64_bitmap_free(back);
        roaring64_bitmap_free(r);
    }
}
}  // namespace

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_64map32bitvals),
        cmocka_unit_test(test_64mapempty),
        cmocka_unit_test(test_64mapemptyinput),
        cmocka_unit_test(test_64maphighvals),
        cmocka_unit_test(test_64mapinvalidsize),
        cmocka_unit_test(test_64mapkeytoosmall),
        cmocka_unit_test(test_64mapsizetoosmall),
        cmocka_unit_test(test_64mapspreadvals),
        cmocka_unit_test(test_64deseroverlappingkeys),
        cmocka_unit_test(test_64deseroverlappingupper32),
        cmocka_unit_test(test_64truncated_every_prefix),
        cmocka_unit_test(test_64run_past_container_end),
        cmocka_unit_test(test_64deserdecreasingkeys),
        cmocka_unit_test(test_64deseremptybucket),
        cmocka_unit_test(test_64roundtrip_after_remove_changes_type),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
