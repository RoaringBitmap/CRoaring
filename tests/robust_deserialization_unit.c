/*
 * robust_deserialization_unit.c
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/misc/configreport.h>
#include <roaring/roaring.h>

#include "config.h"
#include "test.h"

long filesize(FILE* fp) {
    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}

char* readfile(FILE* fp, size_t* bytes) {
    *bytes = filesize(fp);
    char* buf = (char*)malloc(*bytes);
    if (buf == NULL) return NULL;

    rewind(fp);

    size_t cnt = fread(buf, 1, *bytes, fp);
    if (*bytes != cnt) {
        free(buf);
        return NULL;
    }
    return buf;
}

int compare(char* x, char* y, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (x[i] != y[i]) {
            return (int)(i + 1);
        }
    }
    return 0;
}

int test_deserialize(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open %s, check your configuration. \n", filename);
        assert_false(fp == NULL);
    }
    size_t bytes;
    char* input_buffer = readfile(fp, &bytes);

    if (input_buffer == NULL) {
        printf("Could not read bytes from %s, check your configuration. \n",
               filename);
        assert_false(input_buffer == NULL);
    }
    printf("Binary content read.\n");

    roaring_bitmap_t* bitmap =
        roaring_bitmap_portable_deserialize_safe(input_buffer, bytes);

    if (bitmap == NULL) {
        printf("Null bitmap loaded.\n");
        free(input_buffer);
        return 1;  // this is the expected behavior
    }
    printf("Non-null bitmap loaded.\n");

    size_t expected_size = roaring_bitmap_portable_size_in_bytes(bitmap);

    char* output_buffer = (char*)malloc(expected_size);
    size_t actual_size =
        roaring_bitmap_portable_serialize(bitmap, output_buffer);

    if (actual_size != expected_size) {
        free(input_buffer);
        free(output_buffer);
        assert_int_equal(actual_size, expected_size);
    }

    int compare_result = compare(input_buffer, output_buffer, actual_size);

    free(output_buffer);
    free(input_buffer);
    fclose(fp);

    roaring_bitmap_free(bitmap);
    assert_false(compare_result);

    return compare_result;
}

DEFINE_TEST(test_robust_deserialize1) {
    test_deserialize(TEST_DATA_DIR "crashproneinput1.bin");
}

DEFINE_TEST(test_robust_deserialize2) {
    test_deserialize(TEST_DATA_DIR "crashproneinput2.bin");
}

DEFINE_TEST(test_robust_deserialize3) {
    test_deserialize(TEST_DATA_DIR "crashproneinput3.bin");
}

DEFINE_TEST(test_robust_deserialize4) {
    test_deserialize(TEST_DATA_DIR "crashproneinput4.bin");
}

DEFINE_TEST(test_robust_deserialize5) {
    test_deserialize(TEST_DATA_DIR "crashproneinput5.bin");
}

DEFINE_TEST(test_robust_deserialize6) {
    test_deserialize(TEST_DATA_DIR "crashproneinput6.bin");
}

DEFINE_TEST(test_robust_deserialize7) {
    test_deserialize(TEST_DATA_DIR "crashproneinput7.bin");
}

static void invalid_deserialize_test(const void* data, size_t size,
                                     const char* description) {
    // Ensure that the data _looks_ like a valid bitmap, but is not.
    size_t serialized_size =
        roaring_bitmap_portable_deserialize_size(data, size);
    if (serialized_size != size) {
        fail_msg("expected size %zu, got %zu", size, serialized_size);
    }
    // If we truncate the data by one byte, we should get a size of 0
    assert_int_equal(roaring_bitmap_portable_deserialize_size(data, size - 1),
                     0);
    roaring_bitmap_t* bitmap =
        roaring_bitmap_portable_deserialize_safe(data, size);
    if (bitmap != NULL) {
        if (roaring_bitmap_internal_validate(bitmap, NULL)) {
            fail_msg("Validation must fail if a bitmap was returned, %s",
                     description);
        }
        roaring_bitmap_free(bitmap);
    }
    // Truncated data will never return a bitmap
    bitmap = roaring_bitmap_portable_deserialize_safe(data, size - 1);
    assert_null(bitmap);
}

static void valid_deserialize_test(const void* data, size_t size) {
    // Ensure that the data _looks_ like a valid bitmap, but is not.
    size_t serialized_size =
        roaring_bitmap_portable_deserialize_size(data, size);
    if (serialized_size != size) {
        fail_msg("expected size %zu, got %zu", size, serialized_size);
    }
    // If we truncate the data by one byte, we should get a size of 0
    assert_int_equal(roaring_bitmap_portable_deserialize_size(data, size - 1),
                     0);
    roaring_bitmap_t* bitmap =
        roaring_bitmap_portable_deserialize_safe(data, size);
    assert_non_null(bitmap);
    assert_true(roaring_bitmap_internal_validate(bitmap, NULL));
    roaring_bitmap_free(bitmap);
}

DEFINE_TEST(deserialize_duplicate_keys) {
    // clang-format off
    const char data[] = {
        0x3B, 0x30, // Serial Cookie
        1, 0,       // Container count - 1
        0,          // Run Flag Bitset (no runs)
        0, 0,       // Upper 16 bits of the first container
        0, 0,       // Cardinality - 1 of the first container
        0, 0,       // Upper 16 bits of the second container - DUPLICATE
        0, 0,       // Cardinality - 1 of the second container
        0, 0,       // Only value of first container
        0, 0,       // Only value of second container
    };
    // clang-format on
    invalid_deserialize_test(data, sizeof(data), "overlapping keys");
}

DEFINE_TEST(deserialize_unsorted_keys) {
    // clang-format off
    const char data[] = {
        0x3B, 0x30, // Serial Cookie
        1, 0,       // Container count - 1
        0,          // Run Flag Bitset (no runs)
        1, 0,       // Upper 16 bits of the first container
        0, 0,       // Cardinality - 1 of the first container
        0, 0,       // Upper 16 bits of the second container (LESS THAN FIRST)
        0, 0,       // Cardinality - 1 of the second container
        0, 0,       // Only value of first container
        0, 0,       // Only value of second container
    };
    // clang-format on
    invalid_deserialize_test(data, sizeof(data), "unsorted keys");
}

DEFINE_TEST(deserialize_duplicate_array) {
    // clang-format off
    const char data[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0,          // Run Flag Bitset (no runs)
        0, 0,       // Upper 16 bits of the first container
        1, 0,       // Cardinality - 1 of the first container
        1, 0,       // first value of first container
        0, 0,       // second value of first container (LESS THAN FIRST)
    };
    // clang-format on
    invalid_deserialize_test(data, sizeof(data), "duplicate array values");
}

DEFINE_TEST(deserialize_unsorted_array) {
    // clang-format off
    const char data[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0,          // Run Flag Bitset (no runs)
        0, 0,       // Upper 16 bits of the first container
        1, 0,       // Cardinality - 1 of the first container
        0, 0,       // first value of first container
        0, 0,       // second value of first container (DUPLICATE)
    };
    // clang-format on
    invalid_deserialize_test(data, sizeof(data), "duplicate array values");
}

DEFINE_TEST(deserialize_bitset_incorrect_cardinality) {
    // clang-format off
    const uint8_t data_begin[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0,          // Run Flag Bitset (no runs)
        0, 0,       // Upper 16 bits of the first container
        0xFF, 0xFF, // Cardinality - 1 of the first container.

        // First container is a bitset, should be followed by 1 << 16 bits
    };
    // clang-format on
    uint8_t data[sizeof(data_begin) + (1 << 16) / 8];

    memcpy(data, data_begin, sizeof(data_begin));
    memset(data + sizeof(data_begin), 0xFF, (1 << 16) / 8);

    valid_deserialize_test(data, sizeof(data));

    data[sizeof(data) - 1] = 0xFE;
    invalid_deserialize_test(data, sizeof(data),
                             "Incorrect bitset cardinality");
}

int main() {
    tellmeall();
#if CROARING_IS_BIG_ENDIAN
    printf("Big-endian IO unsupported.\n");
    return EXIT_SUCCESS;
#else
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_robust_deserialize1),
        cmocka_unit_test(test_robust_deserialize2),
        cmocka_unit_test(test_robust_deserialize3),
        cmocka_unit_test(test_robust_deserialize4),
        cmocka_unit_test(test_robust_deserialize5),
        cmocka_unit_test(test_robust_deserialize6),
        cmocka_unit_test(test_robust_deserialize7),
        cmocka_unit_test(deserialize_duplicate_keys),
        cmocka_unit_test(deserialize_unsorted_keys),
        cmocka_unit_test(deserialize_duplicate_array),
        cmocka_unit_test(deserialize_unsorted_array),
        cmocka_unit_test(deserialize_bitset_incorrect_cardinality),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
#endif
}
