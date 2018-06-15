/*
 * convert.h
 *
 */

#ifndef INCLUDE_CONTAINERS_CONVERT_H_
#define INCLUDE_CONTAINERS_CONVERT_H_

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/run.h>

/* Convert an array into a bitset. The input container is not freed or modified.
 */
bitset_container_t *bitset_container_from_array(const array_container_t *arr);

/* Convert a run into a bitset. The input container is not freed or modified. */
bitset_container_t *bitset_container_from_run(const run_container_t *arr);

/* Convert a run into an array. The input container is not freed or modified. */
array_container_t *array_container_from_run(const run_container_t *arr);

/* Convert a bitset into an array. The input container is not freed or modified.
 */
array_container_t *array_container_from_bitset(const bitset_container_t *bits);

/* Convert an array into a run. The input container is not freed or modified.
 */
run_container_t *run_container_from_array(const array_container_t *c);

/* convert a run into either an array or a bitset
 * might free the container */
void *convert_to_bitset_or_array_container(run_container_t *r, int32_t card,
                                           uint8_t *resulttype);

/* convert containers to and from runcontainers, as is most space efficient.
 * The container might be freed. */
void *convert_run_optimize(void *c, uint8_t typecode_original,
                           uint8_t *typecode_after);

/* converts a run container to either an array or a bitset, IF it saves space.
 */
/* If a conversion occurs, the caller is responsible to free the original
 * container and
 * he becomes reponsible to free the new one. */
void *convert_run_to_efficient_container(run_container_t *c,
                                         uint8_t *typecode_after);
// like convert_run_to_efficient_container but frees the old result if needed
void *convert_run_to_efficient_container_and_free(run_container_t *c,
                                                  uint8_t *typecode_after);

/**
 * Create new bitset container which is a union of run container and
 * range [min, max]. Caller is responsible for freeing run container.
 */
bitset_container_t *bitset_container_from_run_range(const run_container_t *run,
                                                    uint32_t min, uint32_t max);

#endif /* INCLUDE_CONTAINERS_CONVERT_H_ */
