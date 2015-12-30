/*
 * bitset.h
 *
 */

#ifndef INCLUDE_CONTAINERS_BITSET_H_
#define INCLUDE_CONTAINERS_BITSET_H_


#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

struct bitset_container_s {
	int32_t cardinality;
    uint64_t *array;
};

typedef struct bitset_container_s bitset_container_t;


/* Create a new bitset. Return NULL in case of failure. */
bitset_container_t *bitset_container_create();

/* Free memory. */
void bitset_container_free(bitset_container_t *bitset);

/* Set the ith bit.  */
void bitset_container_set(bitset_container_t *bitset,  uint16_t i );


/* Unset the ith bit.  */
void bitset_container_unset(bitset_container_t *bitset,  uint16_t i );

/* Get the value of the ith bit.  */
int bitset_container_get(bitset_container_t *bitset,  uint16_t i );


/* Get the number of bits set */
inline int bitset_container_cardinality(bitset_container_t *bitset) {
  return  bitset->cardinality;
}

/* computes the union of bitset1 and bitset2 and write the result to bitsetout */
int bitset_container_or(bitset_container_t *bitset1, bitset_container_t *bitset2, bitset_container_t *bitsetout);

/* computes the union of bitset1 and bitset2 and write the result to bitsetout, does not compute the cardinality of the result */
int bitset_container_or_nocard(bitset_container_t *bitset1, bitset_container_t *bitset2, bitset_container_t *bitsetout);


/* Get the number of bits set (force computation) */
int bitset_container_compute_cardinality(bitset_container_t *bitset);

#endif /* INCLUDE_CONTAINERS_BITSET_H_ */
