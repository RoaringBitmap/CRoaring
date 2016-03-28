/*
 * mixed_equal.h
 *
 */

#ifndef CONTAINERS_MIXED_EQUAL_H_
#define CONTAINERS_MIXED_EQUAL_H_

#include "array.h"
#include "bitset.h"
#include "run.h"

/**
 * Return true if the two containers have the same content.
 */
bool array_container_equal_bitset(array_container_t* container1,
                                  bitset_container_t* container2);

/**
 * Return true if the two containers have the same content.
 */
bool run_container_equals_array(run_container_t* container1,
                                array_container_t* container2);
/**
 * Return true if the two containers have the same content.
 */
bool run_container_equals_bitset(run_container_t* container1,
                                 bitset_container_t* container2);

#endif /* CONTAINERS_MIXED_EQUAL_H_ */
