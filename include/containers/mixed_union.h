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
 * dst.  */
void array_bitset_container_union(const array_container_t *src_1,
                                  const bitset_container_t *src_2,
								  bitset_container_t *dst);


#endif /* INCLUDE_CONTAINERS_MIXED_INTERSECTION_H_ */
