/*
 * bitset.h
 *
 */

#ifndef INCLUDE_CONTAINERS_BITSET_H_
#define INCLUDE_CONTAINERS_BITSET_H_

#include <stdint.h>
#include <stdbool.h>
#include <x86intrin.h>

#include "utilasm.h"

#ifdef USEAVX
#define ALIGN_AVX __attribute__((aligned(sizeof(__m256i))))
#else
#define ALIGN_AVX
#endif

enum { BITSET_CONTAINER_SIZE_IN_WORDS = (1 << 16) / 64 };

struct bitset_container_s {
    int32_t cardinality;
    uint64_t *array;
};

typedef struct bitset_container_s bitset_container_t;


/* Create a new bitset. Return NULL in case of failure. */
bitset_container_t *bitset_container_create();

/* Free memory. */
void bitset_container_free(bitset_container_t *bitset);

/* Duplicate bitset */
bitset_container_t *bitset_container_clone( bitset_container_t *src);



/* Set the bit in [begin,end).  */
void bitset_container_set_range(bitset_container_t *bitset, uint32_t begin, uint32_t end);



#ifdef ASMBITMANIPOPTIMIZATION
/* Set the ith bit.  */
inline void bitset_container_set(bitset_container_t *bitset, uint16_t pos) {
    uint64_t shift = 6;
    uint64_t offset;
    uint64_t p = pos;
    ASM_SHIFT_RIGHT(p, shift, offset);
    uint64_t load = bitset->array[offset];
    ASM_SET_BIT_INC_WAS_CLEAR(load, p, bitset->cardinality);
    bitset->array[offset] = load;
}



/* Unset the ith bit.  */
inline void bitset_container_unset(bitset_container_t *bitset, uint16_t pos) {
	uint64_t shift = 6;
    uint64_t offset;
    uint64_t p = pos;
    ASM_SHIFT_RIGHT(p, shift, offset);
    uint64_t load = bitset->array[offset];
    ASM_CLEAR_BIT_DEC_WAS_SET(load, p, bitset->cardinality);
    bitset->array[offset] = load;
}


/* Get the value of the ith bit.  */
inline bool bitset_container_get(const bitset_container_t *bitset, uint16_t pos) {
	 uint64_t word = bitset->array[pos >> 6];
	 const uint64_t p = pos;
	 ASM_INPLACESHIFT_RIGHT(word, p) ;
	return word & 1;
}

#else

/* Set the ith bit.  */
inline void bitset_container_set(bitset_container_t *bitset, uint16_t pos) {
    const uint64_t old_word = bitset->array[pos >> 6];
	const int index = pos & 63;
    const uint64_t new_word = old_word | (UINT64_C(1) << index);
    bitset->cardinality += (old_word ^ new_word) >> index;
    bitset->array[pos >> 6] = new_word;
}



/* Unset the ith bit.  */
inline void bitset_container_unset(bitset_container_t *bitset, uint16_t pos) {
	const uint64_t old_word = bitset->array[pos >> 6];
	const int index = pos & 63;
    const uint64_t new_word = old_word & (~(UINT64_C(1) << index));
    bitset->cardinality -= (old_word ^ new_word) >> index;
    bitset->array[pos >> 6] = new_word;
}



/* Get the value of the ith bit.  */
inline bool bitset_container_get(const bitset_container_t *bitset, uint16_t pos) {
    const uint64_t word = bitset->array[pos >> 6];
    // getting rid of the mask can shave one cycle off...
    return (word >> (pos & 63)) & 1;
}

#endif


/* Get the number of bits set */
inline int bitset_container_cardinality(bitset_container_t *bitset) {
    return bitset->cardinality;
}

/* Get whether there is at least one bit set  */
inline bool bitset_container_nonzero_cardinality(bitset_container_t *bitset) {
    return bitset->cardinality > 0;
}


/* Copy one container into another. We assume that they are distinct. */
void bitset_container_copy(bitset_container_t *source, bitset_container_t *dest) ;

/* Get the number of bits set (force computation) */
int bitset_container_compute_cardinality(const bitset_container_t *bitset);

/* Computes the union of bitsets `src_1' and `src_2' into `dst'. */
int bitset_container_or(const bitset_container_t *src_1,
                        const bitset_container_t *src_2,
                        bitset_container_t *dst);

/* Computes the union of bitsets `src_1' and `src_2' into `dst', but does not
 * update the cardinality. Provided to optimize chained operations. */
int bitset_container_or_nocard(const bitset_container_t *src_1,
                               const bitset_container_t *src_2,
                               bitset_container_t *dst);

/* Computes the intersection of bitsets `src_1' and `src_2' into `dst'. */
int bitset_container_and(const bitset_container_t *src_1,
                         const bitset_container_t *src_2,
                         bitset_container_t *dst);

/* Computes the intersection of bitsets `src_1' and `src_2' into `dst', but does
 * not update the cardinality. Provided to optimize chained operations. */
int bitset_container_and_nocard(const bitset_container_t *src_1,
                                const bitset_container_t *src_2,
                                bitset_container_t *dst);

/* Computes the exclusive or of bitsets `src_1' and `src_2' into `dst'. */
int bitset_container_xor(const bitset_container_t *src_1,
                         const bitset_container_t *src_2,
                         bitset_container_t *dst);

/* Computes the exclusive or of bitsets `src_1' and `src_2' into `dst', but does
 * not update the cardinality. Provided to optimize chained operations. */
int bitset_container_xor_nocard(const bitset_container_t *src_1,
                                const bitset_container_t *src_2,
                                bitset_container_t *dst);

/* Computes the and not of bitsets `src_1' and `src_2' into `dst'. */
int bitset_container_andnot(const bitset_container_t *src_1,
                            const bitset_container_t *src_2,
                            bitset_container_t *dst);

/* Computes the and not or of bitsets `src_1' and `src_2' into `dst', but does
 * not update the cardinality. Provided to optimize chained operations. */
int bitset_container_andnot_nocard(const bitset_container_t *src_1,
                                   const bitset_container_t *src_2,
                                   bitset_container_t *dst);


/*
 * Write out the 16-bit integers contained in this container as a list of 32-bit integers using base
 * as the starting value (it might be expected that base has zeros in its 16 least significant bits).
 * The function returns the number of values written.
 * The caller is responsible for allocating enough memory in out.
 */
int bitset_container_to_uint32_array( uint32_t *out, const bitset_container_t *cont, uint32_t base);

/*
 * Print this container using printf (useful for debugging).
 */
void bitset_container_printf(const bitset_container_t * v);


/*
 * Print this container using printf as a comma-separated list of 32-bit integers starting at base.
 */
void bitset_container_printf_as_uint32_array(const bitset_container_t * v, uint32_t base);


#endif /* INCLUDE_CONTAINERS_BITSET_H_ */
