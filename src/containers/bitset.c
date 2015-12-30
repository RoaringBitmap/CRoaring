/*
 * bitset.c
 *
 */

#include "bitset.h"

/* Create a new bitset. Return NULL in case of failure. */
bitset_container_t *bitset_container_create() {
  bitset_container_t *bitset = NULL;
  /* Allocate the bitset itself. */
  if( ( bitset = malloc( sizeof( bitset_container_t ) ) ) == NULL ) {
      return NULL;
  }
  size_t sizeinwords = (1 << 16) / 64; // this is fixed
  if ((bitset->array = (uint64_t *) malloc(sizeof(uint64_t) * sizeinwords)) == NULL) {
    free( bitset);
    return NULL;
  }
  memset(bitset->array,0,sizeof(uint64_t) * sizeinwords);
  return bitset;
}

/* Free memory. */
void bitset_container_free(bitset_container_t *bitset) {
  free(bitset->array);
  free(bitset);
}

/* Set the ith bit.  */
void bitset_container_set(bitset_container_t *bitset,  uint16_t i ) {
  bitset->array[i >> 6] |= ((uint64_t)1) << (i % 64);
}


/* Get the value of the ith bit.  */
int bitset_container_get(bitset_container_t *bitset,  uint16_t i ) {
  uint64_t w =  bitset->array[i >> 6];
  //return _bittest64(w,i % 64);
  return ( w & ( ((uint64_t)1) << (i % 64))) != 0 ;
}





