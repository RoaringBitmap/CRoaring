/*
 * array.h
 *
 */

#ifndef INCLUDE_CONTAINERS_ARRAY_H_
#define INCLUDE_CONTAINERS_ARRAY_H_

#include <stdint.h>
#include <stdbool.h>

enum{
	DEFAULT_MAX_SIZE = 4096// containers with DEFAULT_MAX_SZE or less integers should be array containers
};

struct array_container_s {
    int32_t cardinality;
    int32_t capacity;
    uint16_t *array;
};

typedef struct array_container_s array_container_t;

/* Create a new array. Return NULL in case of failure. */
array_container_t *array_container_create();

/* Free memory. */
void array_container_free(array_container_t *bitset);

/* Add x to the set. Returns true if x was not already present.  */
bool array_container_add(array_container_t *bitset, uint16_t x);

/* Remove x from the set. Returns true if x was present.  */
bool array_container_remove(array_container_t *bitset, uint16_t x);

/* Check whether x is present.  */
bool array_container_contains(const array_container_t *bitset, uint16_t x);

/* Get the cardinality */
inline int array_container_cardinality(array_container_t *bitset) {
    return bitset->cardinality;
}

/* Compute the union of array1 and array2 and write the result to arrayout.
 * It is assumed that arrayout is distinct from both array1 and array2.
 */
void array_container_union(const array_container_t *array1,
                        const array_container_t *array2,
                        array_container_t *out);

/* Compute the intersection of array1 and array2 and write the result to
 * arrayout.
 * It is assumed that arrayout is distinct from both array1 and array2.
 **/
void array_container_intersection(const array_container_t *array1,
                         const array_container_t *array2,
                         array_container_t *arrayout);

#endif /* INCLUDE_CONTAINERS_ARRAY_H_ */
