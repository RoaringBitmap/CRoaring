/*
 * mixed_intersection.h
 *
 */

#ifndef INCLUDE_CONTAINERS_MIXED_UNION_H_
#define INCLUDE_CONTAINERS_MIXED_UNION_H_

/* These functions appear to exclude cases where the
 * inputs have the same type and the output is guaranteed
 * to have the same type as the inputs.  Eg, bitset unions
 */

#include "array.h"
#include "bitset.h"
#include "run.h"

/* Compute the union of src_1 and src_2 and write the result to
 * dst. It is allowed for src_2 to be dst.   */
void array_bitset_container_union(const array_container_t *src_1,
                                  const bitset_container_t *src_2,
                                  bitset_container_t *dst);

/*
 * Compute the union between src_1 and src_2 and write the result
 * to *dst. If the return function is true, the result is a bitset_container_t
 * otherwise is a array_container_t. We assume that dst is not pre-allocated. In
 * case of failure, *dst will be NULL.
 */
bool array_array_container_union(const array_container_t *src_1,
                                 const array_container_t *src_2, void **dst);

/* Compute the union of src_1 and src_2 and write the result to
 * dst. We assume that dst is a
 * valid container. The result might need to be further converted to array or
 * bitset container,
 * the caller is responsible for the eventual conversion. */
void array_run_container_union(const array_container_t *src_1,
                               const run_container_t *src_2,
                               run_container_t *dst);

/* Compute the union of src_1 and src_2 and write the result to
 * dst. It is allowed for dst to be src_2.  */
void run_bitset_container_union(const run_container_t *src_1,
                                const bitset_container_t *src_2,
                                bitset_container_t *dst);

/*
 * Same as array_array_container_union except that if the output is to
 * be an
 * array_container_t, then src_1 is modified and no allocation is made.
 * If the output is to be a bitset_container_t, then caller is responsible
 * to free src_1.
 * In all cases, the result is in *dst.
 */

bool array_array_container_union_inplace(array_container_t *src_1,
                                         const array_container_t *src_2,
                                         void **dst);

#endif /* INCLUDE_CONTAINERS_MIXED_UNION_H_ */
