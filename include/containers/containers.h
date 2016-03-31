#ifndef CONTAINERS_CONTAINERS_H
#define CONTAINERS_CONTAINERS_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "array.h"
#include "bitset.h"
#include "convert.h"
#include "mixed_equal.h"
#include "mixed_intersection.h"
#include "mixed_union.h"
#include "run.h"

// would enum be possible or better?

#define ARRAY_CONTAINER_TYPE_CODE 1
#define RUN_CONTAINER_TYPE_CODE 2
#define BITSET_CONTAINER_TYPE_CODE 3

// macro for pairing container type codes
#define CONTAINER_PAIR(c1, c2) (4 * (c1) + (c2))

static const char *container_names[] = {"bitset", "array", "run"};
/**
 * Get the container name from the typecode
 */
static inline const char *get_container_name(uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return container_names[0];
        case ARRAY_CONTAINER_TYPE_CODE:
            return container_names[1];
        case RUN_CONTAINER_TYPE_CODE:
            return container_names[2];
        default:
            assert(0);
            return "unknown";
    }
}

/**
 * Get the container cardinality (number of elements), requires a  typecode
 */
static inline int container_get_cardinality(const void *container,
                                            uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_cardinality(container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_cardinality(container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_cardinality(container);
    }
    return 0;  // unreached
}

/**
 * Writes the underlying array to buf, outputs how many bytes were written.
 * This is meant to be byte-by-byte compatible with the Java and Go versions of
 * Roaring.
 * The number of bytes written should be
 * container_write(container, buf).
 *
 */
static inline int32_t container_write(void *container, uint8_t typecode,
                                      char *buf) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_write(container, buf);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_write(container, buf);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_write(container, buf);
    }
    return 0;  // unreached
}

/**
 * Get the container size in bytes under portable serialization (see
 * container_write), requires a
 * typecode
 */
static inline int32_t container_size_in_bytes(void *container,
                                              uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_size_in_bytes(container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_size_in_bytes(container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_size_in_bytes(container);
    }
    return 0;  // unreached
}

/**
 * print the container (useful for debugging), requires a  typecode
 */
void container_printf(const void *container, uint8_t typecode);

/**
 * print the content of the container as a comma-separated list of 32-bit values
 * starting at base, requires a  typecode
 */
void container_printf_as_uint32_array(const void *container, uint8_t typecode,
                                      uint32_t base);

/**
 * Checks whether a container is not empty, requires a  typecode
 */
static inline bool container_nonzero_cardinality(const void *container,
                                                 uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_nonzero_cardinality(container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_nonzero_cardinality(container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_nonzero_cardinality(container);
    }
    return 0;  // unreached
}

/**
 * Recover memory from a container, requires a  typecode
 */
static inline void container_free(void *container, uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_free((bitset_container_t *)container);
            break;
        case ARRAY_CONTAINER_TYPE_CODE:
            array_container_free((array_container_t *)container);
            break;
        case RUN_CONTAINER_TYPE_CODE:
            run_container_free((run_container_t *)container);
            break;
        default:
            __builtin_unreachable();
    }
}

/**
 * Convert a container to an array of values, requires a  typecode as well as a
 * "base" (most significant values)
 * Returns number of ints added.
 */
static inline int container_to_uint32_array(uint32_t *output,
                                            const void *container,
                                            uint8_t typecode, uint32_t base) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_to_uint32_array(output, container, base);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_to_uint32_array(output, container, base);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_to_uint32_array(output, container, base);
    }
    __builtin_unreachable();
    return 0;  // unreached
}

/**
 * Add a value to a container, requires a  typecode, fills in new_typecode and
 * return (possibly different) container.
 * This function may allocate a new container, and caller is responsible for
 * memory deallocation
 */
static inline void *container_add(void *container, uint16_t val,
                                  uint8_t typecode, uint8_t *new_typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_set((bitset_container_t *)container, val);
            *new_typecode = BITSET_CONTAINER_TYPE_CODE;
            return container;
        case ARRAY_CONTAINER_TYPE_CODE:;
            array_container_t *ac = (array_container_t *)container;
            array_container_add(ac, val);
            if (array_container_cardinality(ac) > DEFAULT_MAX_SIZE) {
                *new_typecode = BITSET_CONTAINER_TYPE_CODE;
                return bitset_container_from_array(ac);
            } else {
                *new_typecode = ARRAY_CONTAINER_TYPE_CODE;
                return ac;
            }
        case RUN_CONTAINER_TYPE_CODE:
            // per Java, no container type adjustments are done (revisit?)
            run_container_add((run_container_t *)container, val);
            *new_typecode = RUN_CONTAINER_TYPE_CODE;
            return container;
        default:
            assert(0);
            __builtin_unreachable();
            return NULL;
    }
}

/**
 * Check whether a value is in a container, requires a  typecode
 */
static inline bool container_contains(const void *container, uint16_t val,
                                      uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_get((const bitset_container_t *)container,
                                        val);
        case ARRAY_CONTAINER_TYPE_CODE:;
            return array_container_contains(
                (const array_container_t *)container, val);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_contains((const run_container_t *)container,
                                          val);
        default:
            assert(0);
            __builtin_unreachable();
            return NULL;
    }
}

/**
 * Copies a container, requires a typecode. This allocates new memory, caller
 * is responsible for deallocation.
 */
static inline void *container_clone(const void *container, uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_clone((bitset_container_t *)container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_clone((array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_clone((run_container_t *)container);
        default:
            assert(0);
            __builtin_unreachable();
            return NULL;
    }
}

int32_t container_serialize(void *container, uint8_t typecode,
                            char *buf) WARN_UNUSED;

uint32_t container_serialization_len(void *container, uint8_t typecode);

void *container_deserialize(uint8_t typecode, const char *buf, size_t buf_len);

/**
 * Returns true if the two containers have the same content. Note that
 * two containers having different types can be "equal" in this sense.
 */
static inline bool container_equals(const void *c1, uint8_t type1,
                                    const void *c2, uint8_t type2) {
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return bitset_container_equals((bitset_container_t *)c1,
                                           (bitset_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            return run_container_equals_bitset((run_container_t *)c2,
                                               (bitset_container_t *)c1);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return run_container_equals_bitset((run_container_t *)c1,
                                               (bitset_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            // java would always return false?
            return array_container_equal_bitset((array_container_t *)c2,
                                                (bitset_container_t *)c1);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            // java would always return false?
            return array_container_equal_bitset((array_container_t *)c1,
                                                (bitset_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return run_container_equals_array((run_container_t *)c2,
                                              (array_container_t *)c1);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            return run_container_equals_array((run_container_t *)c1,
                                              (array_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            return array_container_equals((array_container_t *)c1,
                                          (array_container_t *)c2);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return run_container_equals((run_container_t *)c1,
                                        (run_container_t *)c2);
        default:
            assert(0);
            __builtin_unreachable();
            return NULL;
    }
}

// macro-izations possibilities for generic non-inplace binary-op dispatch

/**
 * Compute intersection between two containers, generate a new container (having
 * type result_type), requires a typecode. This allocates new memory, caller
 * is responsible for deallocation.
 */
static inline void *container_and(const void *c1, uint8_t type1, const void *c2,
                                  uint8_t type2, uint8_t *result_type) {
    void *result;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = bitset_bitset_container_intersection(c1, c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            result = array_container_create();
            array_container_intersection(c1, c2, result);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            run_container_intersection(c1, c2, result);
            return convert_run_to_efficient_container(result, result_type);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            result = array_container_create();
            array_bitset_container_intersection(c2, c1, result);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_bitset_container_intersection(c1, c2, result);
            return result;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            *result_type = run_bitset_container_intersection(c2, c1, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = run_bitset_container_intersection(c1, c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_run_container_intersection(c1, c2, result);
            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_run_container_intersection(c2, c1, result);
            return result;
        default:
            assert(0);
            __builtin_unreachable();
            return NULL;
    }
}

/**
 * Compute intersection between two containers, with result in the first
 container if possible. If the returned pointer is identical to c1,
 then the container has been modified. If the returned pointer is different
 from c1, then a new container has been created and the caller is responsible
 for freeing it.
 The type of the first container may change. Returns the modified
 (and possibly new) container.
*/
static inline void *container_iand(void *c1, uint8_t type1, const void *c2,
                                   uint8_t type2, uint8_t *result_type) {
    void *result;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type =
                bitset_bitset_container_intersection_inplace(c1, c2, &result)
                    ? BITSET_CONTAINER_TYPE_CODE
                    : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            array_container_intersection_inplace(c1, c2);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            run_container_intersection(c1, c2, result);
            // as of January 2016, Java code used non-in-place intersection for
            // two runcontainers
            return convert_run_to_efficient_container(result, result_type);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            // c1 is a bitmap so no inplace possible
            result = array_container_create();
            array_bitset_container_intersection(c2, c1, result);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = ARRAY_CONTAINER_TYPE_CODE;         // never bitset
            array_bitset_container_intersection(c1, c2, c1);  // allowed
            return c1;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            // will attempt in-place computation
            *result_type = run_bitset_container_intersection(c2, c1, &c1)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = run_bitset_container_intersection(c1, c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_run_container_intersection(c1, c2, result);
            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_run_container_intersection(c2, c1, result);
            return result;
        default:
            assert(0);
            __builtin_unreachable();
            return NULL;
    }
}

/**
 * Compute union between two containers, generate a new container (having type
 * result_type), requires a typecode. This allocates new memory, caller
 * is responsible for deallocation.
 */
static inline void *container_or(const void *c1, uint8_t type1, const void *c2,
                                 uint8_t type2, uint8_t *result_type) {
    void *result;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            bitset_container_or(c1, c2, result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = array_array_container_union(c1, c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            run_container_union(c1, c2, result);
            *result_type = RUN_CONTAINER_TYPE_CODE;
            // todo: could be optimized since will never convert to array
            result = convert_run_to_efficient_container(result, result_type);
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            array_bitset_container_union(c2, c1, result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            array_bitset_container_union(c1, c2, result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            run_bitset_container_union(c2, c1, result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            run_bitset_container_union(c1, c2, result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_union(c1, c2, result);
            result = convert_run_to_efficient_container(result, result_type);
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_union(c2, c1, result);
            result = convert_run_to_efficient_container(result, result_type);
            return result;
        default:
            assert(0);
            __builtin_unreachable();
            return NULL;  // unreached
    }
}

/**
 * Compute the union between two containers, with result in the first container.
 * If the returned pointer is identical to c1, then the container has been
 * modified.
 * If the returned pointer is different from c1, then a new container has been
 * created and the caller is responsible for freeing it.
 * The type of the first container may change. Returns the modified
 * (and possibly new) container
*/
static inline void *container_ior(void *c1, uint8_t type1, const void *c2,
                                  uint8_t type2, uint8_t *result_type) {
    void *result;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            bitset_container_or(c1, c2, c1);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            // Java impl. also does not do real in-place in this case
            *result_type = array_array_container_union(c1, c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            run_container_union_inplace(c1, c2);
            return convert_run_to_efficient_container(c1, result_type);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            array_bitset_container_union(c2, c1, c1);
            *result_type = BITSET_CONTAINER_TYPE_CODE;  // never array
            return c1;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            // c1 is an array, so no in-place possible
            result = bitset_container_create();
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            array_bitset_container_union(c1, c2, result);
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            run_bitset_container_union(c2, c1, c1);  // allowed
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            run_bitset_container_union(c1, c2, result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_union(c1, c2, result);
            result = convert_run_to_efficient_container(result, result_type);
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            // todo:in Java, an inplace computation would be attempted
            result = run_container_create();
            array_run_container_union(c2, c1, result);
            result = convert_run_to_efficient_container(result, result_type);
            return result;
        default:
            assert(0);
            __builtin_unreachable();
            return NULL;
    }
}

/**
 * Visit all values x of the container once, passing (base+x,ptr)
 * to iterator. You need to specify a container and its type.
 */
static inline void container_iterate(const void *container, uint8_t typecode,
                                     uint32_t base, roaring_iterator iterator,
                                     void *ptr) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_iterate(container, base, iterator, ptr);
            break;
        case ARRAY_CONTAINER_TYPE_CODE:
            array_container_iterate(container, base, iterator, ptr);
            break;
        case RUN_CONTAINER_TYPE_CODE:
            run_container_iterate(container, base, iterator, ptr);
            break;
        default:
            assert(0);
            __builtin_unreachable();
    }
}

#endif
