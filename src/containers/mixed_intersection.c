/*
 * mixed_intersection.c
 *
 */

#include "mixed_intersection.h"

/* Compute the intersection of src_1 and src_2 and write the result to
 * dst.  */
void array_bitset_container_intersection(const array_container_t *src_1,
                                         const bitset_container_t *src_2,
                                         array_container_t *dst) {
    if (dst->capacity < src_1->cardinality)
        array_container_grow(dst, src_1->cardinality, INT32_MAX, false);

    int outpos = 0;
    for (int i = 0; i < src_1->cardinality; ++i) {
        // could probably be vectorized
        uint16_t key = src_1->array[i];
        // next bit could be branchless
        if (bitset_container_contains(src_2, key)) {
            dst->array[outpos++] = key;
        }
    }
}
