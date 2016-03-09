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
    const int32_t origcard = src_1->cardinality;
    dst->cardinality = 0;
    for (int i = 0; i < origcard; ++i) {
        // could probably be vectorized
        uint16_t key = src_1->array[i];
        // next bit could be branchless
        if (bitset_container_contains(src_2, key)) {
            dst->array[dst->cardinality++] = key;
        }
    }
}

/*
 * Compute the intersection between src_1 and src_2 and write the result
 * to *dst. If the return function is true, the result is a bitset_container_t
 * otherwise is a array_container_t.
 */
bool bitset_bitset_container_intersection(const bitset_container_t *src_1,
                                          const bitset_container_t *src_2,
                                          void **dst) {
    const int newCardinality = bitset_container_and_justcard(src_1, src_2);
    if (newCardinality > DEFAULT_MAX_SIZE) {
        *dst = bitset_container_create();
        if (*dst != NULL) {
            bitset_container_and_nocard(src_1, src_2, *dst);
            ((bitset_container_t *)*dst)->cardinality = newCardinality;
        }
        return true;  // it is a bitset
    }
    *dst = array_container_create_given_capacity(newCardinality);
    if (*dst != NULL) {
        ((array_container_t *)*dst)->cardinality = newCardinality;
        int outpos = 0;
        uint16_t *out = ((array_container_t *)*dst)->array;
        for (int i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; ++i) {
            // todo: vectorize!
            uint64_t w = ((bitset_container_t *)src_1)->array[i] &
                         ((bitset_container_t *)src_2)->array[i];
            while (w != 0) {
                uint64_t t = w & -w;
                int r = __builtin_ctzl(w);
                out[outpos++] = i * 64 + r;
                w ^= t;
            }
        }
    }
    return false;  // not a bitset
}
