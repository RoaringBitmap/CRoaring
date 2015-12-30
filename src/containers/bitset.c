/*
 * bitset.c
 *
 */

#include "bitset.h"
#include <assert.h>
#include <nmmintrin.h>

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
    int32_t sum1 = 0;
    uint64_t * a = bitset->array;
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k++) {
        sum1 += _mm_popcnt_u64(a[k]);
    }
    return sum1;
}

/* computes the union of bitset1 and bitset2 and write the result to bitsetout */
int bitset_container_or(bitset_container_t *bitset1, bitset_container_t *bitset2, bitset_container_t *bitsetout) {
    uint64_t * a1 = bitset1->array;
    uint64_t * a2 = bitset2->array;
    uint64_t * ao = bitsetout->array;
    int32_t cardinality = 0;
    assert((BITSET_CONTAINER_SIZE_IN_WORDS & 3) == 0);
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k++) {
        uint64_t w = a1[k] | a2[k];
        ao[k] = w;
        cardinality += _mm_popcnt_u64(w);
    }
    bitsetout->cardinality = cardinality;
    return bitsetout->cardinality;
}

/* computes the union of bitset1 and bitset2 and write the result to bitsetout, does not compute the cardinality of the result */
int bitset_container_or_nocard(bitset_container_t *bitset1, bitset_container_t *bitset2, bitset_container_t *bitsetout) {
    uint64_t * a1 = bitset1->array;
    uint64_t * a2 = bitset2->array;
    uint64_t * ao = bitsetout->array;
    assert((BITSET_CONTAINER_SIZE_IN_WORDS & 3) == 0);
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k++) {
        uint64_t w = a1[k] | a2[k];
        ao[k] = w;
    }
    bitsetout->cardinality = -1;
    return bitsetout->cardinality;
}


/* computes the intersection of bitset1 and bitset2 and write the result to bitsetout */
int bitset_container_and(bitset_container_t *bitset1, bitset_container_t *bitset2, bitset_container_t *bitsetout) {
    uint64_t * a1 = bitset1->array;
    uint64_t * a2 = bitset2->array;
    uint64_t * ao = bitsetout->array;
    int32_t cardinality = 0;

    assert((BITSET_CONTAINER_SIZE_IN_WORDS & 3) == 0);
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k++) {
        uint64_t w1 = a1[k] & a2[k];
        ao[k] = w1;
        cardinality += _mm_popcnt_u64(w1);
    }
    bitsetout->cardinality = cardinality;
    return bitsetout->cardinality;
}

/* computes the intersection of bitset1 and bitset2 and write the result to bitsetout, does not compute the cardinality of the result */
int bitset_container_and_nocard(bitset_container_t *bitset1, bitset_container_t *bitset2, bitset_container_t *bitsetout) {
    uint64_t * a1 = bitset1->array;
    uint64_t * a2 = bitset2->array;
    uint64_t * ao = bitsetout->array;
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k++) {
        uint64_t w = a1[k] & a2[k];
        ao[k] = w;
    }
    bitsetout->cardinality = -1;
    return bitsetout->cardinality;
}

