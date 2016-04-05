/*
 * array.h
 *
 */

#ifndef INCLUDE_CONTAINERS_ARRAY_H_
#define INCLUDE_CONTAINERS_ARRAY_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "portability.h"
#include "roaring_types.h"

/* Containers with DEFAULT_MAX_SIZE or less integers should be arrays */
enum { DEFAULT_MAX_SIZE = 4096 };

/* struct array_container - sparse representation of a bitmap
 *
 * @cardinality: number of indices in `array` (and the bitmap)
 * @capacity:    allocated size of `array`
 * @array:       sorted list of integers
 */
struct array_container_s {
    int32_t cardinality;
    int32_t capacity;
    uint16_t *array;
};

typedef struct array_container_s array_container_t;

/* Create a new array with default. Return NULL in case of failure. See also
 * array_container_create_given_capacity. */
array_container_t *array_container_create(void);

/* Create a new array with a specified capacity size. Return NULL in case of
 * failure. */
array_container_t *array_container_create_given_capacity(int32_t size);

/* Free memory owned by `array'. */
void array_container_free(array_container_t *array);

/* Duplicate container */
array_container_t *array_container_clone(array_container_t *src);

int32_t array_container_serialize(array_container_t *container,
                                  char *buf) WARN_UNUSED;

uint32_t array_container_serialization_len(array_container_t *container);

void *array_container_deserialize(const char *buf, size_t buf_len);

/* Add `pos' to `array'. Returns true if `pos' was not present. */
bool array_container_add(array_container_t *array, uint16_t pos);

/* Remove `pos' from `array'. Returns true if `pos' was present. */
bool array_container_remove(array_container_t *array, uint16_t pos);

/* Check whether `pos' is present in `array'.  */
bool array_container_contains(const array_container_t *array, uint16_t pos);

/* Get the cardinality of `array'. */
static inline int array_container_cardinality(const array_container_t *array) {
    return array->cardinality;
}

static inline bool array_container_nonzero_cardinality(
    const array_container_t *array) {
    return array->cardinality > 0;
}

/* Copy one container into another. We assume that they are distinct. */
void array_container_copy(const array_container_t *src, array_container_t *dst);

/* Set the cardinality to zero (does not release memory). */
static inline void array_container_clear(array_container_t *array) {
    array->cardinality = 0;
}

static inline bool array_container_empty(const array_container_t *array) {
    return array->cardinality == 0;
}

static inline bool array_container_full(const array_container_t *array) {
    return array->cardinality == array->capacity;
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

/* computes the intersection of array1 and array2 and write the result to
 * array1.
 * */
void array_container_intersection_inplace(array_container_t *src_1,
                                          const array_container_t *src_2);

/* computes the negation of an array container src, writing to dst,
 *  assumed distinct from src
 *  moved to mixed_negation  TODO: clean me up here
void array_container_negation(const array_container_t *src,
                              array_container_t *dst);

 TODO delete me too when mixed is ok
* computes the negation of an array container src_dest, writing to src_dest
 * Requires result fits* /
void array_container_negation_inplace(array_container_t *src_dest);
*/

/*
 * Write out the 16-bit integers contained in this container as a list of 32-bit
 * integers using base
 * as the starting value (it might be expected that base has zeros in its 16
 * least significant bits).
 * The function returns the number of values written.
 * The caller is responsible for allocating enough memory in out.
 */
int array_container_to_uint32_array(uint32_t *out,
                                    const array_container_t *cont,
                                    uint32_t base);

/* Compute the number of runs */
int32_t array_container_number_of_runs(const array_container_t *a);

/*
 * Print this container using printf (useful for debugging).
 */
void array_container_printf(const array_container_t *v);

/*
 * Print this container using printf as a comma-separated list of 32-bit
 * integers starting at base.
 */
void array_container_printf_as_uint32_array(const array_container_t *v,
                                            uint32_t base);

/**
 * Return the serialized size in bytes of a container having cardinality "card".
 */
static inline int32_t array_container_serialized_size_in_bytes(int32_t card) {
    return card * 2 + 2;
}

/**
 * increase capacity to at least min, and to no more than max. Whether the
 * existing data needs to be copied over depends on the value of the "preserve"
 * parameter.
 * If preserve is false,
 * then the new content will be uninitialized, otherwise the original data is
 * copied.
 */
void array_container_grow(array_container_t *container, int32_t min,
                          int32_t max, bool preserve);

void array_container_iterate(const array_container_t *cont, uint32_t base,
                             roaring_iterator iterator, void *ptr);

/**
 * Writes the underlying array to buf, outputs how many bytes were written.
 * This is meant to be byte-by-byte compatible with the Java and Go versions of
 * Roaring.
 * The number of bytes written should be
 * array_container_size_in_bytes(container).
 *
 */
int32_t array_container_write(array_container_t *container, char *buf);
/**
 * Reads the instance from buf, outputs how many bytes were read.
 * This is meant to be byte-by-byte compatible with the Java and Go versions of
 * Roaring.
 * The number of bytes read should be array_container_size_in_bytes(container).
 * You need to provide the (known) cardinality.
 */
int32_t array_container_read(int32_t cardinality, array_container_t *container,
                             const char *buf);

/**
 * Return the serialized size in bytes of a container (see
 * bitset_container_write)
 * This is meant to be compatible with the Java and Go versions of Roaring and
 * assumes
 * that the cardinality of the container is already known.
 *
 */
static inline int32_t array_container_size_in_bytes(
    array_container_t *container) {
    return container->cardinality * sizeof(uint16_t);
}

/**
 * Return true if the two arrays have the same content.
 */
bool array_container_equals(array_container_t *container1,
                            array_container_t *container2);

#endif /* INCLUDE_CONTAINERS_ARRAY_H_ */
