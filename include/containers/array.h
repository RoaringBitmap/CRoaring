/*
 * array.h
 *
 */

#ifndef INCLUDE_CONTAINERS_ARRAY_H_
#define INCLUDE_CONTAINERS_ARRAY_H_

enum{
	DEFAULT_MAX_SIZE = 4096// containers with DEFAULT_MAX_SZE or less integers should be array containers
};

struct array_container_s {
    int32_t cardinality;
    uint16_t *array;
};

typedef struct array_container_s array_container_t;

/* Create a new array. Return NULL in case of failure. */
array_container_t *array_container_create();

/* Free memory. */
void array_container_free(array_container_t *bitset);

/* Add i to the set.  */
void array_container_add(array_container_t *bitset, uint16_t i);

/* Remove i from the set.  */
void array_container_remove(array_container_t *bitset, uint16_t i);

/* Check whether i is present.  */
bool array_container_contains(const array_container_t *bitset, uint16_t i);

/* Get the cardinality */
inline int array_container_cardinality(array_container_t *bitset) {
    return bitset->cardinality;
}

/* computes the union of array1 and array2 and write the result to arrayout
 */
int array_container_or(const array_container_t *array1,
                        const array_container_t *array2,
                        array_container_t *out);

/* computes the intersection of array1 and array2 and write the result to
 * arrayout */
int array_container_and(const array_container_t *array1,
                         const array_container_t *array2,
                         array_container_t *arrayout);

#endif /* INCLUDE_CONTAINERS_ARRAY_H_ */
