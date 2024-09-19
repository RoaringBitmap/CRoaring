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
        0, 0,
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
}  // namespace

int main() {
#if CROARING_IS_BIG_ENDIAN
    printf("Big-endian IO is unsupported.\n");
    return 0;
#else
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
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
#endif  // CROARING_IS_BIG_ENDIAN
}
