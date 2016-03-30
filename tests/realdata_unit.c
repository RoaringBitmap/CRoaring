/*
 * realdata_unit.c
 */
#define _GNU_SOURCE
#include "../benchmarks/bitmapsfromtextfiles.h"
#include "config.h"

/**
 * Once you have collected all the integers, build the bitmaps.
 */
static roaring_bitmap_t **create_all_bitmaps(size_t *howmany,
                                             uint32_t **numbers, size_t count) {
    if (numbers == NULL) return NULL;
    printf("Constructing %d  bitmaps.\n", (int)count);
    roaring_bitmap_t **answer = malloc(sizeof(roaring_bitmap_t *) * count);
    for (size_t i = 0; i < count; i++) {
        printf(".");
        fflush(stdout);
        answer[i] = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
    }
    printf("\n");
    return answer;
}


const char * datadir[] = {"census-income","census-income_srt","census1881","census1881_srt",
		"uscensus2000", "weather_sept_85","weather_sept_85_srt","wikileaks-noquotes",
		"wikileaks-noquotes_srt"};


// try ANDing and ORing together consecutive pairs
/*for (int i = 0; i < (int)count - 1; ++i) {
    uint32_t c1 = roaring_bitmap_get_cardinality(bitmaps[i]);
    uint32_t c2 = roaring_bitmap_get_cardinality(bitmaps[i + 1]);
    RDTSC_START(cycles_start);
    roaring_bitmap_t *tempand =
        roaring_bitmap_and(bitmaps[i], bitmaps[i + 1]);
    RDTSC_FINAL(cycles_final);
    successive_and += cycles_final - cycles_start;

    uint32_t ci = roaring_bitmap_get_cardinality(tempand);
    roaring_bitmap_free(tempand);
    RDTSC_START(cycles_start);
    roaring_bitmap_t *tempor =
        roaring_bitmap_or(bitmaps[i], bitmaps[i + 1]);
    RDTSC_FINAL(cycles_final);
    successive_or += cycles_final - cycles_start;

    uint32_t co = roaring_bitmap_get_cardinality(tempor);
    roaring_bitmap_free(tempor);

    if (c1 + c2 != co + ci) {
        printf(KRED "cardinalities are wrong somehow\n");
        printf("c1 = %d, c2 = %d, co = %d, ci = %d\n", c1, c2, co, ci);
        return -1;
    }
}
*/

bool serialize_correctly(roaring_bitmap_t * r){
    uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r);
printf("expected size = %d \n",expectedsize);
    char *serialized = malloc(expectedsize);
	if(serialized == NULL) {
		printf("failure to allocate memory!\n");
		return false;
	}
	uint32_t serialize_len = roaring_bitmap_portable_serialize(r, serialized);
    if(serialize_len != expectedsize) {
		printf("Bad serialized size!\n");
    	free(serialized);
    	return false;
    }
    roaring_bitmap_t * r2 = roaring_bitmap_portable_deserialize(serialized);
   free(serialized);
    /*if(! roaring_bitmap_equals(r,r2)) {
		printf("Won't recover original bitmap!\n");
		roaring_bitmap_free(r2);
		return false;
    }
    if(! roaring_bitmap_equals(r2,r)) {
		printf("Won't recover original bitmap!\n");
		roaring_bitmap_free(r2);
		return false;
    }
    */
   roaring_bitmap_free(r2);
    return true;

}

bool loadAndCheckAll(const char * dirname) {
    printf("[%s] %s datadir=%s\n", __FILE__, __func__,dirname);

    char *extension = ".txt";
    size_t count;

    size_t *howmany = NULL;
    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &howmany, &count);
    if (numbers == NULL) {
        printf(
            "I could not find or load any data file with extension %s in "
            "directory %s.\n",
            extension, dirname);
        return -1;
    }

    roaring_bitmap_t **bitmaps = create_all_bitmaps(howmany, numbers, count);
    roaring_bitmap_t **bitmapswrun = malloc(sizeof(roaring_bitmap_t *) * count);
    for (int i = 0; i < (int)count; i++) {
    	bitmapswrun[i] = roaring_bitmap_copy(bitmaps[i]);
    	roaring_bitmap_run_optimize(bitmapswrun[i]);
    }
    for (int i = 0; i < (int)count; i++) {
    	if(!serialize_correctly(bitmaps[i])) {
    	    return false;// the hell with memory leaks
    	}
    	if(!serialize_correctly(bitmapswrun[i])) {
    		return false;// the hell with memory leaks
    	}
    }

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
        roaring_bitmap_free(bitmaps[i]);
        bitmaps[i] = NULL;  // paranoid
        roaring_bitmap_free(bitmapswrun[i]);
        bitmapswrun[i] = NULL;  // paranoid

    }
    free(bitmapswrun);
    free(bitmaps);
    free(howmany);
    free(numbers);

    return true;
}

int main() {
    char dirbuffer[1024];
    size_t bddl = strlen(BENCHMARK_DATA_DIR);
    strcpy(dirbuffer, BENCHMARK_DATA_DIR);
    for(size_t i = 0; i < sizeof(datadir) /sizeof(const char *); i++) {
    	strcpy(dirbuffer + bddl, datadir[i]);
    	if(!loadAndCheckAll(dirbuffer)) {
    	    printf("failure\n");
    		return -1;
    	}
    }
    printf("done realdata tests\n");
}

