/*
 * bitset.c
 *
 */

#include "bitset.h"

enum{BITSET_CONTAINER_SIZE_IN_WORDS = (1 << 16) / 64};

/* Create a new bitset. Return NULL in case of failure. */
bitset_container_t *bitset_container_create() {
  bitset_container_t *bitset = NULL;
  /* Allocate the bitset itself. */
  if( ( bitset = malloc( sizeof( bitset_container_t ) ) ) == NULL ) {
      return NULL;
  }
  if ((bitset->array = (uint64_t *) malloc(sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS)) == NULL) {
    free( bitset);
    return NULL;
  }
  memset(bitset->array,0,sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
  bitset-> cardinality = 0;
  return bitset;
}

/* Free memory. */
void bitset_container_free(bitset_container_t *bitset) {
  free(bitset->array);
  free(bitset);
}

/* Set the ith bit.  */
void bitset_container_set(bitset_container_t *bitset,  uint16_t i ) {
  uint64_t oldw = bitset->array[i >> 6];
  uint64_t neww =  oldw |(UINT64_C(1) << (i & 63));
  bitset->cardinality += (oldw != neww);
  bitset->array[i >> 6] = neww;
}

/* Unset the ith bit.  */
void bitset_container_unset(bitset_container_t *bitset,  uint16_t i ) {
  uint64_t oldw = bitset->array[i >> 6];
  uint64_t neww =  oldw & (~(UINT64_C(1) << (i & 63)));
  bitset->cardinality -= (oldw != neww);
  bitset->array[i >> 6] = neww;
}

/* Get the value of the ith bit.  */
int bitset_container_get(bitset_container_t *bitset,  uint16_t i ) {
  uint64_t w =  bitset->array[i >> 6];
  return (w >> (i & 63) ) & 1; // getting rid of the mask can shave one cycle off...
}


/* Get the number of bits set (force computation) */
int bitset_container_compute_cardinality(bitset_container_t *bitset) {
	int sum = 0;
	for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k++) {
		sum += __builtin_popcountl(bitset->array[k]);
	}
	return sum;
}





