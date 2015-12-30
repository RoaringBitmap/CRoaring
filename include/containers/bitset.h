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
    uint64_t *array;
};

typedef struct bitset_container_s bitset_container_t;


/* Create a new bitset. Return NULL in case of failure. */
bitset_container_t *bitset_container_create();

/* Free memory. */
void bitset_container_free(bitset_container_t *bitset);

/* Set the ith bit.  */
void bitset_container_set(bitset_container_t *bitset,  uint16_t i );

/* Get the value of the ith bit.  */
int bitset_container_get(bitset_container_t *bitset,  uint16_t i );


#endif /* INCLUDE_CONTAINERS_BITSET_H_ */
