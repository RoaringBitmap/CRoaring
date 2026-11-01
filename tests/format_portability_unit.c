/*
 * format_portability_unit.c
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

static inline long filesize(char const* path) {
    FILE* fp = fopen(path, "rb");
    assert_non_null(fp);

    assert_int_not_equal(fseek(fp, 0L, SEEK_END), -1);

    return ftell(fp);
}

static inline char* readfile(char const* path) {
    FILE* fp = fopen(path, "rb");
    assert_non_null(fp);

    assert_int_not_equal(fseek(fp, 0L, SEEK_END), -1);

    long bytes = ftell(fp);
    char* buf = (char*)malloc(bytes);
    assert_non_null(buf);

    rewind(fp);
    assert_int_equal(bytes, fread(buf, 1, bytes, fp));

    fclose(fp);
    return buf;
}

static inline int compare(char* x, char* y, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (x[i] != y[i]) {
            return i + 1;
        }
    }
    return 0;
}

static inline void test_deserialize(char* filename) {
    char* input_buffer = readfile(filename);
    assert_non_null(input_buffer);

    roaring_bitmap_t* bitmap =
        roaring_bitmap_portable_deserialize(input_buffer);
    assert_non_null(bitmap);

    size_t expected_size = roaring_bitmap_portable_size_in_bytes(bitmap);

    assert_int_equal(expected_size, filesize(filename));

    char* output_buffer = (char*)malloc(expected_size);
    size_t actual_size =
        roaring_bitmap_portable_serialize(bitmap, output_buffer);

    assert_int_equal(actual_size, expected_size);
    assert_false(compare(input_buffer, output_buffer, actual_size));

    free(output_buffer);
    free(input_buffer);
    roaring_bitmap_free(bitmap);
}

DEFINE_TEST(test_deserialize_portable_norun) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "bitmapwithoutruns.bin");

    test_deserialize(filename);
}

DEFINE_TEST(test_deserialize_portable_wrun) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "bitmapwithruns.bin");

    test_deserialize(filename);
}

// Content of bitmapwithruns.bin / bitmapwithoutruns.bin (Java
// SerializeToDiskExample): {k%1000==0, k<100000} U {3k: 100000<=k<200000}
// U [700000, 800000).
static roaring_bitmap_t* expected_shared_bitmap(void) {
    roaring_bitmap_t* r = roaring_bitmap_create();
    for (uint32_t k = 0; k < 100000; k += 1000) roaring_bitmap_add(r, k);
    for (uint32_t k = 100000; k < 200000; k++) roaring_bitmap_add(r, 3 * k);
    roaring_bitmap_add_range(r, 700000, 800000);
    return r;
}

static void check_shared_file_content(const char* name, uint32_t runs) {
    char filename[1024];
    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, name);
    char* buf = readfile(filename);
    size_t size = (size_t)filesize(filename);
    roaring_bitmap_t* r = roaring_bitmap_portable_deserialize_safe(buf, size);
    assert_non_null(r);
    assert_true(roaring_bitmap_internal_validate(r, NULL));
    roaring_bitmap_t* expected = expected_shared_bitmap();
    assert_int_equal(roaring_bitmap_get_cardinality(r), 200100);
    assert_true(roaring_bitmap_equals(r, expected));
    roaring_statistics_t stats;
    roaring_bitmap_statistics(r, &stats);
    assert_int_equal(stats.n_containers, 11);
    assert_int_equal(stats.n_run_containers, runs);
    // run-optimizing the expected bitmap yields the "with runs" file
    if (runs > 0) {
        roaring_bitmap_run_optimize(expected);
        assert_int_equal(roaring_bitmap_portable_size_in_bytes(expected), size);
        char* out = (char*)malloc(size);
        roaring_bitmap_portable_serialize(expected, out);
        assert_false(compare(buf, out, size));
        free(out);
    }
    roaring_bitmap_free(expected);
    roaring_bitmap_free(r);
    free(buf);
}

DEFINE_TEST(test_shared_files_content) {
    check_shared_file_content("bitmapwithoutruns.bin", 0);
    check_shared_file_content("bitmapwithruns.bin", 3);
}

// Cross-implementation frozen/portable files made with gocroaring
// (roaring/testfrozendata).
static void check_frozen_golden(const char* name, uint64_t cardinality,
                                uint32_t bitsets, uint32_t arrays,
                                uint32_t runs) {
    char filename[1024];
    strcpy(filename, TEST_DATA_DIR "testfrozendata/");
    strcat(filename, name);
    strcat(filename, ".portable");
    char* portable = readfile(filename);
    size_t portable_size = (size_t)filesize(filename);
    roaring_bitmap_t* r =
        roaring_bitmap_portable_deserialize_safe(portable, portable_size);
    assert_non_null(r);
    assert_true(roaring_bitmap_internal_validate(r, NULL));
    assert_int_equal(roaring_bitmap_get_cardinality(r), cardinality);
    roaring_statistics_t stats;
    roaring_bitmap_statistics(r, &stats);
    assert_int_equal(stats.n_bitset_containers, bitsets);
    assert_int_equal(stats.n_array_containers, arrays);
    assert_int_equal(stats.n_run_containers, runs);
    assert_int_equal(roaring_bitmap_portable_size_in_bytes(r), portable_size);
    char* out = (char*)malloc(portable_size);
    assert_int_equal(roaring_bitmap_portable_serialize(r, out), portable_size);
    assert_false(compare(portable, out, portable_size));
    free(out);

#if !CROARING_IS_BIG_ENDIAN
    // The frozen format is native-endian; the files are little-endian.
    strcpy(filename, TEST_DATA_DIR "testfrozendata/");
    strcat(filename, name);
    strcat(filename, ".frozen");
    char* frozen_file = readfile(filename);
    size_t frozen_size = (size_t)filesize(filename);
    assert_int_equal(roaring_bitmap_frozen_size_in_bytes(r), frozen_size);
    char* frozen = (char*)roaring_aligned_malloc(32, frozen_size);
    memcpy(frozen, frozen_file, frozen_size);
    const roaring_bitmap_t* view =
        roaring_bitmap_frozen_view(frozen, frozen_size);
    assert_non_null(view);
    assert_true(roaring_bitmap_internal_validate(view, NULL));
    assert_true(roaring_bitmap_equals(view, r));
    roaring_bitmap_free(view);
    char* reserialized = (char*)roaring_aligned_malloc(32, frozen_size);
    roaring_bitmap_frozen_serialize(r, reserialized);
    assert_false(compare(frozen_file, reserialized, frozen_size));
    roaring_aligned_free(reserialized);
    roaring_aligned_free(frozen);
    free(frozen_file);
#endif
    roaring_bitmap_free(r);
    free(portable);
}

DEFINE_TEST(test_frozen_golden_files) {
    check_frozen_golden("bitmaps_only", 65536, 2, 0, 0);
    check_frozen_golden("arrays_only", 132, 0, 2, 0);
    check_frozen_golden("runs_only", 122878, 0, 0, 2);
    check_frozen_golden("mixed", 188546, 2, 2, 2);
}

int main() {
    tellmeall();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_deserialize_portable_norun),
        cmocka_unit_test(test_deserialize_portable_wrun),
        cmocka_unit_test(test_shared_files_content),
        cmocka_unit_test(test_frozen_golden_files),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
