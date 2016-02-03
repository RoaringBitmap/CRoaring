/*
 * bitset_container_unit.c
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "containers/bitset.h"
#include "benchmark.h"
#include "random.h"


// flushes the array of words from cache
void bitset_cache_flush(bitset_container_t* B) {
	const int32_t CACHELINESIZE = computecacheline();// 64 bytes per cache line
	for(int32_t  k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k += CACHELINESIZE/sizeof(uint64_t)) {
		__builtin_ia32_clflush(B->array + k);
	}
}

// tries to put array of words in cache
void bitset_cache_prefetch(bitset_container_t* B) {
	const int32_t CACHELINESIZE = computecacheline();// 64 bytes per cache line
	for(int32_t  k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k += CACHELINESIZE/sizeof(uint64_t)) {
		__builtin_prefetch(B->array + k);
	}
}


int extract_test(bitset_container_t* B) {
	int card = bitset_container_cardinality(B);
	uint32_t *out = malloc(sizeof(uint32_t) * card);
	bitset_container_to_uint32_array(out,B,1234);
	free(out);
	return card;
}



int set_test(bitset_container_t* B) {
    int x;
    for (x = 0; x < 1 << 16; x += 3) {
        bitset_container_set(B, (uint16_t)x);
    }
    return 0;
}

int unset_test(bitset_container_t* B) {
    int x;
    for (x = 0; x < 1 << 16; x += 3) {
        bitset_container_unset(B, (uint16_t)x);
    }
    return 0;
}

int get_test(bitset_container_t* B) {
    int card = 0;
    int x;
    for (x = 0; x < 1 << 16; x++) {
        card += bitset_container_get(B, (uint16_t)x);
    }
    return card;
}

int main() {
    int repeat = 500;
    int size = (1 << 16) / 3;
    tellmeall();
    printf("bitset container benchmarks\n");
    bitset_container_t* B = bitset_container_create();
    BEST_TIME(set_test(B), 0, repeat, size);
    int answer = get_test(B);
    size = 1 << 16;
    BEST_TIME(get_test(B), answer, repeat, size);
    BEST_TIME(bitset_container_cardinality(B), answer, repeat, 1);
    BEST_TIME(bitset_container_compute_cardinality(B), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);

    size = (1 << 16) / 3;
    BEST_TIME(unset_test(B), 0, repeat, size);
    bitset_container_free(B);

    for(int howmany = 4096; howmany <= (1<<16); howmany *= 2) {
    	bitset_container_t* Bt = bitset_container_create();
        while(bitset_container_cardinality(Bt) < howmany) {
        	bitset_container_set(Bt, (uint16_t)pcg32_random() );
        }
        size_t nbrtestvalues = 1024;
        uint16_t * testvalues = malloc(nbrtestvalues * sizeof(uint16_t) );
        printf("\n number of values in container = %d\n",bitset_container_cardinality(Bt));
    	int card = bitset_container_cardinality(Bt);
    	uint32_t *out = malloc(sizeof(uint32_t) * card + sizeof(__m256i));
        BEST_TIME(bitset_container_to_uint32_array(out,Bt,1234), card, repeat, card);
    	free(out);
        BEST_TIME_PRE_ARRAY(Bt,bitset_container_get, bitset_cache_prefetch, testvalues, nbrtestvalues);
        BEST_TIME_PRE_ARRAY(Bt,bitset_container_get, bitset_cache_flush, testvalues, nbrtestvalues);
        free(testvalues);
        bitset_container_free(Bt);
    }
    printf("\n");

    bitset_container_t* B1 = bitset_container_create();
    for (int x = 0; x < 1 << 16; x += 3) {
        bitset_container_set(B1, (uint16_t)x);
    }
    bitset_container_t* B2 = bitset_container_create();
    for (int x = 0; x < 1 << 16; x += 5) {
        bitset_container_set(B2, (uint16_t)x);
    }
    bitset_container_t* BO = bitset_container_create();
    BEST_TIME(bitset_container_or_nocard(B1, B2, BO), -1, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    answer = bitset_container_compute_cardinality(BO);
    BEST_TIME(bitset_container_or(B1, B2, BO), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    BEST_TIME(bitset_container_cardinality(BO), answer, repeat, 1);
    BEST_TIME(bitset_container_compute_cardinality(BO), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    BEST_TIME(bitset_container_and_nocard(B1, B2, BO), -1, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    answer = bitset_container_compute_cardinality(BO);
    BEST_TIME(bitset_container_and(B1, B2, BO), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);
    BEST_TIME(bitset_container_cardinality(BO), answer, repeat, 1);
    BEST_TIME(bitset_container_compute_cardinality(BO), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);

    return 0;
}
