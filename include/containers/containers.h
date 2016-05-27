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
#include "mixed_negation.h"
#include "mixed_union.h"
#include "run.h"

// would enum be possible or better?

#define ARRAY_CONTAINER_TYPE_CODE 1
#define RUN_CONTAINER_TYPE_CODE 2
#define BITSET_CONTAINER_TYPE_CODE 3
#define SHARED_CONTAINER_TYPE_CODE 4

// macro for pairing container type codes
#define CONTAINER_PAIR(c1, c2) (4 * (c1) + (c2))





/**
 * A shared container is a wrapper around a container
 * with reference counting.
 */


struct shared_container_s {
    void * container;
    uint8_t typecode;
    uint32_t counter;
};

typedef struct shared_container_s shared_container_t;

/*
 * With copy_on_write = true
 *  Create a new shared container if the typecode is not SHARED_CONTAINER_TYPE,
 * otherwise, increase the count
 * If copy_on_write = false, then clone.
 * Return NULL in case of failure.
 **/
void *get_copy_of_container(void * container, uint8_t * typecode, bool copy_on_write);

/* Frees a shared container (actually decrement its counter and only frees when the counter falls to zero). */
void shared_container_free (shared_container_t * container);

/* extract a copy from the shared container, freeing the shared container if there is just one instance left,
clone instances when the counter is higher than one
*/
void * shared_container_extract_copy (shared_container_t * container, uint8_t * typecode);

/* access to container underneath */
static inline const void * container_unwrap_shared(const void *candidate_shared_container, uint8_t * type) {
	if(*type == SHARED_CONTAINER_TYPE_CODE) {
		*type = ((const shared_container_t *) candidate_shared_container)->typecode;
		assert(*type != SHARED_CONTAINER_TYPE_CODE);
		return ((shared_container_t *) candidate_shared_container)->container;
	} else {
		assert(*type != SHARED_CONTAINER_TYPE_CODE);
		return candidate_shared_container;
	}
}


/* access to container underneath and queries its type */
static inline uint8_t get_container_type(const void *container, uint8_t  type) {
	if(type == SHARED_CONTAINER_TYPE_CODE) {
		return ((shared_container_t *) container)->typecode;
	} else {
		return type;
	}
}


/**
 * Copies a container, requires a typecode. This allocates new memory, caller
 * is responsible for deallocation. If the container is not shared, then it is
 * physically cloned. Sharable containers are not cloneable.
 */
void *container_clone(const void *container, uint8_t typecode);


/* access to container underneath, cloning it if needed */
static inline void * get_writable_copy_if_shared(void *candidate_shared_container, uint8_t * type) {
	if(*type == SHARED_CONTAINER_TYPE_CODE) {
                 return shared_container_extract_copy(candidate_shared_container,type);
	} else {
		return candidate_shared_container;
	}
}




/**
 * End of shared container code
 */



static const char *container_names[] = {"bitset", "array", "run", "shared"};
static const char *shared_container_names[] = {"bitset (shared)", "array (shared)", "run (shared)"};

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
        case SHARED_CONTAINER_TYPE_CODE:
            return container_names[3];
        default:
            assert(false);
            __builtin_unreachable();
            return "unknown";
    }
}

static inline const char *get_full_container_name(void * container, uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return container_names[0];
        case ARRAY_CONTAINER_TYPE_CODE:
            return container_names[1];
        case RUN_CONTAINER_TYPE_CODE:
            return container_names[2];
        case SHARED_CONTAINER_TYPE_CODE:
        	switch(((shared_container_t *)container)->typecode) {
            case BITSET_CONTAINER_TYPE_CODE:
                return shared_container_names[0];
            case ARRAY_CONTAINER_TYPE_CODE:
                return shared_container_names[1];
            case RUN_CONTAINER_TYPE_CODE:
                return shared_container_names[2];
            default:
                assert(false);
                __builtin_unreachable();
                return "unknown";
        	}
        	break;
        default:
            assert(false);
            __builtin_unreachable();
            return "unknown";
    }
    __builtin_unreachable();
}


/**
 * Get the container cardinality (number of elements), requires a  typecode
 */
static inline int container_get_cardinality(const void *container,
                                            uint8_t typecode) {
	container = container_unwrap_shared(container,&typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_cardinality(
                (const bitset_container_t *)container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_cardinality(
                (const array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_cardinality(
                (const run_container_t *)container);
    }
    assert(false);
    __builtin_unreachable();
    return 0;  // unreached
}

/*  Create a container with all the values between in [min,max) at a
    distance k*step from min. */
static inline void *container_from_range(uint8_t *type, uint32_t min, uint32_t max,
                                                  uint16_t step) {
    if(step == 0) return NULL; // being paranoid
    if(step == 1) {
       *type = RUN_CONTAINER_TYPE_CODE;
       return run_container_create_range(min, max);
    }
    int size = (max-min+step-1)/step;
    if(size <= DEFAULT_MAX_SIZE) { // array container
        *type = ARRAY_CONTAINER_TYPE_CODE;
        array_container_t * array = array_container_create_given_capacity(size);
        array_container_add_from_range(array, min, max, step);
        assert(array->cardinality == size);
        return array;
    }
    else { // bitset container
        *type = BITSET_CONTAINER_TYPE_CODE;
        bitset_container_t *bitset = bitset_container_create();
        bitset_container_add_from_range(bitset, min, max, step);
        assert(bitset->cardinality == size);
        return bitset;
    }
}

/**
 * "repair" the container after lazy operations.
 */
static inline void *container_repair_after_lazy(void *container,
                                                uint8_t *typecode) {
     container = get_writable_copy_if_shared(container,typecode);//TODO: this introduces unnecessary cloning
	void *result = NULL;
    switch (*typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            ((bitset_container_t *)container)->cardinality =
                bitset_container_compute_cardinality(
                    (bitset_container_t *)container);
            if (((bitset_container_t *)container)->cardinality <=
                DEFAULT_MAX_SIZE) {
                result = array_container_from_bitset(
                    (const bitset_container_t *)container);
                bitset_container_free((bitset_container_t *)container);
                *typecode = ARRAY_CONTAINER_TYPE_CODE;
                return result;
            }
            return container;
        case ARRAY_CONTAINER_TYPE_CODE:
            return container;  // nothing to do
        case RUN_CONTAINER_TYPE_CODE:
           return convert_run_to_efficient_container_and_free(
                   (run_container_t*)container,typecode);
        case SHARED_CONTAINER_TYPE_CODE:
            assert(false);
    }
    assert(false);
    __builtin_unreachable();
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
static inline int32_t container_write(const void *container, uint8_t typecode,
                                      char *buf) {
	container = container_unwrap_shared(container,&typecode);
	switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_write((bitset_container_t *)container, buf);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_write((array_container_t *)container, buf);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_write((run_container_t *)container, buf);
    }
    assert(false);
    __builtin_unreachable();
    return 0;  // unreached
}

/**
 * Get the container size in bytes under portable serialization (see
 * container_write), requires a
 * typecode
 */
static inline int32_t container_size_in_bytes(const void *container,
                                              uint8_t typecode) {
	container = container_unwrap_shared(container,&typecode);
	switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_size_in_bytes(
                (bitset_container_t *)container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_size_in_bytes(
                (array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_size_in_bytes((run_container_t *)container);
    }
    assert(false);
    __builtin_unreachable();
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
	container = container_unwrap_shared(container,&typecode);
	switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_nonzero_cardinality(
                (bitset_container_t *)container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_nonzero_cardinality(
                (array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_nonzero_cardinality(
                (run_container_t *)container);
    }
    assert(false);
    __builtin_unreachable();
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
        case SHARED_CONTAINER_TYPE_CODE:
        	shared_container_free((shared_container_t *)container);
        	break;
        default:
            assert(false);
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
	container = container_unwrap_shared(container,&typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_to_uint32_array(
                output, (bitset_container_t *)container, base);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_to_uint32_array(
                output, (array_container_t *)container, base);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_to_uint32_array(
                output, (run_container_t *)container, base);
    }
    assert(false);
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
	container = get_writable_copy_if_shared(container,&typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_set((bitset_container_t *)container, val);
            *new_typecode = BITSET_CONTAINER_TYPE_CODE;
            return container;
        case ARRAY_CONTAINER_TYPE_CODE: {
            array_container_t *ac = (array_container_t *)container;
            array_container_add(ac, val);
            if (array_container_cardinality(ac) > DEFAULT_MAX_SIZE) {
                *new_typecode = BITSET_CONTAINER_TYPE_CODE;
                return bitset_container_from_array(ac);
            } else {
                *new_typecode = ARRAY_CONTAINER_TYPE_CODE;
                return ac;
            }
        } break;
        case RUN_CONTAINER_TYPE_CODE:
            // per Java, no container type adjustments are done (revisit?)
            run_container_add((run_container_t *)container, val);
            *new_typecode = RUN_CONTAINER_TYPE_CODE;
            return container;
        default:
            assert(false);
            __builtin_unreachable();
            return NULL;
    }
}

/**
 * Check whether a value is in a container, requires a  typecode
 */
static inline bool container_contains(const void *container, uint16_t val,
                                      uint8_t typecode) {
	container = container_unwrap_shared(container,&typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_get((const bitset_container_t *)container,
                                        val);
        case ARRAY_CONTAINER_TYPE_CODE:
            ;
            return array_container_contains(
                (const array_container_t *)container, val);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_contains((const run_container_t *)container,
                                          val);
        default:
            assert(false);
            __builtin_unreachable();
            return false;
    }
}


int32_t container_serialize(const void *container, uint8_t typecode,
                            char *buf) WARN_UNUSED;

uint32_t container_serialization_len(const void *container, uint8_t typecode);

void *container_deserialize(uint8_t typecode, const char *buf, size_t buf_len);



/**
 * Returns true if the two containers have the same content. Note that
 * two containers having different types can be "equal" in this sense.
 */
static inline bool container_equals(const void *c1, uint8_t type1,
                                    const void *c2, uint8_t type2) {
	c1 = container_unwrap_shared(c1,&type1);
	c2 = container_unwrap_shared(c2,&type2);
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
            assert(false);
            __builtin_unreachable();
            return false;
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
	c1 = container_unwrap_shared(c1,&type1);
	c2 = container_unwrap_shared(c2,&type2);
	void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = bitset_bitset_container_intersection(
                               (const bitset_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            result = array_container_create();
            array_container_intersection((const array_container_t *)c1,
                                         (const array_container_t *)c2,
                                         (array_container_t *)result);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            run_container_intersection((const run_container_t *)c1,
                                       (const run_container_t *)c2,
                                       (run_container_t *)result);
            return convert_run_to_efficient_container_and_free(
                (run_container_t *)result, result_type);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            result = array_container_create();
            array_bitset_container_intersection((const array_container_t *)c2,
                                                (const bitset_container_t *)c1,
                                                (array_container_t *)result);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_bitset_container_intersection((const array_container_t *)c1,
                                                (const bitset_container_t *)c2,
                                                (array_container_t *)result);
            return result;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            *result_type = run_bitset_container_intersection(
                               (const run_container_t *)c2,
                               (const bitset_container_t *)c1, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = run_bitset_container_intersection(
                               (const run_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_run_container_intersection((const array_container_t *)c1,
                                             (const run_container_t *)c2,
                                             (array_container_t *)result);
            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_run_container_intersection((const array_container_t *)c2,
                                             (const run_container_t *)c1,
                                             (array_container_t *)result);
            return result;
        default:
            assert(false);
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
	c1 = get_writable_copy_if_shared(c1,&type1);
	c2 = container_unwrap_shared(c2,&type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type =
                bitset_bitset_container_intersection_inplace(
                    (bitset_container_t *)c1, (bitset_container_t *)c2, &result)
                    ? BITSET_CONTAINER_TYPE_CODE
                    : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            array_container_intersection_inplace((array_container_t *)c1,
                                                 (const array_container_t *)c2);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            run_container_intersection((const run_container_t *)c1,
                                       (const run_container_t *)c2,
                                       (run_container_t *)result);
            // as of January 2016, Java code used non-in-place intersection for
            // two runcontainers
            return convert_run_to_efficient_container_and_free(
                (run_container_t *)result, result_type);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            // c1 is a bitmap so no inplace possible
            result = array_container_create();
            array_bitset_container_intersection((const array_container_t *)c2,
                                                (const bitset_container_t *)c1,
                                                (array_container_t *)result);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_bitset_container_intersection(
                (const array_container_t *)c1, (const bitset_container_t *)c2,
                (array_container_t *)c1);  // allowed
            return c1;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            // will attempt in-place computation
            *result_type = run_bitset_container_intersection(
                               (const run_container_t *)c2,
                               (const bitset_container_t *)c1, &c1)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = run_bitset_container_intersection(
                               (const run_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_run_container_intersection((const array_container_t *)c1,
                                             (const run_container_t *)c2,
                                             (array_container_t *)result);
            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            result = array_container_create();
            *result_type = ARRAY_CONTAINER_TYPE_CODE;  // never bitset
            array_run_container_intersection((const array_container_t *)c2,
                                             (const run_container_t *)c1,
                                             (array_container_t *)result);
            return result;
        default:
            assert(false);
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
	c1 = container_unwrap_shared(c1,&type1);
	c2 = container_unwrap_shared(c2,&type2);
	void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            bitset_container_or((const bitset_container_t *)c1,
                                (const bitset_container_t *)c2,
                                (bitset_container_t *)result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = array_array_container_union(
                               (const array_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            run_container_union((const run_container_t *)c1,
                                (const run_container_t *)c2,
                                (run_container_t *)result);
            *result_type = RUN_CONTAINER_TYPE_CODE;
            // todo: could be optimized since will never convert to array
            result = convert_run_to_efficient_container_and_free(
                (run_container_t *)result, (uint8_t *)result_type);
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            array_bitset_container_union((const array_container_t *)c2,
                                         (const bitset_container_t *)c1,
                                         (bitset_container_t *)result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            array_bitset_container_union((const array_container_t *)c1,
                                         (const bitset_container_t *)c2,
                                         (bitset_container_t *)result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            if (run_container_is_full((run_container_t *)c2)) {
                result = run_container_create();
                *result_type = RUN_CONTAINER_TYPE_CODE;
                run_container_copy((const run_container_t *)c2,
                                   (run_container_t *)result);
                return result;
            }
            result = bitset_container_create();
            run_bitset_container_union((const run_container_t *)c2,
                                       (const bitset_container_t *)c1,
                                       (bitset_container_t *)result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c1)) {
                result = run_container_create();
                *result_type = RUN_CONTAINER_TYPE_CODE;
                run_container_copy((const run_container_t *)c1,
                                   (run_container_t *)result);
                return result;
            }
            result = bitset_container_create();
            run_bitset_container_union((const run_container_t *)c1,
                                       (const bitset_container_t *)c2,
                                       (bitset_container_t *)result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_union((const array_container_t *)c1,
                                      (const run_container_t *)c2,
                                      (run_container_t *)result);
            result = convert_run_to_efficient_container_and_free(
                (run_container_t *)result, (uint8_t *)result_type);
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_union((const array_container_t *)c2,
                                      (const run_container_t *)c1,
                                      (run_container_t *)result);
            result = convert_run_to_efficient_container_and_free(
                (run_container_t *)result, (uint8_t *)result_type);
            return result;
        default:
            assert(false);
            __builtin_unreachable();
            return NULL;  // unreached
    }
}

/**
 * Compute union between two containers, generate a new container (having type
 * result_type), requires a typecode. This allocates new memory, caller
 * is responsible for deallocation.
 *
 * This lazy version delays some operations such as the maintenance of the
 * cardinality. It requires repair later on the generated containers.
 */
static inline void *container_lazy_or(const void *c1, uint8_t type1,
                                      const void *c2, uint8_t type2,
                                      uint8_t *result_type) {
	c1 = container_unwrap_shared(c1,&type1);
	c2 = container_unwrap_shared(c2,&type2);
	void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            bitset_container_or_nocard(
                (const bitset_container_t *)c1, (const bitset_container_t *)c2,
                (bitset_container_t *)result);  // is lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = array_array_container_lazy_union(
                               (const array_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            run_container_union((const run_container_t *)c1,
                                (const run_container_t *)c2,
                                (run_container_t *)result);
            *result_type = RUN_CONTAINER_TYPE_CODE;
            // we are being lazy
            result = convert_run_to_efficient_container(
                (run_container_t *)result, result_type);
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            array_bitset_container_lazy_union(
                (const array_container_t *)c2, (const bitset_container_t *)c1,
                (bitset_container_t *)result);  // is lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            array_bitset_container_lazy_union(
                (const array_container_t *)c1, (const bitset_container_t *)c2,
                (bitset_container_t *)result);  // is lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c2)) {
                result = run_container_create();
                *result_type = RUN_CONTAINER_TYPE_CODE;
                run_container_copy((const run_container_t *)c2,
                                   (run_container_t *)result);
                return result;
            }
            result = bitset_container_create();
            run_bitset_container_lazy_union(
                (const run_container_t *)c2, (const bitset_container_t *)c1,
                (bitset_container_t *)result);  // is lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c1)) {
                result = run_container_create();
                *result_type = RUN_CONTAINER_TYPE_CODE;
                run_container_copy((const run_container_t *)c1,
                                   (run_container_t *)result);
                return result;
            }
            result = bitset_container_create();
            run_bitset_container_lazy_union(
                (const run_container_t *)c1, (const bitset_container_t *)c2,
                (bitset_container_t *)result);  // is lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_union((const array_container_t *)c1,
                                      (const run_container_t *)c2,
                                      (run_container_t *)result);
            *result_type = RUN_CONTAINER_TYPE_CODE;
            // next line skipped since we are lazy
            // result = convert_run_to_efficient_container(result, result_type);
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_union(
                (const array_container_t *)c2, (const run_container_t *)c1,
                (run_container_t *)result);  // TODO make lazy
            *result_type = RUN_CONTAINER_TYPE_CODE;
            // next line skipped since we are lazy
            // result = convert_run_to_efficient_container(result, result_type);
            return result;
        default:
            assert(false);
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
	c1 = get_writable_copy_if_shared(c1,&type1);
	c2 = container_unwrap_shared(c2,&type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            bitset_container_or((const bitset_container_t *)c1,
                                (const bitset_container_t *)c2,
                                (bitset_container_t *)c1);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            // Java impl. also does not do real in-place in this case
            *result_type = array_array_container_union(
                               (const array_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            run_container_union_inplace((run_container_t *)c1,
                                        (const run_container_t *)c2);
            return convert_run_to_efficient_container((run_container_t *)c1,
                                                      result_type);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            array_bitset_container_union((const array_container_t *)c2,
                                         (const bitset_container_t *)c1,
                                         (bitset_container_t *)c1);
            *result_type = BITSET_CONTAINER_TYPE_CODE;  // never array
            return c1;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            // c1 is an array, so no in-place possible
            result = bitset_container_create();
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            array_bitset_container_union((const array_container_t *)c1,
                                         (const bitset_container_t *)c2,
                                         (bitset_container_t *)result);
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c2)) {
                result = run_container_create();
                *result_type = RUN_CONTAINER_TYPE_CODE;
                run_container_copy((const run_container_t *)c2,
                                   (run_container_t *)result);
                return result;
            }
            run_bitset_container_union((const run_container_t *)c2,
                                       (const bitset_container_t *)c1,
                                       (bitset_container_t *)c1);  // allowed
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c1)) {
                *result_type = RUN_CONTAINER_TYPE_CODE;

                return c1;
            }
            result = bitset_container_create();
            run_bitset_container_union((const run_container_t *)c1,
                                       (const bitset_container_t *)c2,
                                       (bitset_container_t *)result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_union((const array_container_t *)c1,
                                      (const run_container_t *)c2,
                                      (run_container_t *)result);
            result = convert_run_to_efficient_container_and_free(
                (run_container_t *)result, result_type);
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            array_run_container_inplace_union((const array_container_t *)c2,
                                              (run_container_t *)c1);
            c1 = convert_run_to_efficient_container((run_container_t *)c1,
                                                    result_type);
            return c1;
        default:
            assert(false);
            __builtin_unreachable();
            return NULL;
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
 *
 * This lazy version delays some operations such as the maintenance of the
 * cardinality. It requires repair later on the generated containers.
*/
static inline void *container_lazy_ior(void *c1, uint8_t type1, const void *c2,
                                       uint8_t type2, uint8_t *result_type) {
  assert(type1 != SHARED_CONTAINER_TYPE_CODE);
	//c1 = get_writable_copy_if_shared(c1,&type1);
	c2 = container_unwrap_shared(c2,&type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            bitset_container_or_nocard((const bitset_container_t *)c1,
                                       (const bitset_container_t *)c2,
                                       (bitset_container_t *)c1);  // is lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            // Java impl. also does not do real in-place in this case
            *result_type = array_array_container_lazy_union(
                               (const array_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            run_container_union_inplace((run_container_t *)c1,
                                        (const run_container_t *)c2);
            *result_type = RUN_CONTAINER_TYPE_CODE;
            return convert_run_to_efficient_container((run_container_t *)c1,
                                                      result_type);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            array_bitset_container_lazy_union(
                (const array_container_t *)c2, (const bitset_container_t *)c1,
                (bitset_container_t *)c1);              // is lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;  // never array
            return c1;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            // c1 is an array, so no in-place possible
            result = bitset_container_create();
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            array_bitset_container_lazy_union(
                (const array_container_t *)c1, (const bitset_container_t *)c2,
                (bitset_container_t *)result);  // is lazy
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c2)) {
                result = run_container_create();
                *result_type = RUN_CONTAINER_TYPE_CODE;
                run_container_copy((const run_container_t *)c2,
                                   (run_container_t *)result);
                return result;
            }
            run_bitset_container_lazy_union(
                (const run_container_t *)c2, (const bitset_container_t *)c1,
                (bitset_container_t *)c1);  // allowed //  lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c1)) {
                *result_type = RUN_CONTAINER_TYPE_CODE;
                return c1;
            }
            result = bitset_container_create();
            run_bitset_container_lazy_union(
                (const run_container_t *)c1, (const bitset_container_t *)c2,
                (bitset_container_t *)result);  //  lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_union((const array_container_t *)c1,
                                      (const run_container_t *)c2,
                                      (run_container_t *)result);
            *result_type = RUN_CONTAINER_TYPE_CODE;
            // next line skipped since we are lazy
            // result = convert_run_to_efficient_container_and_free(result,
            // result_type);
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            array_run_container_inplace_union((const array_container_t *)c2,
                                              (run_container_t *)c1);
            *result_type = RUN_CONTAINER_TYPE_CODE;
            // next line skipped since we are lazy
            // result = convert_run_to_efficient_container_and_free(result,
            // result_type);
            return c1;
        default:
            assert(false);
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
	container = container_unwrap_shared(container,&typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_iterate((const bitset_container_t *)container,
                                     base, iterator, ptr);
            break;
        case ARRAY_CONTAINER_TYPE_CODE:
            array_container_iterate((const array_container_t *)container, base,
                                    iterator, ptr);
            break;
        case RUN_CONTAINER_TYPE_CODE:
            run_container_iterate((const run_container_t *)container, base,
                                  iterator, ptr);
            break;
        default:
            assert(false);
            __builtin_unreachable();
    }
}

static inline void *container_not(const void *c, uint8_t typ,
                                  uint8_t *result_type) {
    void *result = NULL;
    switch (typ) {
        case BITSET_CONTAINER_TYPE_CODE:
            *result_type = bitset_container_negation(c, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case ARRAY_CONTAINER_TYPE_CODE:
            result = bitset_container_create();
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            array_container_negation(c, result);
            return result;
        case RUN_CONTAINER_TYPE_CODE:
            *result_type = run_container_negation(c, &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
    }
}

static inline void *container_not_range(const void *c, uint8_t typ,
                                        uint32_t range_start,
                                        uint32_t range_end,
                                        uint8_t *result_type) {
    void *result = NULL;
    switch (typ) {
        case BITSET_CONTAINER_TYPE_CODE:
            *result_type = bitset_container_negation_range(c, range_start,
                                                           range_end, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case ARRAY_CONTAINER_TYPE_CODE:
            *result_type = array_container_negation_range(c, range_start,
                                                          range_end, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case RUN_CONTAINER_TYPE_CODE:
            *result_type = run_container_negation_range(c, range_start,
                                                        range_end, &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
    }
}

static inline void *container_inot(void *c, uint8_t typ, uint8_t *result_type) {
    void *result = NULL;
    switch (typ) {
        case BITSET_CONTAINER_TYPE_CODE:
            *result_type = bitset_container_negation_inplace(c, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case ARRAY_CONTAINER_TYPE_CODE:
            // will never be inplace
            result = bitset_container_create();
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            array_container_negation(c, result);
            array_container_free(c);
            return result;
        case RUN_CONTAINER_TYPE_CODE:
            *result_type = run_container_negation_inplace(c, &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
    }
}

static inline void *container_inot_range(void *c, uint8_t typ,
                                         uint32_t range_start,
                                         uint32_t range_end,
                                         uint8_t *result_type) {
    void *result = NULL;
    switch (typ) {
        case BITSET_CONTAINER_TYPE_CODE:
            *result_type = bitset_container_negation_range_inplace(
                               c, range_start, range_end, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case ARRAY_CONTAINER_TYPE_CODE:
            *result_type = array_container_negation_range_inplace(
                               c, range_start, range_end, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case RUN_CONTAINER_TYPE_CODE:
            *result_type = run_container_negation_range_inplace(
                c, range_start, range_end, &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
    }
}

/**
 * make a container with a run of ones
 */
/* initially always use a run container, even if an array might be marginally
 * smaller */
static inline void *container_range_of_ones(uint32_t range_start,
                                            uint32_t range_end,
                                            uint8_t *result_type) {
    *result_type = RUN_CONTAINER_TYPE_CODE;
    return run_container_create_range(range_start, range_end);
}

#endif
