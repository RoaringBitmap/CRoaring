/*
 * mixed_andnot_nonzero.h
 *
 */

#ifndef INCLUDE_CONTAINERS_MIXED_ANDNOT_NONZERO_H_
#define INCLUDE_CONTAINERS_MIXED_ANDNOT_NONZERO_H_

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/run.h>

#ifdef __cplusplus
extern "C" {
namespace roaring {
namespace internal {
#endif

bool bitset_array_container_andnot_nonzero(const bitset_container_t *src_1,
                                           const array_container_t *src_2);

bool bitset_run_container_andnot_nonzero(const bitset_container_t *src_1,
                                         const run_container_t *src_2);

bool array_bitset_container_andnot_nonzero(const array_container_t *src_1,
                                           const bitset_container_t *src_2);

bool array_run_container_andnot_nonzero(const array_container_t *src_1,
                                        const run_container_t *src_2);

bool run_bitset_container_andnot_nonzero(const run_container_t *src_1,
                                         const bitset_container_t *src_2);

bool run_array_container_andnot_nonzero(const run_container_t *src_1,
                                        const array_container_t *src_2);

#ifdef __cplusplus
}
}
}  // extern "C" { namespace roaring { namespace internal {
#endif

#endif /* INCLUDE_CONTAINERS_MIXED_ANDNOT_NONZERO_H_ */
