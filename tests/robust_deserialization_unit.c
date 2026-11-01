/*
 * robust_deserialization_unit.c
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/misc/configreport.h>
#include <roaring/portability.h>
#include <roaring/roaring.h>

#include "config.h"
#include "test.h"

#define MAX_CONTAINERS (1 << 16)

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

DEFINE_TEST(deserialize_negative_container_count) {
    // clang-format off
    const uint8_t data[] = {
        0x3A, 0x30, 0, 0,       // Serial Cookie No Run Container
        0x00, 0x00, 0x00, 0x80, // Container count (NEGATIVE)
    };
    // clang-format on
    assert_int_equal(roaring_bitmap_portable_deserialize_size((const char*)data,
                                                              sizeof(data)),
                     0);
    roaring_bitmap_t* bitmap = roaring_bitmap_portable_deserialize_safe(
        (const char*)data, sizeof(data));
    assert_null(bitmap);
}

DEFINE_TEST(deserialize_huge_container_count) {
    // clang-format off
    const uint8_t data_begin[] = {
        0x3A, 0x30, 0, 0,       // Serial Cookie No Run Container
        0x00, 0x00, 0x01, 0x00, // Container count (MAX_CONTAINERS + 1)
    };
    // clang-format on
#define EXTRA_DATA(num_containers) \
    ((num_containers) * (3 * sizeof(uint16_t) + sizeof(uint32_t)))

    // For each container, 32 bits for container offset,
    //   16 bits each for a key, cardinality - 1, and a value
    uint8_t data[sizeof(data_begin) + EXTRA_DATA(MAX_CONTAINERS + 1)];
    memcpy(data, data_begin, sizeof(data_begin));
    memset(data + sizeof(data_begin), 0, EXTRA_DATA(MAX_CONTAINERS + 1));

    size_t valid_size = sizeof(data_begin) + EXTRA_DATA(MAX_CONTAINERS);
    assert_int_equal(
        roaring_bitmap_portable_deserialize_size((const char*)data, valid_size),
        valid_size);

    // Add an extra container
    data[4] += 1;
    assert_int_equal(roaring_bitmap_portable_deserialize_size((const char*)data,
                                                              sizeof(data)),
                     0);
    roaring_bitmap_t* bitmap = roaring_bitmap_portable_deserialize_safe(
        (const char*)data, sizeof(data));
    assert_null(bitmap);
#undef EXTRA_DATA
}

DEFINE_TEST(deserialize_run_container_empty) {
    // clang-format off
    const uint8_t data[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0x01,       // Run Flag Bitset (single container is a run)
        0, 0,       // Upper 16 bits of the first container
        0, 0,       // Cardinality - 1 of the first container
        0, 0,       // First Container - Number of runs
    };
    // clang-format on
    invalid_deserialize_test(data, sizeof(data), "empty run container");
}

DEFINE_TEST(deserialize_run_container_should_combine) {
    // clang-format off
    const uint8_t data[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0x01,       // Run Flag Bitset (single container is a run)
        0, 0,       // Upper 16 bits of the first container
        1, 0,       // Cardinality - 1 of the first container
        2, 0,       // First Container - Number of runs
        0, 0,       // First run start
        0, 0,       // First run length - 1
        1, 0,       // Second run start (STARTS AT THE END OF THE FIRST)
        0, 0,       // Second run length - 1
    };
    // clang-format on
    invalid_deserialize_test(data, sizeof(data),
                             "ranges shouldn't be contiguous");
}

DEFINE_TEST(deserialize_run_container_overlap) {
    // clang-format off
    const uint8_t data[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0x01,       // Run Flag Bitset (single container is a run)
        0, 0,       // Upper 16 bits of the first container
        4, 0,       // Cardinality - 1 of the first container
        2, 0,       // First Container - Number of runs
        0, 0,       // First run start
        4, 0,       // First run length - 1
        1, 0,       // Second run start (STARTS INSIDE THE FIRST)
        0, 0,       // Second run length - 1
    };
    // clang-format on
    invalid_deserialize_test(data, sizeof(data), "overlapping ranges");
}

DEFINE_TEST(deserialize_run_container_overflow) {
    // clang-format off
    const uint8_t data[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0x01,       // Run Flag Bitset (single container is a run)
        0, 0,       // Upper 16 bits of the first container
        4, 0,       // Cardinality - 1 of the first container
        1, 0,       // First Container - Number of runs
        0xFE, 0xFF, // First run start
        4, 0,       // First run length - 1 (OVERFLOW)
    };
    // clang-format on
    invalid_deserialize_test(data, sizeof(data), "run length overflow");
}

DEFINE_TEST(deserialize_run_container_incorrect_cardinality_still_allowed) {
    // clang-format off
    const uint8_t data[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0x01,       // Run Flag Bitset (single container is a run)
        0, 0,       // Upper 16 bits of the first container
        0, 0,       // Cardinality - 1 of the first container
        1, 0,       // First Container - Number of runs
        0, 0,       // First run start
        8, 0,       // First run length - 1 (9 items, but cardinality is 1)
    };
    // clang-format on

    // The cardinality doesn't match the actual number of items in the run,
    // but the implementation ignores the cardinality field.
    valid_deserialize_test(data, sizeof(data));
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

// Reads a whole file; *bytes receives its size. Works for empty files.
static char* read_whole_file(const char* filename, size_t* bytes) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open %s, check your configuration. \n", filename);
    }
    assert_non_null(fp);
    *bytes = (size_t)filesize(fp);
    char* buf = (char*)malloc(*bytes + 1);  // +1: never malloc(0)
    assert_non_null(buf);
    rewind(fp);
    assert_int_equal(fread(buf, 1, *bytes, fp), *bytes);
    fclose(fp);
    return buf;
}

// The file must be rejected by both the size probe and the safe deserializer.
static void expect_rejected_file(const char* filename) {
    size_t bytes;
    char* buf = read_whole_file(filename, &bytes);
    assert_int_equal(roaring_bitmap_portable_deserialize_size(buf, bytes), 0);
    roaring_bitmap_t* bitmap =
        roaring_bitmap_portable_deserialize_safe(buf, bytes);
    assert_null(bitmap);
    free(buf);
}

// Also exercised by the Go library (testdata/crash*).
DEFINE_TEST(test_robust_deserialize8) {
    test_deserialize(TEST_DATA_DIR "crashproneinput8.bin");
    expect_rejected_file(TEST_DATA_DIR "crashproneinput8.bin");
}

// "0000" in ASCII: not a valid cookie.
DEFINE_TEST(test_robust_deserialize_invalid_cookie) {
    expect_rejected_file(TEST_DATA_DIR "crashwithinvalidcookie.bin");
}

// Empty file: no cookie at all.
DEFINE_TEST(test_robust_deserialize_without_cookie) {
    expect_rejected_file(TEST_DATA_DIR "crashwithoutcookie.bin");
}

// bad-bitmap.bin (shared with Java): a single array container with
// 1518 values that are mostly zero. It is accepted by the deserializer, but
// it must not validate, and it must re-serialize to the same bytes.
DEFINE_TEST(test_robust_deserialize_bad_bitmap) {
    size_t bytes;
    char* buf = read_whole_file(TEST_DATA_DIR "bad-bitmap.bin", &bytes);
    assert_int_equal(bytes, 3052);
    assert_int_equal(roaring_bitmap_portable_deserialize_size(buf, bytes),
                     bytes);
    roaring_bitmap_t* bitmap =
        roaring_bitmap_portable_deserialize_safe(buf, bytes);
    assert_non_null(bitmap);
    assert_int_equal(roaring_bitmap_get_cardinality(bitmap), 1518);
    const char* reason = NULL;
    assert_false(roaring_bitmap_internal_validate(bitmap, &reason));
    assert_non_null(reason);
    assert_int_equal(roaring_bitmap_portable_size_in_bytes(bitmap), bytes);
    char* out = (char*)malloc(bytes);
    assert_int_equal(roaring_bitmap_portable_serialize(bitmap, out), bytes);
    assert_memory_equal(out, buf, bytes);
    free(out);
    roaring_bitmap_free(bitmap);
    free(buf);
}

// A run container whose last run ends exactly at 65535 is valid; one that
// would end at 65536 is not (Java validateRejectsRunPastTheEndOfTheContainer).
DEFINE_TEST(deserialize_run_container_end_boundary) {
    // clang-format off
    uint8_t data[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0x01,       // Run Flag Bitset (single container is a run)
        0, 0,       // Upper 16 bits of the first container
        0x17, 0x02, // Cardinality - 1 of the first container (535)
        1, 0,       // First Container - Number of runs
        0xE8, 0xFD, // First run start (65000)
        0x17, 0x02, // First run length - 1 (535): ends at 65535
    };
    // clang-format on
    valid_deserialize_test(data, sizeof(data));
    roaring_bitmap_t* bitmap = roaring_bitmap_portable_deserialize_safe(
        (const char*)data, sizeof(data));
    assert_non_null(bitmap);
    assert_int_equal(roaring_bitmap_get_cardinality(bitmap), 536);
    assert_true(roaring_bitmap_contains(bitmap, 65535));
    assert_false(roaring_bitmap_contains(bitmap, 65536));
    roaring_bitmap_free(bitmap);

    data[13] = 0x18;  // length - 1 = 536: would end at 65536
    invalid_deserialize_test(data, sizeof(data), "run past container end");
}

// The offset header of the no-run format is ignored by the (non-frozen)
// deserializer: containers are read sequentially. This pins that behaviour.
DEFINE_TEST(deserialize_offsets_are_ignored) {
    // clang-format off
    const uint8_t aliasing[] = {
        0x3A, 0x30, 0, 0,       // Serial Cookie No Run Container
        2, 0, 0, 0,             // Container count
        0, 0, 0, 0,             // key 0, cardinality - 1 = 0
        1, 0, 0, 0,             // key 1, cardinality - 1 = 0
        24, 0, 0, 0,            // offset of container 0 (correct)
        24, 0, 0, 0,            // offset of container 1 (WRONG: should be 26)
        7, 0,                   // value of container 0
        9, 0,                   // value of container 1
    };
    const uint8_t out_of_range[] = {
        0x3A, 0x30, 0, 0,       // Serial Cookie No Run Container
        1, 0, 0, 0,             // Container count
        5, 0, 0, 0,             // key 5, cardinality - 1 = 0
        0xFF, 0, 0, 0,          // offset (points past the end)
        7, 0,                   // value
    };
    // clang-format on
    assert_int_equal(roaring_bitmap_portable_deserialize_size(
                         (const char*)aliasing, sizeof(aliasing)),
                     sizeof(aliasing));
    roaring_bitmap_t* r = roaring_bitmap_portable_deserialize_safe(
        (const char*)aliasing, sizeof(aliasing));
    assert_non_null(r);
    assert_true(roaring_bitmap_internal_validate(r, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(r), 2);
    assert_true(roaring_bitmap_contains(r, 7));
    assert_true(roaring_bitmap_contains(r, 65536 + 9));
    roaring_bitmap_free(r);

    assert_int_equal(roaring_bitmap_portable_deserialize_size(
                         (const char*)out_of_range, sizeof(out_of_range)),
                     sizeof(out_of_range));
    r = roaring_bitmap_portable_deserialize_safe((const char*)out_of_range,
                                                 sizeof(out_of_range));
    assert_non_null(r);
    assert_true(roaring_bitmap_internal_validate(r, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(r), 1);
    assert_true(roaring_bitmap_contains(r, 327687));
    roaring_bitmap_free(r);
}

// Bytes after a valid portable bitmap are not consumed.
DEFINE_TEST(deserialize_trailing_bytes) {
    // clang-format off
    const uint8_t data[] = {
        0x3A, 0x30, 0, 0,       // Serial Cookie No Run Container
        1, 0, 0, 0,             // Container count
        5, 0, 0, 0,             // key 5, cardinality - 1 = 0
        16, 0, 0, 0,            // offset
        7, 0,                   // value
        0xAA, 0xBB,             // garbage
    };
    // clang-format on
    assert_int_equal(roaring_bitmap_portable_deserialize_size((const char*)data,
                                                              sizeof(data)),
                     18);
    roaring_bitmap_t* r = roaring_bitmap_portable_deserialize_safe(
        (const char*)data, sizeof(data));
    assert_non_null(r);
    assert_true(roaring_bitmap_internal_validate(r, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(r), 1);
    assert_true(roaring_bitmap_contains(r, 327687));
    assert_int_equal(roaring_bitmap_portable_size_in_bytes(r), 18);
    roaring_bitmap_free(r);
}

// Run containers that are not the smallest encoding are still valid.
DEFINE_TEST(deserialize_non_minimal_run_containers) {
    // clang-format off
    const uint8_t small[] = {
        0x3B, 0x30, // Serial Cookie
        0, 0,       // Container count - 1
        0x01,       // Run Flag Bitset
        0, 0,       // key
        1, 0,       // Cardinality - 1 (2 values)
        1, 0,       // Number of runs
        0, 0,       // run start
        1, 0,       // run length - 1 (2 values; an array would be smaller)
    };
    // clang-format on
    valid_deserialize_test(small, sizeof(small));

    // 3000 runs of length 1 at 4*i: larger than a bitset container.
    const size_t nruns = 3000;
    const size_t size = 11 + 4 * nruns;
    uint8_t* data = (uint8_t*)malloc(size);
    const uint8_t header[] = {0x3B,
                              0x30,
                              0,
                              0,
                              0x01,
                              0,
                              0,
                              (uint8_t)((nruns - 1) & 0xFF),
                              (uint8_t)((nruns - 1) >> 8),
                              (uint8_t)(nruns & 0xFF),
                              (uint8_t)(nruns >> 8)};
    memcpy(data, header, sizeof(header));
    for (size_t i = 0; i < nruns; i++) {
        uint16_t start = (uint16_t)(4 * i);
        data[11 + 4 * i] = (uint8_t)(start & 0xFF);
        data[11 + 4 * i + 1] = (uint8_t)(start >> 8);
        data[11 + 4 * i + 2] = 0;
        data[11 + 4 * i + 3] = 0;
    }
    assert_int_equal(size, 12011);
    valid_deserialize_test(data, size);
    roaring_bitmap_t* r =
        roaring_bitmap_portable_deserialize_safe((const char*)data, size);
    assert_non_null(r);
    assert_int_equal(roaring_bitmap_get_cardinality(r), nruns);
    assert_true(roaring_bitmap_contains(r, 4 * 2999));
    assert_false(roaring_bitmap_contains(r, 1));
    roaring_bitmap_free(r);
    free(data);
}

static void check_portable_round_trip(const roaring_bitmap_t* r) {
    size_t size = roaring_bitmap_portable_size_in_bytes(r);
    char* buf = (char*)malloc(size);
    assert_int_equal(roaring_bitmap_portable_serialize(r, buf), size);
    assert_int_equal(roaring_bitmap_portable_deserialize_size(buf, size), size);
    roaring_bitmap_t* r2 = roaring_bitmap_portable_deserialize_safe(buf, size);
    assert_non_null(r2);
    assert_true(roaring_bitmap_internal_validate(r2, NULL));
    assert_true(roaring_bitmap_equals(r, r2));
    roaring_bitmap_free(r2);
    r2 = roaring_bitmap_portable_deserialize(buf);
    assert_non_null(r2);
    assert_true(roaring_bitmap_equals(r, r2));
    roaring_bitmap_free(r2);
    free(buf);
}

// The empty bitmap serializes to exactly 8 bytes; shorter inputs are
// rejected.
DEFINE_TEST(serialize_empty_golden_bytes) {
    const uint8_t expected[] = {0x3A, 0x30, 0, 0, 0, 0, 0, 0};
    roaring_bitmap_t* r = roaring_bitmap_create();
    assert_int_equal(roaring_bitmap_portable_size_in_bytes(r), 8);
    char buf[8];
    assert_int_equal(roaring_bitmap_portable_serialize(r, buf), 8);
    assert_memory_equal(buf, expected, 8);
    check_portable_round_trip(r);
    roaring_bitmap_t* r2 = roaring_bitmap_portable_deserialize_safe(buf, 8);
    assert_non_null(r2);
    assert_true(roaring_bitmap_is_empty(r2));
    assert_true(roaring_bitmap_internal_validate(r2, NULL));
    roaring_bitmap_free(r2);
    const size_t bad_lengths[] = {0, 4, 7};
    for (size_t i = 0; i < 3; i++) {
        assert_int_equal(
            roaring_bitmap_portable_deserialize_size(buf, bad_lengths[i]), 0);
        assert_null(
            roaring_bitmap_portable_deserialize_safe(buf, bad_lengths[i]));
    }
#if !CROARING_IS_BIG_ENDIAN
    // Frozen format is native-endian.
    assert_int_equal(roaring_bitmap_frozen_size_in_bytes(r), 4);
    char* fbuf = (char*)roaring_aligned_malloc(32, 4);
    roaring_bitmap_frozen_serialize(r, fbuf);
    const uint8_t frozen_expected[] = {0xC6, 0x35, 0, 0};
    assert_memory_equal(fbuf, frozen_expected, 4);
    roaring_aligned_free(fbuf);
#endif
    roaring_bitmap_free(r);
}

// k containers, each [j<<16, (j<<16)+10), around NO_OFFSET_THRESHOLD (4):
// with fewer than 4 containers a run-format bitmap has no offset header.
DEFINE_TEST(serialize_no_offset_threshold_golden_bytes) {
    // clang-format off
    static const uint8_t k1[] = {
        0x3b,0x30,0x00,0x00,0x01,0x00,0x00,0x09,0x00,0x01,0x00,0x00,0x00,
        0x09,0x00};
    static const uint8_t k2[] = {
        0x3b,0x30,0x01,0x00,0x03,0x00,0x00,0x09,0x00,0x01,0x00,0x09,0x00,
        0x01,0x00,0x00,0x00,0x09,0x00,0x01,0x00,0x00,0x00,0x09,0x00};
    static const uint8_t k3[] = {
        0x3b,0x30,0x02,0x00,0x07,0x00,0x00,0x09,0x00,0x01,0x00,0x09,0x00,
        0x02,0x00,0x09,0x00,0x01,0x00,0x00,0x00,0x09,0x00,0x01,0x00,0x00,
        0x00,0x09,0x00,0x01,0x00,0x00,0x00,0x09,0x00};
    static const uint8_t k4[] = {
        0x3b,0x30,0x03,0x00,0x0f,0x00,0x00,0x09,0x00,0x01,0x00,0x09,0x00,
        0x02,0x00,0x09,0x00,0x03,0x00,0x09,0x00,0x25,0x00,0x00,0x00,0x2b,
        0x00,0x00,0x00,0x31,0x00,0x00,0x00,0x37,0x00,0x00,0x00,0x01,0x00,
        0x00,0x00,0x09,0x00,0x01,0x00,0x00,0x00,0x09,0x00,0x01,0x00,0x00,
        0x00,0x09,0x00,0x01,0x00,0x00,0x00,0x09,0x00};
    // clang-format on
    const uint8_t* golden[] = {k1, k2, k3, k4};
    const size_t golden_size[] = {sizeof(k1), sizeof(k2), sizeof(k3),
                                  sizeof(k4)};
    const size_t run_sizes[] = {15, 25, 35, 61, 75};
    for (uint32_t k = 1; k <= 5; k++) {
        roaring_bitmap_t* r = roaring_bitmap_create();
        roaring_bitmap_t* plain = roaring_bitmap_create();
        for (uint32_t j = 0; j < k; j++) {
            roaring_bitmap_add_range(r, (uint64_t)j << 16,
                                     ((uint64_t)j << 16) + 10);
            roaring_bitmap_add(plain, (j << 16) + 7);
        }
        roaring_bitmap_run_optimize(r);
        size_t size = roaring_bitmap_portable_size_in_bytes(r);
        assert_int_equal(size, run_sizes[k - 1]);
        char* buf = (char*)malloc(size);
        assert_int_equal(roaring_bitmap_portable_serialize(r, buf), size);
        if (k <= 4) {
            assert_int_equal(size, golden_size[k - 1]);
            assert_memory_equal(buf, golden[k - 1], size);
        }
        free(buf);
        check_portable_round_trip(r);
        // without runs: 8 + 10 * k bytes
        assert_int_equal(roaring_bitmap_portable_size_in_bytes(plain),
                         8 + 10 * k);
        check_portable_round_trip(plain);
        roaring_bitmap_free(r);
        roaring_bitmap_free(plain);
    }
}

// Array with 4096 values vs bitset with 4097 values: header and size.
DEFINE_TEST(serialize_array_bitset_boundary_golden_bytes) {
    for (uint32_t card = 4096; card <= 4097; card++) {
        roaring_bitmap_t* r = roaring_bitmap_create();
        for (uint32_t i = 0; i < card; i++) roaring_bitmap_add(r, 2 * i);
        size_t size = roaring_bitmap_portable_size_in_bytes(r);
        assert_int_equal(size, 8208);
        uint8_t* buf = (uint8_t*)malloc(size);
        assert_int_equal(roaring_bitmap_portable_serialize(r, (char*)buf),
                         size);
        const uint8_t header[] = {0x3A,
                                  0x30,
                                  0,
                                  0,
                                  1,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  (uint8_t)((card - 1) & 0xFF),
                                  (uint8_t)((card - 1) >> 8),
                                  16,
                                  0,
                                  0,
                                  0};
        assert_memory_equal(buf, header, sizeof(header));
        roaring_bitmap_t* r2 =
            roaring_bitmap_portable_deserialize_safe((const char*)buf, size);
        assert_non_null(r2);
        assert_true(roaring_bitmap_internal_validate(r2, NULL));
        assert_int_equal(roaring_bitmap_get_cardinality(r2), card);
        roaring_statistics_t stats;
        roaring_bitmap_statistics(r2, &stats);
        assert_int_equal(stats.n_array_containers, card == 4096 ? 1 : 0);
        assert_int_equal(stats.n_bitset_containers, card == 4097 ? 1 : 0);
        uint8_t* buf2 = (uint8_t*)malloc(size);
        assert_int_equal(roaring_bitmap_portable_serialize(r2, (char*)buf2),
                         size);
        assert_memory_equal(buf, buf2, size);
        free(buf2);
        free(buf);
        roaring_bitmap_free(r2);
        roaring_bitmap_free(r);
    }
}

// Round trip for every container count from 1 to 15, with and without runs
// (Java TestSerializationViaByteBuffer).
DEFINE_TEST(serialize_small_container_counts) {
    for (uint32_t keys = 1; keys <= 15; keys++) {
        for (int optimize = 0; optimize <= 1; optimize++) {
            roaring_bitmap_t* r = roaring_bitmap_create();
            for (uint32_t j = 0; j < keys; j++) {
                uint32_t base = (3 * j + 1) << 16;
                switch (j % 3) {
                    case 0:  // array
                        for (uint32_t i = 0; i < 100; i++)
                            roaring_bitmap_add(r, base + 7 * i);
                        break;
                    case 1:  // bitset
                        for (uint32_t i = 0; i < 10000; i++)
                            roaring_bitmap_add(r, base + 2 * i);
                        break;
                    default:  // runs (once optimized)
                        for (uint32_t i = 0; i < 1000; i++)
                            roaring_bitmap_add(r, base + 100 + i);
                        break;
                }
            }
            if (optimize) roaring_bitmap_run_optimize(r);
            assert_true(roaring_bitmap_internal_validate(r, NULL));
            check_portable_round_trip(r);
            roaring_bitmap_free(r);
        }
    }
}

// Two bitmaps written back to back in one buffer.
DEFINE_TEST(deserialize_back_to_back) {
    roaring_bitmap_t* empty = roaring_bitmap_create();
    roaring_bitmap_t* a = roaring_bitmap_from_range(0, 200000, 3);
    roaring_bitmap_add_range(a, 500000, 600000);
    roaring_bitmap_run_optimize(a);
    size_t n1 = roaring_bitmap_portable_size_in_bytes(empty);
    size_t n2 = roaring_bitmap_portable_size_in_bytes(a);
    char* buf = (char*)malloc(n1 + n2);
    assert_int_equal(roaring_bitmap_portable_serialize(empty, buf), n1);
    assert_int_equal(roaring_bitmap_portable_serialize(a, buf + n1), n2);
    assert_int_equal(roaring_bitmap_portable_deserialize_size(buf, n1 + n2), 8);
    roaring_bitmap_t* r1 =
        roaring_bitmap_portable_deserialize_safe(buf, n1 + n2);
    assert_non_null(r1);
    assert_true(roaring_bitmap_is_empty(r1));
    assert_int_equal(roaring_bitmap_portable_deserialize_size(buf + n1, n2),
                     n2);
    roaring_bitmap_t* r2 =
        roaring_bitmap_portable_deserialize_safe(buf + n1, n2);
    assert_non_null(r2);
    assert_true(roaring_bitmap_equals(a, r2));
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    free(buf);
    roaring_bitmap_free(a);
    roaring_bitmap_free(empty);
}

// The (non-frozen) portable deserializer works at any byte offset.
DEFINE_TEST(deserialize_unaligned_source) {
    roaring_bitmap_t* r = roaring_bitmap_create();
    for (uint32_t i = 0; i < 100; i++) roaring_bitmap_add(r, 3 * i);
    for (uint32_t i = 0; i < 10000; i++) roaring_bitmap_add(r, 65536 + 2 * i);
    roaring_bitmap_add_range(r, 3 * 65536, 3 * 65536 + 5000);
    for (uint32_t i = 0; i < 5; i++) roaring_bitmap_add(r, (10 + i) << 16);
    roaring_bitmap_run_optimize(r);
    size_t size = roaring_bitmap_portable_size_in_bytes(r);
    char* storage = (char*)malloc(size + 8);
    for (size_t off = 0; off < 8; off++) {
        assert_int_equal(roaring_bitmap_portable_serialize(r, storage + off),
                         size);
        roaring_bitmap_t* r2 =
            roaring_bitmap_portable_deserialize_safe(storage + off, size);
        assert_non_null(r2);
        assert_true(roaring_bitmap_internal_validate(r2, NULL));
        assert_true(roaring_bitmap_equals(r, r2));
        roaring_bitmap_free(r2);
        r2 = roaring_bitmap_portable_deserialize(storage + off);
        assert_true(roaring_bitmap_equals(r, r2));
        roaring_bitmap_free(r2);
    }
    free(storage);
    roaring_bitmap_free(r);
}

// Every strict prefix of a valid serialized bitmap is rejected.
static void check_truncations(const char* buf, size_t size, size_t stride) {
    for (size_t k = 0; k < size; k++) {
        bool full = (stride == 1) || (k % stride == 0) || (k + 64 >= size);
        assert_int_equal(roaring_bitmap_portable_deserialize_size(buf, k), 0);
        if (full) {
            roaring_bitmap_t* r =
                roaring_bitmap_portable_deserialize_safe(buf, k);
            if (r != NULL) {
                fail_msg("prefix of length %zu of %zu was accepted", k, size);
            }
        }
    }
    assert_int_equal(roaring_bitmap_portable_deserialize_size(buf, size), size);
}

DEFINE_TEST(deserialize_truncation_sweep) {
    const char* files[] = {TEST_DATA_DIR "bitmapwithruns.bin",
                           TEST_DATA_DIR "bitmapwithoutruns.bin",
                           TEST_DATA_DIR "testfrozendata/mixed.portable",
                           TEST_DATA_DIR "testfrozendata/runs_only.portable",
                           TEST_DATA_DIR "testfrozendata/arrays_only.portable",
                           TEST_DATA_DIR
                           "testfrozendata/bitmaps_only.portable"};
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        size_t bytes;
        char* buf = read_whole_file(files[i], &bytes);
        // the safe deserializer is tried on every prefix for small files and
        // on a sample of prefixes for larger ones
        check_truncations(buf, bytes, bytes > 20000 ? 97 : 1);
        free(buf);
    }
}

#if !CROARING_IS_BIG_ENDIAN
// Builds a frozen buffer holding n one-value array containers. Keys are i
// (mod 65536). Returns a 32-byte aligned buffer; *len receives its size.
static char* make_frozen_arrays(uint32_t n, size_t* len) {
    *len = (size_t)n * (2 + 5) + 4;
    char* buf = (char*)roaring_aligned_malloc(32, *len);
    assert_non_null(buf);
    uint16_t* values = (uint16_t*)buf;
    uint16_t* keys = (uint16_t*)(buf + 2 * (size_t)n);
    uint16_t* counts = (uint16_t*)(buf + 4 * (size_t)n);
    uint8_t* typecodes = (uint8_t*)(buf + 6 * (size_t)n);
    for (uint32_t i = 0; i < n; i++) {
        values[i] = 7;
        keys[i] = (uint16_t)i;
        counts[i] = 0;     // cardinality - 1
        typecodes[i] = 2;  // ARRAY_CONTAINER_TYPE
    }
    uint32_t header = (n << 15) | 13766;  // FROZEN_COOKIE
    memcpy(buf + *len - 4, &header, 4);
    return buf;
}

// At most 65536 containers can be held in a frozen view (Go rejects more
// with ErrFrozenBitmapOverpopulated).
DEFINE_TEST(frozen_view_too_many_containers) {
    size_t len;
    char* buf = make_frozen_arrays(1 << 16, &len);
    const roaring_bitmap_t* r = roaring_bitmap_frozen_view(buf, len);
    assert_non_null(r);
    assert_true(roaring_bitmap_internal_validate(r, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(r), 1 << 16);
    assert_true(roaring_bitmap_contains(r, 0xFFFF0007));
    roaring_bitmap_free(r);
    roaring_aligned_free(buf);

    buf = make_frozen_arrays((1 << 16) + 1, &len);
    assert_null(roaring_bitmap_frozen_view(buf, len));
    roaring_aligned_free(buf);
}

// Frozen inputs that must be refused.
DEFINE_TEST(frozen_view_negative_cases) {
    roaring_bitmap_t* r = roaring_bitmap_create();
    for (uint32_t i = 0; i < 100; i++) roaring_bitmap_add(r, 5 * i);
    for (uint32_t i = 0; i < 10000; i++) roaring_bitmap_add(r, 65536 + 2 * i);
    roaring_bitmap_add_range(r, 2 * 65536, 2 * 65536 + 1000);
    roaring_bitmap_run_optimize(r);
    size_t len = roaring_bitmap_frozen_size_in_bytes(r);
    char* buf = (char*)roaring_aligned_malloc(32, len + 32);
    roaring_bitmap_frozen_serialize(r, buf);
    const roaring_bitmap_t* v = roaring_bitmap_frozen_view(buf, len);
    assert_non_null(v);
    assert_true(roaring_bitmap_equals(r, v));
    roaring_bitmap_free(v);
    // truncated at an aligned address
    assert_null(roaring_bitmap_frozen_view(buf, len - 1));
    // trailing byte after the header
    buf[len] = 0;
    assert_null(roaring_bitmap_frozen_view(buf, len + 1));
    // fewer than 4 bytes
    assert_null(roaring_bitmap_frozen_view(buf, 3));
    // leading (aligned) padding before a valid frozen bitmap
    {
        char* padded = (char*)roaring_aligned_malloc(32, len + 32);
        memset(padded, 0, 32);
        memcpy(padded + 32, buf, len);
        assert_null(roaring_bitmap_frozen_view(padded, len + 32));
        roaring_aligned_free(padded);
    }
    // big-endian header
    {
        const uint8_t be[] = {0, 0, 0x35, 0xC6};
        char* small = (char*)roaring_aligned_malloc(32, 4);
        memcpy(small, be, 4);
        assert_null(roaring_bitmap_frozen_view(small, 4));
        // portable bytes of an empty bitmap are not a frozen bitmap either
        const uint8_t portable_empty[] = {0x3A, 0x30, 0, 0, 0, 0, 0, 0};
        roaring_aligned_free(small);
        small = (char*)roaring_aligned_malloc(32, 8);
        memcpy(small, portable_empty, 8);
        assert_null(roaring_bitmap_frozen_view(small, 8));
        roaring_aligned_free(small);
    }
    // invalid typecodes (0 and 4) for the first container
    size_t ncontainers = 3;
    char* typecode = buf + len - 4 - ncontainers;
    char saved = *typecode;
    *typecode = 0;
    assert_null(roaring_bitmap_frozen_view(buf, len));
    *typecode = 4;
    assert_null(roaring_bitmap_frozen_view(buf, len));
    *typecode = saved;
    v = roaring_bitmap_frozen_view(buf, len);
    assert_non_null(v);
    roaring_bitmap_free(v);
    roaring_aligned_free(buf);
    roaring_bitmap_free(r);
}

// Corrupt the payload of a frozen bitmap: the view is still created (it does
// not inspect the payload), but validation must fail
// (Go TestBitMapValidationFromFrozen).
static void check_frozen_corruption(roaring_bitmap_t* r, size_t offset,
                                    const uint8_t* patch, size_t patch_len) {
    size_t len = roaring_bitmap_frozen_size_in_bytes(r);
    char* buf = (char*)roaring_aligned_malloc(32, len);
    roaring_bitmap_frozen_serialize(r, buf);
    const roaring_bitmap_t* v = roaring_bitmap_frozen_view(buf, len);
    assert_non_null(v);
    assert_true(roaring_bitmap_internal_validate(v, NULL));
    roaring_bitmap_free(v);
    memcpy(buf + offset, patch, patch_len);
    v = roaring_bitmap_frozen_view(buf, len);
    assert_non_null(v);
    const char* reason = NULL;
    assert_false(roaring_bitmap_internal_validate(v, &reason));
    assert_non_null(reason);
    roaring_bitmap_free(v);
    roaring_aligned_free(buf);
    roaring_bitmap_free(r);
}

DEFINE_TEST(frozen_view_corrupted_payload) {
    const uint8_t ffff[] = {0xFF, 0xFF};
    // runs [0,2), [4,6), [8,100): last run length at bytes 10-11
    roaring_bitmap_t* r = roaring_bitmap_create();
    roaring_bitmap_add_range(r, 0, 2);
    roaring_bitmap_add_range(r, 4, 6);
    roaring_bitmap_add_range(r, 8, 100);
    roaring_bitmap_run_optimize(r);
    check_frozen_corruption(r, 10, ffff, 2);
    // single run [100,110): run length at bytes 2-3
    r = roaring_bitmap_from_range(100, 110, 1);
    roaring_bitmap_run_optimize(r);
    check_frozen_corruption(r, 2, ffff, 2);
    // runs [100,110) and [115,125): second start moved to 108 (overlap)
    r = roaring_bitmap_create();
    roaring_bitmap_add_range(r, 100, 110);
    roaring_bitmap_add_range(r, 115, 125);
    roaring_bitmap_run_optimize(r);
    const uint8_t overlap[] = {108, 0};
    check_frozen_corruption(r, 4, overlap, 2);
    // array 0..9: third value set to 0 (sort order)
    r = roaring_bitmap_create();
    for (uint32_t i = 0; i < 10; i++) roaring_bitmap_add(r, i);
    const uint8_t zero[] = {0, 0};
    check_frozen_corruption(r, 4, zero, 2);
}
#endif  // !CROARING_IS_BIG_ENDIAN

// Go fuzzer reproducers (roaring_test.go TestFuzzerRepro_*): decode, validate,
// apply the operation, validate again.
static roaring_bitmap_t* decode_valid(const uint8_t* data, size_t size) {
    assert_int_equal(
        roaring_bitmap_portable_deserialize_size((const char*)data, size),
        size);
    roaring_bitmap_t* r =
        roaring_bitmap_portable_deserialize_safe((const char*)data, size);
    assert_non_null(r);
    assert_true(roaring_bitmap_internal_validate(r, NULL));
    return r;
}

DEFINE_TEST(go_fuzzer_reproducers) {
    // clang-format off
    // {9<<16 | 0xE055, 15<<16 | 0xFFFC}
    const uint8_t two_keys[] = {0x3a, 0x30, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x09, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x1a, 0x00, 0x00, 0x00, 0x55, 0xe0, 0xfc, 0xff};
    // {9<<16 | 0xE055}
    const uint8_t first_key[] = {0x3a, 0x30, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x09, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x55, 0xe0};
    // run [3,6]
    const uint8_t run3to6[] = {0x3b, 0x30, 0x00, 0x00, 0x01, 0x00, 0x00, 0x03,
        0x00, 0x01, 0x00, 0x03, 0x00, 0x03, 0x00};
    // {1}
    const uint8_t one[] = {0x3a, 0x30, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00};
    // {0, 15<<16 | 0xFFFF}
    const uint8_t zero_and_top[] = {0x3a, 0x30, 0x00, 0x00, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
        0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff};
    // run [15<<16 | 0xFFF5, +3]
    const uint8_t run_fff5[] = {0x3b, 0x30, 0x00, 0x00, 0x01, 0x0f, 0x00, 0x03,
        0x00, 0x01, 0x00, 0xf5, 0xff, 0x03, 0x00};
    // {15<<16 | 0xFFF6, 15<<16 | 0xFFF7}
    const uint8_t two_inside[] = {0x3a, 0x30, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x0f, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0xf6, 0xff, 0xf7,
        0xff};
    // run [15<<16 | 0xFFE8, +3]
    const uint8_t run_ffe8[] = {0x3b, 0x30, 0x00, 0x00, 0x01, 0x0f, 0x00, 0x03,
        0x00, 0x01, 0x00, 0xe8, 0xff, 0x03, 0x00};
    // run [2,5]
    const uint8_t run2to5[] = {0x3b, 0x30, 0x00, 0x00, 0x01, 0x00, 0x00, 0x03,
        0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00};
    // run [54,57]
    const uint8_t run54to57[] = {0x3b, 0x30, 0x00, 0x00, 0x01, 0x00, 0x00, 0x03,
        0x00, 0x01, 0x00, 0x36, 0x00, 0x03, 0x00};
    // clang-format on
    roaring_bitmap_t *a, *b;

    // andnot empties the first container
    a = decode_valid(two_keys, sizeof(two_keys));
    b = decode_valid(first_key, sizeof(first_key));
    roaring_bitmap_andnot_inplace(a, b);
    assert_true(roaring_bitmap_internal_validate(a, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(a), 1);
    assert_true(roaring_bitmap_contains(a, (15u << 16) | 0xFFFC));
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);

    // or of a run with a value before it
    a = decode_valid(run3to6, sizeof(run3to6));
    b = decode_valid(one, sizeof(one));
    roaring_bitmap_or_inplace(a, b);
    assert_true(roaring_bitmap_internal_validate(a, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(a), 5);
    assert_true(roaring_bitmap_contains_range(a, 3, 7));
    assert_true(roaring_bitmap_contains(a, 1));
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);

    // xor with an equal copy empties the bitmap; or leaves it unchanged
    a = decode_valid(zero_and_top, sizeof(zero_and_top));
    b = decode_valid(zero_and_top, sizeof(zero_and_top));
    roaring_bitmap_or_inplace(a, b);
    assert_true(roaring_bitmap_internal_validate(a, NULL));
    assert_true(roaring_bitmap_equals(a, b));
    roaring_bitmap_xor_inplace(a, b);
    assert_true(roaring_bitmap_internal_validate(a, NULL));
    assert_true(roaring_bitmap_is_empty(a));
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);

    // andnot splits a run into two singletons
    a = decode_valid(run_fff5, sizeof(run_fff5));
    b = decode_valid(two_inside, sizeof(two_inside));
    roaring_bitmap_andnot_inplace(a, b);
    assert_true(roaring_bitmap_internal_validate(a, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(a), 2);
    assert_true(roaring_bitmap_contains(a, (15u << 16) | 0xFFF5));
    assert_true(roaring_bitmap_contains(a, (15u << 16) | 0xFFF8));
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);

    // flip the last value of a run
    a = decode_valid(run_ffe8, sizeof(run_ffe8));
    roaring_bitmap_flip_inplace(a, 1048555, 1048556);
    assert_true(roaring_bitmap_internal_validate(a, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(a), 3);
    assert_false(roaring_bitmap_contains(a, 1048555));
    roaring_bitmap_free(a);

    // remove inside a short run
    a = decode_valid(run2to5, sizeof(run2to5));
    roaring_bitmap_remove(a, 3);
    assert_true(roaring_bitmap_internal_validate(a, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(a), 3);
    roaring_bitmap_free(a);

    // add after a short run
    a = decode_valid(run54to57, sizeof(run54to57));
    roaring_bitmap_add(a, 112);
    assert_true(roaring_bitmap_internal_validate(a, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(a), 5);
    roaring_bitmap_free(a);
}

int main() {
    tellmeall();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_robust_deserialize1),
        cmocka_unit_test(test_robust_deserialize2),
        cmocka_unit_test(test_robust_deserialize3),
        cmocka_unit_test(test_robust_deserialize4),
        cmocka_unit_test(test_robust_deserialize5),
        cmocka_unit_test(test_robust_deserialize6),
        cmocka_unit_test(test_robust_deserialize7),
        cmocka_unit_test(deserialize_negative_container_count),
        cmocka_unit_test(deserialize_huge_container_count),
        cmocka_unit_test(deserialize_duplicate_keys),
        cmocka_unit_test(deserialize_unsorted_keys),
        cmocka_unit_test(deserialize_duplicate_array),
        cmocka_unit_test(deserialize_unsorted_array),
        cmocka_unit_test(deserialize_bitset_incorrect_cardinality),
        cmocka_unit_test(deserialize_run_container_empty),
        cmocka_unit_test(deserialize_run_container_should_combine),
        cmocka_unit_test(deserialize_run_container_overlap),
        cmocka_unit_test(deserialize_run_container_overflow),
        cmocka_unit_test(
            deserialize_run_container_incorrect_cardinality_still_allowed),
        cmocka_unit_test(test_robust_deserialize8),
        cmocka_unit_test(test_robust_deserialize_invalid_cookie),
        cmocka_unit_test(test_robust_deserialize_without_cookie),
        cmocka_unit_test(test_robust_deserialize_bad_bitmap),
        cmocka_unit_test(deserialize_run_container_end_boundary),
        cmocka_unit_test(deserialize_offsets_are_ignored),
        cmocka_unit_test(deserialize_trailing_bytes),
        cmocka_unit_test(deserialize_non_minimal_run_containers),
        cmocka_unit_test(serialize_empty_golden_bytes),
        cmocka_unit_test(serialize_no_offset_threshold_golden_bytes),
        cmocka_unit_test(serialize_array_bitset_boundary_golden_bytes),
        cmocka_unit_test(serialize_small_container_counts),
        cmocka_unit_test(deserialize_back_to_back),
        cmocka_unit_test(deserialize_unaligned_source),
        cmocka_unit_test(deserialize_truncation_sweep),
#if !CROARING_IS_BIG_ENDIAN
        cmocka_unit_test(frozen_view_too_many_containers),
        cmocka_unit_test(frozen_view_negative_cases),
        cmocka_unit_test(frozen_view_corrupted_payload),
#endif
        cmocka_unit_test(go_fuzzer_reproducers),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
