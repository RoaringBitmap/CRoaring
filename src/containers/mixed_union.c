/*
 * mixed_union.c
 *
 */

#include "mixed_union.h"
#include "bitset_util.h"
#include "convert.h"

/* Compute the union of src_1 and src_2 and write the result to
 * dst.  */
void array_bitset_container_union(const array_container_t *src_1,
                                  const bitset_container_t *src_2,
                                  bitset_container_t *dst) {
    if (src_2 != dst) bitset_container_copy(src_2, dst);
    dst->cardinality = bitset_set_list_withcard(
        dst->array, dst->cardinality, src_1->array, src_1->cardinality);
}

bool array_array_container_union(const array_container_t *src_1,
                                 const array_container_t *src_2, void **dst) {
    int totalCardinality = src_1->cardinality + src_2->cardinality;
    if (totalCardinality <= DEFAULT_MAX_SIZE) {
        *dst = array_container_create_given_capacity(totalCardinality);
        if (*dst != NULL) array_container_union(src_1, src_2, *dst);
        return false;  // not a bitset
    }
    *dst = bitset_container_create();
    bool returnval = true;  // expect a bitset
    if (*dst != NULL) {
        bitset_container_t *ourbitset = *dst;
        bitset_set_list(ourbitset->array, src_1->array, src_1->cardinality);
        ourbitset->cardinality =
            bitset_set_list_withcard(ourbitset->array, src_1->cardinality,
                                     src_2->array, src_2->cardinality);
        if (ourbitset->cardinality <= DEFAULT_MAX_SIZE) {
            // need to convert!
            *dst = array_container_from_bitset(ourbitset);
            bitset_container_free(ourbitset);
            returnval = false;  // not going to be a bitset
        }
    }
    return returnval;
}
