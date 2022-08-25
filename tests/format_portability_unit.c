/*
 * format_portability_unit.c
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/roaring.h>
#include <roaring/misc/configreport.h>

#include "config.h"

#include "test.h"


long filesize(char const* path) {
    FILE* fp = fopen(path, "rb");
    assert_non_null(fp);

    assert_int_not_equal(fseek(fp, 0L, SEEK_END), -1);

    return ftell(fp);
}

char* readfile(char const* path) {
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

int compare(char* x, char* y, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (x[i] != y[i]) {
            return i + 1;
        }
    }
    return 0;
}

void test_deserialize(char* filename) {
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

int main() {
    tellmeall();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_deserialize_portable_norun),
        cmocka_unit_test(test_deserialize_portable_wrun),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
