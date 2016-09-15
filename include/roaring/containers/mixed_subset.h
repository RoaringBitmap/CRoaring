/*
 * mixed_subset.h
 *
 */

#ifndef CONTAINERS_MIXED_SUBSET_H_
#define CONTAINERS_MIXED_SUBSET_H_

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/run.h>

/**
 * Return true if container1 is a subset of container2.
 */
bool array_container_is_subset_bitset(array_container_t* container1,
                                  bitset_container_t* container2);

/**
* Return true if container1 is a subset of container2.
 */
bool run_container_is_subset_array(run_container_t* container1,
                                array_container_t* container2);

/**
* Return true if container1 is a subset of container2.
 */
bool array_container_is_subset_run(array_container_t* container1,
                                run_container_t* container2);

/**
* Return true if container1 is a subset of container2.
 */
bool run_container_is_subset_bitset(run_container_t* container1,
                                 bitset_container_t* container2);

/**
* Return true if container1 is a subset of container2.
*/
bool bitset_container_is_subset_run(bitset_container_t* container1,
                              run_container_t* container2);

#endif /* CONTAINERS_MIXED_SUBSET_H_ */
