/*
 * bitset_container_unit.c
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "containers/bitset.h"
#include "benchmark.h"


int set_test(bitset_container_t * B) {
	int x;
	for(x = 0; x< 1<<16; x+=3) {
		bitset_container_set(B,(uint16_t)x);
	}
	return 0;
}

int unset_test(bitset_container_t * B) {
	int x;
	for(x = 0; x< 1<<16; x+=3) {
		bitset_container_unset(B,(uint16_t)x);
	}
	return 0;
}
int get_test(bitset_container_t * B) {
	int card = 0;
	int x;
	for(x = 0; x< 1<<16; x++) {
		card += bitset_container_get(B,(uint16_t)x);
	}
	return card;
}



int main() {
	int repeat = 5000;
	int size = (1<<16)/3;
	printf("bitset container benchmarks\n");
	bitset_container_t * B = bitset_container_create();
	BEST_TIME(set_test(B), 0, repeat, size);
	int answer = get_test(B);
	size = 1 << 16;
	BEST_TIME(get_test(B), answer, repeat, size);
	BEST_TIME(bitset_container_cardinality(B), answer, repeat, 1);
	BEST_TIME(bitset_container_compute_cardinality(B), answer, repeat, 1);

	size = (1<<16)/3;
	BEST_TIME(unset_test(B), 0, repeat, size);
	bitset_container_free(B);

	bitset_container_t * B1 = bitset_container_create();
	for(int x = 0; x< 1<<16; x+=3) {
		bitset_container_set(B1,(uint16_t)x);
	}
	bitset_container_t * B2 = bitset_container_create();
	for(int x = 0; x< 1<<16; x+=5) {
		bitset_container_set(B2,(uint16_t)x);
	}
	bitset_container_t * BO = bitset_container_create();
	BEST_TIME(bitset_container_or_nocard(B1,B2,BO), -1, repeat, 1);
	answer = bitset_container_compute_cardinality(BO);
	BEST_TIME(bitset_container_or(B1,B2,BO), answer, repeat, 1);
	BEST_TIME(bitset_container_cardinality(BO), answer, repeat, 1);
	BEST_TIME(bitset_container_compute_cardinality(BO), answer, repeat, 1);
	return 0;
}
