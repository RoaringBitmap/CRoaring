/*
 * mixed_intersection.h
 *
 */

#ifndef INCLUDE_CONTAINERS_MIXED_UNION_H_
#define INCLUDE_CONTAINERS_MIXED_UNION_H_

#include "array.h"
#include "bitset.h"
#include "run.h"

/* Compute the union of src_1 and src_2 and write the result to
 * dst. It is allowed for src_1 to be dst.   */
void array_bitset_container_union(const array_container_t *src_1,
                                  const bitset_container_t *src_2,
                                  bitset_container_t *dst);

/*
 * Compute the intersection between src_1 and src_2 and write the result
 * to *dst. If the return function is true, the result is a bitset_container_t
 * otherwise is a array_container_t. We assume that dst is not pre-allocated. In
 * case of failure, *dst will be NULL.
 */
bool array_array_container_union(const array_container_t *src_1,
                                 const array_container_t *src_2, void **dst);

#endif /* INCLUDE_CONTAINERS_MIXED_INTERSECTION_H_ */
