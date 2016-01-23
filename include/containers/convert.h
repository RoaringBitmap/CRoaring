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

#endif /* INCLUDE_CONTAINERS_CONVERT_H_ */
