/*
 * bitset_container_unit.c
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "containers/bitset.h"


int set_get_test() {
	bitset_container_t * B = bitset_container_create();
	int x;
	printf("[%s] %s\n",__FILE__,__func__);
	if(B == NULL) {
		printf("Bug %s, line %d \n", __FILE__, __LINE__);
		return 0;
	}
	for(x = 0; x< 1<<16; x+=3) {
		bitset_container_set(B,(uint16_t)x);
	}
	for(x = 0; x< 1<<16; x++) {
		int isset = bitset_container_get(B,(uint16_t)x);
		int shouldbeset = (x/3*3==x);
		if(isset != shouldbeset) {
			printf("Bug %s, line %d \n", __FILE__, __LINE__);
			bitset_container_free(B);
			return 0;
		}
	}
	bitset_container_free(B);
	return 1;
}



int main() {
	if(!set_get_test()) return -1;
	printf("[%s] your code might be ok.\n",__FILE__);
	return 0;
}
