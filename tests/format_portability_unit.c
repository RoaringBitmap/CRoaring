/*
 * format_portability_unit.c
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "roaring.h"
#include "misc/configreport.h"

long filesize(char const* path) {
	FILE  * fp = fopen(path, "rb");
    if( NULL == fp ) {
    	fprintf(stderr, "Could not open.\n");
        return 0;
    }
    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}

char * readfile(char const* path) {
	printf("loading file %s\n",path);
    FILE  * fp = fopen(path, "rb");
    if( NULL == fp ) {
    	fprintf(stderr, "Could not open.\n");
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    long bytes = ftell(fp);
    char *buf = malloc( bytes );
    if( NULL == buf ) {
    	fprintf(stderr, "Allocation failure.\n");
    	return NULL;
    }
    rewind(fp);
    if(bytes != (long) fread(buf, 1, bytes, fp)) {
    	fprintf(stderr, "Failure while reading file.\n");
    	free(buf);
        fclose(fp);
    	return NULL;
    }
    fclose(fp);
    return buf;
}

int test_deserialize_portable_norun() {
    printf("[%s] %s\n", __FILE__, __func__);
    char filename[1024];
    strcpy (filename,TEST_DATA_DIR);
    strcat (filename,"bitmapwithoutruns.bin");
    char * buf = readfile(filename);
    assert(buf != NULL);
    roaring_bitmap_t * bitmap = roaring_bitmap_portable_deserialize(buf);
    free(buf);
    size_t sizeinbytes = roaring_bitmap_portable_size_in_bytes(bitmap);
    if((long) sizeinbytes != filesize(filename)) {
    	fprintf(stderr, "Bitmap size mismatch, expected %d but got %d.\n",(int)filesize(filename),(int)sizeinbytes);
    	return 0;
    }
    roaring_bitmap_free(bitmap);
    printf("\n");
    return 1;
}

int test_deserialize_portable_wrun() {
    printf("[%s] %s\n", __FILE__, __func__);
    char filename[1024];
    strcpy (filename,TEST_DATA_DIR);
    strcat (filename,"bitmapwithruns.bin");
    char * buf = readfile(filename);
    assert(buf != NULL);
    roaring_bitmap_t * bitmap = roaring_bitmap_portable_deserialize(buf);
    free(buf);
    size_t sizeinbytes = roaring_bitmap_portable_size_in_bytes(bitmap);
    if((long) sizeinbytes != filesize(filename)) {
    	fprintf(stderr, "Bitmap size mismatch, expected %d but got %d.\n",(int)filesize(filename),(int)sizeinbytes);
    	return 0;
    }
    roaring_bitmap_free(bitmap);
    printf("\n");
    return 1;
}

int main() {
    tellmeall();
    if (!test_deserialize_portable_norun()) return -1;
    if (!test_deserialize_portable_wrun()) return -1;

    printf("[%s] your code might be ok.\n", __FILE__);
    return 0;
}



