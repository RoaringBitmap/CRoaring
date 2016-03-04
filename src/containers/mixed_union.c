/*
 * mixed_union.c
 *
 */

#include "mixed_union.h"
#include "util.h"

/* Compute the union of src_1 and src_2 and write the result to
 * dst.  */
void array_bitset_container_union(const array_container_t *src_1,
                                  const bitset_container_t *src_2,
                                  bitset_container_t *dst) {
    bitset_container_copy(src_2, dst);
    dst->cardinality = bitset_set_list(dst->array, dst->cardinality,
                                       src_1->array, src_1->cardinality);
}
