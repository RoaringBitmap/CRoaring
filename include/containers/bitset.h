/*
 * bitset.h
 *
 */

#ifndef INCLUDE_CONTAINERS_BITSET_H_
#define INCLUDE_CONTAINERS_BITSET_H_

#include <stdint.h>
#include <stdbool.h>
#include <x86intrin.h>

//#define USEAVX  now controlled by makefile

#ifdef USEAVX
#define ALIGN_AVX __attribute__((aligned(sizeof(__m256i))))
#else
#define ALIGN_AVX
#endif

enum { BITSET_CONTAINER_SIZE_IN_WORDS = (1 << 16) / 64 };

struct bitset_container_s {
    int32_t cardinality;
    ALIGN_AVX uint64_t array[BITSET_CONTAINER_SIZE_IN_WORDS];
};

typedef struct bitset_container_s bitset_container_t;

/* Create a new bitset. Return NULL in case of failure. */
bitset_container_t *bitset_container_create();

/* Free memory. */
void bitset_container_free(bitset_container_t *bitset);

/* Set the ith bit.  */
void bitset_container_set(bitset_container_t *bitset, uint16_t i);

/* Unset the ith bit.  */
void bitset_container_unset(bitset_container_t *bitset, uint16_t i);

/* Get the value of the ith bit.  */
bool bitset_container_get(const bitset_container_t *bitset, uint16_t i);

/* Get the number of bits set */
inline int bitset_container_cardinality(bitset_container_t *bitset) {
    return bitset->cardinality;
}

/* Get the number of bits set (force computation) */
int bitset_container_compute_cardinality(const bitset_container_t *bitset);

/* computes the union of bitset1 and bitset2 and write the result to bitsetout
 */
int bitset_container_or(const bitset_container_t *bitset1,
                        const bitset_container_t *bitset2,
                        bitset_container_t *out);

/* computes the union of bitset1 and bitset2 and write the result to bitsetout,
 * does not compute the cardinality of the result */
int bitset_container_or_nocard(const bitset_container_t *bitset1,
                               const bitset_container_t *bitset2,
                               bitset_container_t *bitsetout);

/* similarly, routines for intersection */
int bitset_container_and(const bitset_container_t *bitset1,
                         const bitset_container_t *bitset2,
                         bitset_container_t *bitsetout);

int bitset_container_and_nocard(const bitset_container_t *bitset1,
                                const bitset_container_t *bitset2,
                                bitset_container_t *bitsetout);

/* similarly, routines for xor */
int bitset_container_xor(const bitset_container_t *bitset1,
                         const bitset_container_t *bitset2,
                         bitset_container_t *bitsetout);

int bitset_container_xor_nocard(const bitset_container_t *bitset1,
                                const bitset_container_t *bitset2,
                                bitset_container_t *bitsetout);

/* similarly, routines for and_not */
int bitset_container_andnot(const bitset_container_t *bitset1,
                            const bitset_container_t *bitset2,
                            bitset_container_t *bitsetout);

int bitset_container_andnot_nocard(const bitset_container_t *bitset1,
                                   const bitset_container_t *bitset2,
                                   bitset_container_t *bitsetout);


#endif /* INCLUDE_CONTAINERS_BITSET_H_ */
