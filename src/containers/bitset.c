/*
 * bitset.c
 *
 */

#include <assert.h>
#include <x86intrin.h>
#include <stdlib.h>
#include <string.h>

#include "bitset.h"

#define USEAVX

/* Create a new bitset. Return NULL in case of failure. */
bitset_container_t *bitset_container_create() {
    bitset_container_t *bitset = NULL;
    /* Allocate the bitset itself. */
    if ((bitset = malloc(sizeof(bitset_container_t))) == NULL) {
        return NULL;
    }
    assert((BITSET_CONTAINER_SIZE_IN_WORDS & 3) == 0);
    if (posix_memalign((void **)&bitset->array, sizeof(__m256i),
                       sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS) !=
        0) {
        free(bitset);
        return NULL;
    }
    memset(bitset->array, 0, sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
    bitset->cardinality = 0;
    return bitset;
}

/* Free memory. */
void bitset_container_free(bitset_container_t *bitset) {
    free(bitset->array);
    free(bitset);
}

/* Set the ith bit.  */
void bitset_container_set(bitset_container_t *bitset, uint16_t i) {
    const uint64_t oldw = bitset->array[i >> 6];
    const uint64_t neww = oldw | (UINT64_C(1) << (i & 63));
    bitset->cardinality += (oldw != neww);
    bitset->array[i >> 6] = neww;
}

/* Unset the ith bit.  */
void bitset_container_unset(bitset_container_t *bitset, uint16_t i) {
    const uint64_t oldw = bitset->array[i >> 6];
    const uint64_t neww = oldw & (~(UINT64_C(1) << (i & 63)));
    bitset->cardinality -= (oldw != neww);
    bitset->array[i >> 6] = neww;
}

/* Get the value of the ith bit.  */
bool bitset_container_get(const bitset_container_t *bitset, uint16_t i) {
    const uint64_t w = bitset->array[i >> 6];
    // getting rid of the mask can shave one cycle off...
    return (w >> (i & 63)) & 1;
}

/* Get the number of bits set (force computation) */
int bitset_container_compute_cardinality(const bitset_container_t *bitset) {
    const uint64_t *a = bitset->array;
    int32_t sum1 = 0;
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k += 4) {
        sum1 += _mm_popcnt_u64(a[k]);
        sum1 += _mm_popcnt_u64(a[k + 1]);
        sum1 += _mm_popcnt_u64(a[k + 2]);
        sum1 += _mm_popcnt_u64(a[k + 3]);
    }
    return sum1;
}

/* computes the union of bitset1 and bitset2 and write the result to bitsetout
 */
int bitset_container_or(const bitset_container_t *bitset1,
                        const bitset_container_t *bitset2,
                        bitset_container_t *bitsetout) {
#ifdef USEAVX
    bitset_container_or_nocard(bitset1, bitset2, bitsetout);
    bitsetout->cardinality = bitset_container_compute_cardinality(bitsetout);
    return bitsetout->cardinality;
#else
    const uint64_t *a1 = bitset1->array;
    const uint64_t *a2 = bitset2->array;
    uint64_t *ao = bitsetout->array;
    int32_t cardinality = 0;
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k += 2) {
        const uint64_t w1 = a1[k] | a2[k];
        ao[k] = w1;
        cardinality += _mm_popcnt_u64(w1);
        const uint64_t w2 = a1[k + 1] | a2[k + 1];
        ao[k + 1] = w2;
        cardinality += _mm_popcnt_u64(w2);
    }
    bitsetout->cardinality = cardinality;
    return bitsetout->cardinality;
#endif
}

/* computes the union of bitset1 and bitset2 and write the result to bitsetout,
 * does not compute the cardinality of the result */
int bitset_container_or_nocard(const bitset_container_t *bitset1,
                               const bitset_container_t *bitset2,
                               bitset_container_t *bitsetout) {
    const uint64_t *a1 = bitset1->array;
    const uint64_t *a2 = bitset2->array;
    uint64_t *ao = bitsetout->array;
#ifdef USEAVX
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS / 4; k++) {
        __m256i A1 = _mm256_lddqu_si256((__m256i *)a1 + k);
        __m256i A2 = _mm256_lddqu_si256((__m256i *)a2 + k);
        __m256i AO = _mm256_or_si256(A1, A2);
        _mm256_storeu_si256((__m256i *)ao + k, AO);
    }
#else
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k++) {
        const uint64_t w = a1[k] | a2[k];
        ao[k] = w;
    }
#endif
    bitsetout->cardinality = -1;
    return bitsetout->cardinality;
}

/* computes the intersection of bitset1 and bitset2 and write the result to
 * bitsetout */
int bitset_container_and(const bitset_container_t *bitset1,
                         const bitset_container_t *bitset2,
                         bitset_container_t *bitsetout) {
#ifdef USEAVX
    bitset_container_and_nocard(bitset1, bitset2, bitsetout);
    bitsetout->cardinality = bitset_container_compute_cardinality(bitsetout);
    return bitsetout->cardinality;
#else
    const uint64_t *a1 = bitset1->array;
    const uint64_t *a2 = bitset2->array;
    uint64_t *ao = bitsetout->array;
    int32_t cardinality = 0;
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k += 2) {
        const uint64_t w1 = a1[k] & a2[k];
        ao[k] = w1;
        cardinality += _mm_popcnt_u64(w1);
        const uint64_t w2 = a1[k + 1] & a2[k + 1];
        ao[k + 1] = w2;
        cardinality += _mm_popcnt_u64(w2);
    }
    bitsetout->cardinality = cardinality;
    return bitsetout->cardinality;
#endif
}

/* computes the intersection of bitset1 and bitset2 and write the result to
 * bitsetout, does not compute the cardinality of the result */
int bitset_container_and_nocard(const bitset_container_t *bitset1,
                                const bitset_container_t *bitset2,
                                bitset_container_t *bitsetout) {
    const uint64_t *a1 = bitset1->array;
    const uint64_t *a2 = bitset2->array;
    uint64_t *ao = bitsetout->array;
#ifdef USEAVX
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS / 4; k++) {
        __m256i A1 = _mm256_lddqu_si256((__m256i *)a1 + k);
        __m256i A2 = _mm256_lddqu_si256((__m256i *)a2 + k);
        __m256i AO = _mm256_and_si256(A1, A2);
        _mm256_storeu_si256((__m256i *)ao + k, AO);
    }
#else
    for (int k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS; k++) {
        uint64_t w = a1[k] & a2[k];
        ao[k] = w;
    }
#endif
    bitsetout->cardinality = -1;
    return bitsetout->cardinality;
}
