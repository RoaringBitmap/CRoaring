/*
 * robust_deserialization_unit.c
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


long filesize(FILE* fp) {
    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}

char* readfile(FILE* fp, size_t * bytes) {
    *bytes = filesize(fp);
    char* buf = (char*)malloc(*bytes);
    if(buf == NULL) return NULL;

    rewind(fp);

    size_t cnt = fread(buf, 1, *bytes, fp);
    if(*bytes != cnt){
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

int test_deserialize(const char * filename) {
    FILE* fp = fopen(filename, "rb");
    if(fp == NULL) {
        printf("Could not open %s, check your configuration. \n",filename);
        assert_false(fp == NULL);
    }
    size_t bytes;
    char* input_buffer = readfile(fp, &bytes);

    if(input_buffer == NULL) {
      printf("Could not read bytes from %s, check your configuration. \n",filename);
      assert_false(input_buffer == NULL);
    }
    printf("Binary content read.\n");


    roaring_bitmap_t* bitmap =
        roaring_bitmap_portable_deserialize_safe(input_buffer, bytes);

    if(bitmap == NULL) {
        printf("Null bitmap loaded.\n");
        free(input_buffer);
        return 1; // this is the expected behavior
    }
    printf("Non-null bitmap loaded.\n");

    size_t expected_size = roaring_bitmap_portable_size_in_bytes(bitmap);

    char* output_buffer = (char*)malloc(expected_size);
    size_t actual_size =
        roaring_bitmap_portable_serialize(bitmap, output_buffer);


    if(actual_size != expected_size) {
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
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "crashproneinput1.bin");

    test_deserialize(filename);
}


DEFINE_TEST(test_robust_deserialize2) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "crashproneinput2.bin");

    test_deserialize(filename);
}


DEFINE_TEST(test_robust_deserialize3) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "crashproneinput3.bin");

    test_deserialize(filename);
}

DEFINE_TEST(test_robust_deserialize4) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "crashproneinput4.bin");

    test_deserialize(filename);
}

DEFINE_TEST(test_robust_deserialize5) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "crashproneinput5.bin");

    test_deserialize(filename);
}

DEFINE_TEST(test_robust_deserialize6) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "crashproneinput6.bin");

    test_deserialize(filename);
}

DEFINE_TEST(test_robust_deserialize7) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "crashproneinput7.bin");

    test_deserialize(filename);
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
     };

    return cmocka_run_group_tests(tests, NULL, NULL);
#endif
}
