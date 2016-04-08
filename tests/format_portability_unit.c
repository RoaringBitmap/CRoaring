/*
 * format_portability_unit.c
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "misc/configreport.h"
#include "roaring.h"

#include "test.h"

long filesize(char const* path) {
    FILE* fp = fopen(path, "rb");
    if (NULL == fp) {
        fprintf(stderr, "Could not open.\n");
        return 0;
    }
    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}

char* readfile(char const* path) {
    printf("loading file %s\n", path);
    FILE* fp = fopen(path, "rb");
    if (NULL == fp) {
        fprintf(stderr, "Could not open.\n");
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    long bytes = ftell(fp);
    char* buf = malloc(bytes);
    if (NULL == buf) {
        fprintf(stderr, "Allocation failure.\n");
        return NULL;
    }
    rewind(fp);
    if (bytes != (long)fread(buf, 1, bytes, fp)) {
        fprintf(stderr, "Failure while reading file.\n");
        free(buf);
        fclose(fp);
        return NULL;
    }
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

int test_deserialize(char* filename) {
    char* buf = readfile(filename);
    assert(buf != NULL);

    roaring_bitmap_t* bitmap = roaring_bitmap_portable_deserialize(buf);
    size_t sizeinbytes = roaring_bitmap_portable_size_in_bytes(bitmap);

    if ((long)sizeinbytes != filesize(filename)) {
        fprintf(stderr, "Bitmap size mismatch, expected %d but got %d.\n",
                (int)filesize(filename), (int)sizeinbytes);
        return 0;
    }
    char* tmpbuf = malloc(sizeinbytes);
    size_t newsize = roaring_bitmap_portable_serialize(bitmap, tmpbuf);

    if (newsize != sizeinbytes) {
        fprintf(stderr, "Bitmap size change, expected %d but got %d.\n",
                (int)newsize, (int)sizeinbytes);
        return 0;
    }
    if (compare(buf, tmpbuf, newsize)) {
        fprintf(stderr,
                "We do not serialize to the same content. First difference "
                "found at %d. \n",
                compare(buf, tmpbuf, newsize) - 1);
        return 0;
    }
    free(tmpbuf);
    free(buf);
    roaring_bitmap_free(bitmap);
    printf("\n");
    return 1;
}

int test_deserialize_portable_norun() {
    DESCRIBE_TEST;

    char filename[1024];
    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "bitmapwithoutruns.bin");
    return test_deserialize(filename);
}

int test_deserialize_portable_wrun() {
    DESCRIBE_TEST;

    char filename[1024];
    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "bitmapwithruns.bin");
    return test_deserialize(filename);
}

int main() {
    tellmeall();
    if (!test_deserialize_portable_norun()) return -1;
    if (!test_deserialize_portable_wrun()) return -1;

    return EXIT_SUCCESS;
}
