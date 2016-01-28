/*
 * array.h
 *
 */

#ifndef INCLUDE_CONTAINERS_ARRAY_H_
#define INCLUDE_CONTAINERS_ARRAY_H_

#include <stdint.h>
#include <stdbool.h>



/* Containers with DEFAULT_MAX_SIZE or less integers should be arrays */
enum { DEFAULT_MAX_SIZE = 4096 };

struct array_container_s {
    int32_t cardinality; // how many elements in array are occupied
    int32_t capacity; // size of array (should be larger or equal to cardinality)
    uint16_t *array; // strictly increasing list of integers 
};

typedef struct array_container_s array_container_t;

/* Create a new array. Return NULL in case of failure. */
array_container_t *array_container_create();

/* Free memory owned by `array'. */
void array_container_free(array_container_t *array);

/* Duplicate container */
array_container_t *array_container_clone( array_container_t *src);


/* Add `pos' to `array'. Returns true if `pos' was not present. */
bool array_container_add(array_container_t *array, uint16_t pos);

/* Remove `pos' from `array'. Returns true if `pos' was present. */
bool array_container_remove(array_container_t *array, uint16_t pos);

/* Check whether `pos' is present in `array'.  */
bool array_container_contains(const array_container_t *array, uint16_t pos);

/* Get the cardinality of `array'. */
inline int array_container_cardinality(const array_container_t *array) {
    return array->cardinality;
}

inline bool array_container_nonzero_cardinality(const array_container_t *array) {
    return array->cardinality > 0;
}

/* Copy one container into another. We assume that they are distinct. */
void array_container_copy(array_container_t *source, array_container_t *dest) ;

/* Set the cardinality to zero (does not release memory). */
inline void array_container_clear(array_container_t *array) {
    array->cardinality = 0;
}

/* Compute the union of `src_1' and `src_2' and write the result to `dst'
 * It is assumed that `dst' is distinct from both `src_1' and `src_2'. */
void array_container_union(const array_container_t *src_1,
                           const array_container_t *src_2,
                           array_container_t *dst);

/* Compute the intersection of src_1 and src_2 and write the result to
 * dst. It is assumed that dst is distinct from both src_1 and src_2. */
void array_container_intersection(const array_container_t *src_1,
                                  const array_container_t *src_2,
                                  array_container_t *dst);


/*
 * Write out the 16-bit integers contained in this container as a list of 32-bit integers using base
 * as the starting value (it might be expected that base has zeros in its 16 least significant bits).
 * The function returns the number of values written.
 * The caller is responsible for allocating enough memory in out.
 */
int array_container_to_uint32_array( uint32_t *out, const array_container_t *cont, uint32_t base);

/* Create a new array with capacity size. Return NULL in case of failure. */
array_container_t *array_container_create_given_capacity(int32_t size);


/* Compute the number of runs */
int32_t array_container_number_of_runs( array_container_t *a);

/*
 * Print this container using printf (useful for debugging).
 */
void array_container_printf(const array_container_t * v);


/*
 * Print this container using printf as a comma-separated list of 32-bit integers starting at base.
 */
void array_container_printf_as_uint32_array(const array_container_t * v, uint32_t base);

inline int32_t array_container_serialized_size_in_bytes( int32_t card) {
  return card * 2 + 2;
}


#endif /* INCLUDE_CONTAINERS_ARRAY_H_ */
