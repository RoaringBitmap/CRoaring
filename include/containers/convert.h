/*
 * convert.h
 *
 */

#ifndef INCLUDE_CONTAINERS_CONVERT_H_
#define INCLUDE_CONTAINERS_CONVERT_H_


#include "array.h"
#include "run.h"
#include "bitset.h"



/* Convert an array into a bitset */
bitset_container_t *bitset_container_from_array( array_container_t *arr);

/* Convert a run into a bitset */
bitset_container_t *bitset_container_from_run( run_container_t *arr);

/* Convert a bitset into an array */
array_container_t *array_container_from_bitset( bitset_container_t *bits);

/* convert a run into either an array or a bitset */
void *convert_to_bitset_or_array_container( run_container_t *r, int32_t card, uint8_t *resulttype);

/* convert containers to and from runcontainers, as is most space efficient. */

void *convert_run_optimize(void *c, uint8_t typecode_original, uint8_t *typecode_after);

#endif /* INCLUDE_CONTAINERS_CONVERT_H_ */
