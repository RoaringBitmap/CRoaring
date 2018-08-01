#ifndef CONTAINERS_CONTAINERS_H
#define CONTAINERS_CONTAINERS_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/convert.h>
#include <roaring/containers/mixed_andnot.h>
#include <roaring/containers/mixed_equal.h>
#include <roaring/containers/mixed_intersection.h>
#include <roaring/containers/mixed_negation.h>
#include <roaring/containers/mixed_subset.h>
#include <roaring/containers/mixed_union.h>
#include <roaring/containers/mixed_xor.h>
#include <roaring/containers/run.h>
#include <roaring/bitset_util.h>

// would enum be possible or better?

/**
 * The switch case statements follow
 * BITSET_CONTAINER_TYPE_CODE -- ARRAY_CONTAINER_TYPE_CODE --
 * RUN_CONTAINER_TYPE_CODE
 * so it makes more sense to number them 1, 2, 3 (in the vague hope that the
 * compiler might exploit this ordering).
 */

#define BITSET_CONTAINER_TYPE_CODE 1
#define ARRAY_CONTAINER_TYPE_CODE 2
#define RUN_CONTAINER_TYPE_CODE 3
#define SHARED_CONTAINER_TYPE_CODE 4

// macro for pairing container type codes
#define CONTAINER_PAIR(c1, c2) (4 * (c1) + (c2))

/**
 * A shared container is a wrapper around a container
 * with reference counting.
 */

struct shared_container_s {
    void *container;
    uint8_t typecode;
    uint32_t counter;  // to be managed atomically
};

typedef struct shared_container_s shared_container_t;

/*
 * With copy_on_write = true
 *  Create a new shared container if the typecode is not SHARED_CONTAINER_TYPE,
 * otherwise, increase the count
 * If copy_on_write = false, then clone.
 * Return NULL in case of failure.
 **/
void *get_copy_of_container(void *container, uint8_t *typecode,
                            bool copy_on_write);

/* Frees a shared container (actually decrement its counter and only frees when
 * the counter falls to zero). */
void shared_container_free(shared_container_t *container);

/* extract a copy from the shared container, freeing the shared container if
there is just one instance left,
clone instances when the counter is higher than one
*/
void *shared_container_extract_copy(shared_container_t *container,
                                    uint8_t *typecode);

/* access to container underneath */
inline const void *container_unwrap_shared(
    const void *candidate_shared_container, uint8_t *type) {
    if (*type == SHARED_CONTAINER_TYPE_CODE) {
        *type =
            ((const shared_container_t *)candidate_shared_container)->typecode;
        assert(*type != SHARED_CONTAINER_TYPE_CODE);
        return ((const shared_container_t *)candidate_shared_container)->container;
    } else {
        return candidate_shared_container;
    }
}


/* access to container underneath */
inline void *container_mutable_unwrap_shared(
    void *candidate_shared_container, uint8_t *type) {
    if (*type == SHARED_CONTAINER_TYPE_CODE) {
        *type =
            ((shared_container_t *)candidate_shared_container)->typecode;
        assert(*type != SHARED_CONTAINER_TYPE_CODE);
        return ((shared_container_t *)candidate_shared_container)->container;
    } else {
        return candidate_shared_container;
    }
}

/* access to container underneath and queries its type */
static inline uint8_t get_container_type(const void *container, uint8_t type) {
    if (type == SHARED_CONTAINER_TYPE_CODE) {
        return ((const shared_container_t *)container)->typecode;
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
static inline void *get_writable_copy_if_shared(
    void *candidate_shared_container, uint8_t *type) {
    if (*type == SHARED_CONTAINER_TYPE_CODE) {
        return shared_container_extract_copy(
            (shared_container_t *)candidate_shared_container, type);
    } else {
        return candidate_shared_container;
    }
}

/**
 * End of shared container code
 */

static const char *container_names[] = {"bitset", "array", "run", "shared"};
static const char *shared_container_names[] = {
    "bitset (shared)", "array (shared)", "run (shared)"};

// no matter what the initial container was, convert it to a bitset
// if a new container is produced, caller responsible for freeing the previous
// one
// container should not be a shared container
static inline void *container_to_bitset(void *container, uint8_t typecode) {
    bitset_container_t *result = NULL;
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return container;  // nothing to do
        case ARRAY_CONTAINER_TYPE_CODE:
            result =
                bitset_container_from_array((array_container_t *)container);
            return result;
        case RUN_CONTAINER_TYPE_CODE:
            result = bitset_container_from_run((run_container_t *)container);
            return result;
        case SHARED_CONTAINER_TYPE_CODE:
            assert(false);
    }
    assert(false);
    __builtin_unreachable();
    return 0;  // unreached
}

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

static inline const char *get_full_container_name(const void *container,
                                                  uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return container_names[0];
        case ARRAY_CONTAINER_TYPE_CODE:
            return container_names[1];
        case RUN_CONTAINER_TYPE_CODE:
            return container_names[2];
        case SHARED_CONTAINER_TYPE_CODE:
            switch (((const shared_container_t *)container)->typecode) {
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
    return NULL;
}

/**
 * Get the container cardinality (number of elements), requires a  typecode
 */
static inline int container_get_cardinality(const void *container,
                                            uint8_t typecode) {
    container = container_unwrap_shared(container, &typecode);
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



// returns true if a container is known to be full. Note that a lazy bitset
// container
// might be full without us knowing
static inline bool container_is_full(const void *container, uint8_t typecode) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_cardinality(
                       (const bitset_container_t *)container) == (1 << 16);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_cardinality(
                       (const array_container_t *)container) == (1 << 16);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_is_full((const run_container_t *)container);
    }
    assert(false);
    __builtin_unreachable();
    return 0;  // unreached
}

static inline int container_shrink_to_fit(void *container, uint8_t typecode) {
    container = container_mutable_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return 0;  // no shrinking possible
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_shrink_to_fit(
                (array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_shrink_to_fit((run_container_t *)container);
    }
    assert(false);
    __builtin_unreachable();
    return 0;  // unreached
}


/**
 * make a container with a run of ones
 */
/* initially always use a run container, even if an array might be
 * marginally
 * smaller */
static inline void *container_range_of_ones(uint32_t range_start,
                                            uint32_t range_end,
                                            uint8_t *result_type) {
    assert(range_end >= range_start);
    uint64_t cardinality =  range_end - range_start + 1;
    if(cardinality <= 2) {
      *result_type = ARRAY_CONTAINER_TYPE_CODE;
      return array_container_create_range(range_start, range_end);
    } else {
      *result_type = RUN_CONTAINER_TYPE_CODE;
      return run_container_create_range(range_start, range_end);
    }
}


/*  Create a container with all the values between in [min,max) at a
    distance k*step from min. */
static inline void *container_from_range(uint8_t *type, uint32_t min,
                                         uint32_t max, uint16_t step) {
    if (step == 0) return NULL;  // being paranoid
    if (step == 1) {
        return container_range_of_ones(min,max,type);
        // Note: the result is not always a run (need to check the cardinality)
        //*type = RUN_CONTAINER_TYPE_CODE;
        //return run_container_create_range(min, max);
    }
    int size = (max - min + step - 1) / step;
    if (size <= DEFAULT_MAX_SIZE) {  // array container
        *type = ARRAY_CONTAINER_TYPE_CODE;
        array_container_t *array = array_container_create_given_capacity(size);
        array_container_add_from_range(array, min, max, step);
        assert(array->cardinality == size);
        return array;
    } else {  // bitset container
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
    container = get_writable_copy_if_shared(
        container, typecode);  // TODO: this introduces unnecessary cloning
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
                (run_container_t *)container, typecode);
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
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_write((const bitset_container_t *)container, buf);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_write((const array_container_t *)container, buf);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_write((const run_container_t *)container, buf);
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
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_size_in_bytes(
                (const bitset_container_t *)container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_size_in_bytes(
                (const array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_size_in_bytes((const run_container_t *)container);
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
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_const_nonzero_cardinality(
                (const bitset_container_t *)container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_nonzero_cardinality(
                (const array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_nonzero_cardinality(
                (const run_container_t *)container);
    }
    assert(false);
    __builtin_unreachable();
    return 0;  // unreached
}

/**
 * Recover memory from a container, requires a  typecode
 */
void container_free(void *container, uint8_t typecode);

/**
 * Convert a container to an array of values, requires a  typecode as well as a
 * "base" (most significant values)
 * Returns number of ints added.
 */
static inline int container_to_uint32_array(uint32_t *output,
                                            const void *container,
                                            uint8_t typecode, uint32_t base) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_to_uint32_array(
                output, (const bitset_container_t *)container, base);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_to_uint32_array(
                output, (const array_container_t *)container, base);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_to_uint32_array(
                output, (const run_container_t *)container, base);
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
    container = get_writable_copy_if_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_set((bitset_container_t *)container, val);
            *new_typecode = BITSET_CONTAINER_TYPE_CODE;
            return container;
        case ARRAY_CONTAINER_TYPE_CODE: {
            array_container_t *ac = (array_container_t *)container;
            if (array_container_try_add(ac, val, DEFAULT_MAX_SIZE) != -1) {
                *new_typecode = ARRAY_CONTAINER_TYPE_CODE;
                return ac;
            } else {
                bitset_container_t* bitset = bitset_container_from_array(ac);
                bitset_container_add(bitset, val);
                *new_typecode = BITSET_CONTAINER_TYPE_CODE;
                return bitset;
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
 * Remove a value from a container, requires a  typecode, fills in new_typecode
 * and
 * return (possibly different) container.
 * This function may allocate a new container, and caller is responsible for
 * memory deallocation
 */
static inline void *container_remove(void *container, uint16_t val,
                                     uint8_t typecode, uint8_t *new_typecode) {
    container = get_writable_copy_if_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            if (bitset_container_remove((bitset_container_t *)container, val)) {
                if (bitset_container_cardinality(
                        (bitset_container_t *)container) <= DEFAULT_MAX_SIZE) {
                    *new_typecode = ARRAY_CONTAINER_TYPE_CODE;
                    return array_container_from_bitset(
                        (bitset_container_t *)container);
                }
            }
            *new_typecode = typecode;
            return container;
        case ARRAY_CONTAINER_TYPE_CODE:
            *new_typecode = typecode;
            array_container_remove((array_container_t *)container, val);
            return container;
        case RUN_CONTAINER_TYPE_CODE:
            // per Java, no container type adjustments are done (revisit?)
            run_container_remove((run_container_t *)container, val);
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
inline bool container_contains(const void *container, uint16_t val,
                               uint8_t typecode) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_get((const bitset_container_t *)container,
                                        val);
        case ARRAY_CONTAINER_TYPE_CODE:
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

/**
 * Check whether a range of values from range_start (included) to range_end (excluded)
 * is in a container, requires a typecode
 */
static inline bool container_contains_range(const void *container, uint32_t range_start,
					uint32_t range_end, uint8_t typecode) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_get_range((const bitset_container_t *)container,
                                                range_start, range_end);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_contains_range((const array_container_t *)container,
                                                    range_start, range_end);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_contains_range((const run_container_t *)container,
                                                    range_start, range_end);
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
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return bitset_container_equals((const bitset_container_t *)c1,
                                           (const bitset_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            return run_container_equals_bitset((const run_container_t *)c2,
                                               (const bitset_container_t *)c1);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return run_container_equals_bitset((const run_container_t *)c1,
                                               (const bitset_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            // java would always return false?
            return array_container_equal_bitset((const array_container_t *)c2,
                                                (const bitset_container_t *)c1);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            // java would always return false?
            return array_container_equal_bitset((const array_container_t *)c1,
                                                (const bitset_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return run_container_equals_array((const run_container_t *)c2,
                                              (const array_container_t *)c1);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            return run_container_equals_array((const run_container_t *)c1,
                                              (const array_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            return array_container_equals((const array_container_t *)c1,
                                          (const array_container_t *)c2);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return run_container_equals((const run_container_t *)c1,
                                        (const run_container_t *)c2);
        default:
            assert(false);
            __builtin_unreachable();
            return false;
    }
}

/**
 * Returns true if the container c1 is a subset of the container c2. Note that
 * c1 can be a subset of c2 even if they have a different type.
 */
static inline bool container_is_subset(const void *c1, uint8_t type1,
                                       const void *c2, uint8_t type2) {
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return bitset_container_is_subset((const bitset_container_t *)c1,
                                              (const bitset_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            return bitset_container_is_subset_run((const bitset_container_t *)c1,
                                                  (const run_container_t *)c2);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return run_container_is_subset_bitset((const run_container_t *)c1,
                                                  (const bitset_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            return false;  // by construction, size(c1) > size(c2)
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return array_container_is_subset_bitset((const array_container_t *)c1,
                                                    (const bitset_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return array_container_is_subset_run((const array_container_t *)c1,
                                                 (const run_container_t *)c2);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            return run_container_is_subset_array((const run_container_t *)c1,
                                                 (const array_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            return array_container_is_subset((const array_container_t *)c1,
                                             (const array_container_t *)c2);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return run_container_is_subset((const run_container_t *)c1,
                                           (const run_container_t *)c2);
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
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
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
 * Compute the size of the intersection between two containers.
 */
static inline int container_and_cardinality(const void *c1, uint8_t type1,
                                            const void *c2, uint8_t type2) {
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return bitset_container_and_justcard(
                (const bitset_container_t *)c1, (const bitset_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            return array_container_intersection_cardinality(
                (const array_container_t *)c1, (const array_container_t *)c2);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return run_container_intersection_cardinality(
                (const run_container_t *)c1, (const run_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            return array_bitset_container_intersection_cardinality(
                (const array_container_t *)c2, (const bitset_container_t *)c1);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return array_bitset_container_intersection_cardinality(
                (const array_container_t *)c1, (const bitset_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            return run_bitset_container_intersection_cardinality(
                (const run_container_t *)c2, (const bitset_container_t *)c1);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return run_bitset_container_intersection_cardinality(
                (const run_container_t *)c1, (const bitset_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return array_run_container_intersection_cardinality(
                (const array_container_t *)c1, (const run_container_t *)c2);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            return array_run_container_intersection_cardinality(
                (const array_container_t *)c2, (const run_container_t *)c1);
        default:
            assert(false);
            __builtin_unreachable();
            return 0;
    }
}

/**
 * Check whether two containers intersect.
 */
static inline bool container_intersect(const void *c1, uint8_t type1, const void *c2,
                                  uint8_t type2) {
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return bitset_container_intersect(
                               (const bitset_container_t *)c1,
                               (const bitset_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            return array_container_intersect((const array_container_t *)c1,
                                         (const array_container_t *)c2);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return run_container_intersect((const run_container_t *)c1,
                                       (const run_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            return array_bitset_container_intersect((const array_container_t *)c2,
                                                (const bitset_container_t *)c1);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return array_bitset_container_intersect((const array_container_t *)c1,
                                                (const bitset_container_t *)c2);
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            return run_bitset_container_intersect(
                               (const run_container_t *)c2,
                               (const bitset_container_t *)c1);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            return run_bitset_container_intersect(
                               (const run_container_t *)c1,
                               (const bitset_container_t *)c2);
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            return array_run_container_intersect((const array_container_t *)c1,
                                             (const run_container_t *)c2);
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            return array_run_container_intersect((const array_container_t *)c2,
                                             (const run_container_t *)c1);
        default:
            assert(false);
            __builtin_unreachable();
            return 0;
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
    c1 = get_writable_copy_if_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type =
                bitset_bitset_container_intersection_inplace(
                    (bitset_container_t *)c1, (const bitset_container_t *)c2, &result)
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
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
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
            if (run_container_is_full((const run_container_t *)c2)) {
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
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
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
    c1 = get_writable_copy_if_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            bitset_container_or((const bitset_container_t *)c1,
                                (const bitset_container_t *)c2,
                                (bitset_container_t *)c1);
#ifdef OR_BITSET_CONVERSION_TO_FULL
            if (((bitset_container_t *)c1)->cardinality ==
                (1 << 16)) {  // we convert
                result = run_container_create_range(0, (1 << 16));
                *result_type = RUN_CONTAINER_TYPE_CODE;
                return result;
            }
#endif
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = array_array_container_inplace_union(
                               (array_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            if((result == NULL)
               && (*result_type == ARRAY_CONTAINER_TYPE_CODE)) {
                 return c1; // the computation was done in-place!
            }
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
    // c1 = get_writable_copy_if_shared(c1,&type1);
    c2 = container_unwrap_shared(c2, &type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
#ifdef LAZY_OR_BITSET_CONVERSION_TO_FULL
            // if we have two bitsets, we might as well compute the cardinality
            bitset_container_or((const bitset_container_t *)c1,
                                (const bitset_container_t *)c2,
                                (bitset_container_t *)c1);
            // it is possible that two bitsets can lead to a full container
            if (((bitset_container_t *)c1)->cardinality ==
                (1 << 16)) {  // we convert
                result = run_container_create_range(0, (1 << 16));
                *result_type = RUN_CONTAINER_TYPE_CODE;
                return result;
            }
#else
            bitset_container_or_nocard((const bitset_container_t *)c1,
                                       (const bitset_container_t *)c2,
                                       (bitset_container_t *)c1);

#endif
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return c1;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = array_array_container_lazy_inplace_union(
                               (array_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            if((result == NULL)
               && (*result_type == ARRAY_CONTAINER_TYPE_CODE)) {
                 return c1; // the computation was done in-place!
            }
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
 * Compute symmetric difference (xor) between two containers, generate a new
 * container (having type result_type), requires a typecode. This allocates new
 * memory, caller is responsible for deallocation.
 */
static inline void *container_xor(const void *c1, uint8_t type1, const void *c2,
                                  uint8_t type2, uint8_t *result_type) {
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = bitset_bitset_container_xor(
                               (const bitset_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = array_array_container_xor(
                               (const array_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            *result_type =
                run_run_container_xor((const run_container_t *)c1,
                                      (const run_container_t *)c2, &result);
            return result;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = array_bitset_container_xor(
                               (const array_container_t *)c2,
                               (const bitset_container_t *)c1, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = array_bitset_container_xor(
                               (const array_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            *result_type = run_bitset_container_xor(
                               (const run_container_t *)c2,
                               (const bitset_container_t *)c1, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):

            *result_type = run_bitset_container_xor(
                               (const run_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;

        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            *result_type =
                array_run_container_xor((const array_container_t *)c1,
                                        (const run_container_t *)c2, &result);
            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            *result_type =
                array_run_container_xor((const array_container_t *)c2,
                                        (const run_container_t *)c1, &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
            return NULL;  // unreached
    }
}

/**
 * Compute xor between two containers, generate a new container (having type
 * result_type), requires a typecode. This allocates new memory, caller
 * is responsible for deallocation.
 *
 * This lazy version delays some operations such as the maintenance of the
 * cardinality. It requires repair later on the generated containers.
 */
static inline void *container_lazy_xor(const void *c1, uint8_t type1,
                                       const void *c2, uint8_t type2,
                                       uint8_t *result_type) {
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            bitset_container_xor_nocard(
                (const bitset_container_t *)c1, (const bitset_container_t *)c2,
                (bitset_container_t *)result);  // is lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = array_array_container_lazy_xor(
                               (const array_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            // nothing special done yet.
            *result_type =
                run_run_container_xor((const run_container_t *)c1,
                                      (const run_container_t *)c2, &result);
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            array_bitset_container_lazy_xor((const array_container_t *)c2,
                                            (const bitset_container_t *)c1,
                                            (bitset_container_t *)result);
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            array_bitset_container_lazy_xor((const array_container_t *)c1,
                                            (const bitset_container_t *)c2,
                                            (bitset_container_t *)result);
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            run_bitset_container_lazy_xor((const run_container_t *)c2,
                                          (const bitset_container_t *)c1,
                                          (bitset_container_t *)result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = bitset_container_create();
            run_bitset_container_lazy_xor((const run_container_t *)c1,
                                          (const bitset_container_t *)c2,
                                          (bitset_container_t *)result);
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return result;

        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_lazy_xor((const array_container_t *)c1,
                                         (const run_container_t *)c2,
                                         (run_container_t *)result);
            *result_type = RUN_CONTAINER_TYPE_CODE;
            // next line skipped since we are lazy
            // result = convert_run_to_efficient_container(result, result_type);
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            result = run_container_create();
            array_run_container_lazy_xor((const array_container_t *)c2,
                                         (const run_container_t *)c1,
                                         (run_container_t *)result);
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
 * Compute the xor between two containers, with result in the first container.
 * If the returned pointer is identical to c1, then the container has been
 * modified.
 * If the returned pointer is different from c1, then a new container has been
 * created and the caller is responsible for freeing it.
 * The type of the first container may change. Returns the modified
 * (and possibly new) container
*/
static inline void *container_ixor(void *c1, uint8_t type1, const void *c2,
                                   uint8_t type2, uint8_t *result_type) {
    c1 = get_writable_copy_if_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = bitset_bitset_container_ixor(
                               (bitset_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = array_array_container_ixor(
                               (array_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            *result_type = run_run_container_ixor(
                (run_container_t *)c1, (const run_container_t *)c2, &result);
            return result;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = bitset_array_container_ixor(
                               (bitset_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = array_bitset_container_ixor(
                               (array_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;

            return result;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            *result_type =
                bitset_run_container_ixor((bitset_container_t *)c1,
                                          (const run_container_t *)c2, &result)
                    ? BITSET_CONTAINER_TYPE_CODE
                    : ARRAY_CONTAINER_TYPE_CODE;

            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = run_bitset_container_ixor(
                               (run_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;

            return result;

        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            *result_type = array_run_container_ixor(
                (array_container_t *)c1, (const run_container_t *)c2, &result);
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            *result_type = run_array_container_ixor(
                (run_container_t *)c1, (const array_container_t *)c2, &result);
            return result;
        default:
            assert(false);
            __builtin_unreachable();
            return NULL;
    }
}

/**
 * Compute the xor between two containers, with result in the first container.
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
static inline void *container_lazy_ixor(void *c1, uint8_t type1, const void *c2,
                                        uint8_t type2, uint8_t *result_type) {
    assert(type1 != SHARED_CONTAINER_TYPE_CODE);
    // c1 = get_writable_copy_if_shared(c1,&type1);
    c2 = container_unwrap_shared(c2, &type2);
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            bitset_container_xor_nocard((bitset_container_t *)c1,
                                        (const bitset_container_t *)c2,
                                        (bitset_container_t *)c1);  // is lazy
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            return c1;
        // TODO: other cases being lazy, esp. when we know inplace not likely
        // could see the corresponding code for union
        default:
            // we may have a dirty bitset (without a precomputed cardinality) and
            // calling container_ixor on it might be unsafe.
            if( (type1 == BITSET_CONTAINER_TYPE_CODE)
              && (((const bitset_container_t *)c1)->cardinality == BITSET_UNKNOWN_CARDINALITY)) {
                ((bitset_container_t *)c1)->cardinality = bitset_container_compute_cardinality((bitset_container_t *)c1);
            }
            return container_ixor(c1, type1, c2, type2, result_type);
    }
}

/**
 * Compute difference (andnot) between two containers, generate a new
 * container (having type result_type), requires a typecode. This allocates new
 * memory, caller is responsible for deallocation.
 */
static inline void *container_andnot(const void *c1, uint8_t type1,
                                     const void *c2, uint8_t type2,
                                     uint8_t *result_type) {
    c1 = container_unwrap_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = bitset_bitset_container_andnot(
                               (const bitset_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            result = array_container_create();
            array_array_container_andnot((const array_container_t *)c1,
                                         (const array_container_t *)c2,
                                         (array_container_t *)result);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c2)) {
                result = array_container_create();
                *result_type = ARRAY_CONTAINER_TYPE_CODE;
                return result;
            }
            *result_type =
                run_run_container_andnot((const run_container_t *)c1,
                                         (const run_container_t *)c2, &result);
            return result;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = bitset_array_container_andnot(
                               (const bitset_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            result = array_container_create();
            array_bitset_container_andnot((const array_container_t *)c1,
                                          (const bitset_container_t *)c2,
                                          (array_container_t *)result);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c2)) {
                result = array_container_create();
                *result_type = ARRAY_CONTAINER_TYPE_CODE;
                return result;
            }
            *result_type = bitset_run_container_andnot(
                               (const bitset_container_t *)c1,
                               (const run_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):

            *result_type = run_bitset_container_andnot(
                               (const run_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;

        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            if (run_container_is_full((const run_container_t *)c2)) {
                result = array_container_create();
                *result_type = ARRAY_CONTAINER_TYPE_CODE;
                return result;
            }
            result = array_container_create();
            array_run_container_andnot((const array_container_t *)c1,
                                       (const run_container_t *)c2,
                                       (array_container_t *)result);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;
            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            *result_type = run_array_container_andnot(
                (const run_container_t *)c1, (const array_container_t *)c2,
                &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
            return NULL;  // unreached
    }
}

/**
 * Compute the andnot between two containers, with result in the first
 * container.
 * If the returned pointer is identical to c1, then the container has been
 * modified.
 * If the returned pointer is different from c1, then a new container has been
 * created and the caller is responsible for freeing it.
 * The type of the first container may change. Returns the modified
 * (and possibly new) container
*/
static inline void *container_iandnot(void *c1, uint8_t type1, const void *c2,
                                      uint8_t type2, uint8_t *result_type) {
    c1 = get_writable_copy_if_shared(c1, &type1);
    c2 = container_unwrap_shared(c2, &type2);
    void *result = NULL;
    switch (CONTAINER_PAIR(type1, type2)) {
        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = bitset_bitset_container_iandnot(
                               (bitset_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            array_array_container_iandnot((array_container_t *)c1,
                                          (const array_container_t *)c2);
            *result_type = ARRAY_CONTAINER_TYPE_CODE;
            return c1;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            *result_type = run_run_container_iandnot(
                (run_container_t *)c1, (const run_container_t *)c2, &result);
            return result;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            ARRAY_CONTAINER_TYPE_CODE):
            *result_type = bitset_array_container_iandnot(
                               (bitset_container_t *)c1,
                               (const array_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = ARRAY_CONTAINER_TYPE_CODE;

            array_bitset_container_iandnot((array_container_t *)c1,
                                           (const bitset_container_t *)c2);
            return c1;

        case CONTAINER_PAIR(BITSET_CONTAINER_TYPE_CODE,
                            RUN_CONTAINER_TYPE_CODE):
            *result_type = bitset_run_container_iandnot(
                               (bitset_container_t *)c1,
                               (const run_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;

            return result;

        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE,
                            BITSET_CONTAINER_TYPE_CODE):
            *result_type = run_bitset_container_iandnot(
                               (run_container_t *)c1,
                               (const bitset_container_t *)c2, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;

            return result;

        case CONTAINER_PAIR(ARRAY_CONTAINER_TYPE_CODE, RUN_CONTAINER_TYPE_CODE):
            *result_type = ARRAY_CONTAINER_TYPE_CODE;
            array_run_container_iandnot((array_container_t *)c1,
                                        (const run_container_t *)c2);
            return c1;
        case CONTAINER_PAIR(RUN_CONTAINER_TYPE_CODE, ARRAY_CONTAINER_TYPE_CODE):
            *result_type = run_array_container_iandnot(
                (run_container_t *)c1, (const array_container_t *)c2, &result);
            return result;
        default:
            assert(false);
            __builtin_unreachable();
            return NULL;
    }
}

/**
 * Visit all values x of the container once, passing (base+x,ptr)
 * to iterator. You need to specify a container and its type.
 * Returns true if the iteration should continue.
 */
static inline bool container_iterate(const void *container, uint8_t typecode,
                                     uint32_t base, roaring_iterator iterator,
                                     void *ptr) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_iterate(
                (const bitset_container_t *)container, base, iterator, ptr);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_iterate((const array_container_t *)container,
                                           base, iterator, ptr);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_iterate((const run_container_t *)container,
                                         base, iterator, ptr);
        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return false;
}

static inline bool container_iterate64(const void *container, uint8_t typecode,
                                       uint32_t base,
                                       roaring_iterator64 iterator,
                                       uint64_t high_bits, void *ptr) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_iterate64(
                (const bitset_container_t *)container, base, iterator,
                high_bits, ptr);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_iterate64(
                (const array_container_t *)container, base, iterator, high_bits,
                ptr);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_iterate64((const run_container_t *)container,
                                           base, iterator, high_bits, ptr);
        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return false;
}

static inline void *container_not(const void *c, uint8_t typ,
                                  uint8_t *result_type) {
    c = container_unwrap_shared(c, &typ);
    void *result = NULL;
    switch (typ) {
        case BITSET_CONTAINER_TYPE_CODE:
            *result_type = bitset_container_negation(
                               (const bitset_container_t *)c, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case ARRAY_CONTAINER_TYPE_CODE:
            result = bitset_container_create();
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            array_container_negation((const array_container_t *)c,
                                     (bitset_container_t *)result);
            return result;
        case RUN_CONTAINER_TYPE_CODE:
            *result_type =
                run_container_negation((const run_container_t *)c, &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return NULL;
}

static inline void *container_not_range(const void *c, uint8_t typ,
                                        uint32_t range_start,
                                        uint32_t range_end,
                                        uint8_t *result_type) {
    c = container_unwrap_shared(c, &typ);
    void *result = NULL;
    switch (typ) {
        case BITSET_CONTAINER_TYPE_CODE:
            *result_type =
                bitset_container_negation_range((const bitset_container_t *)c,
                                                range_start, range_end, &result)
                    ? BITSET_CONTAINER_TYPE_CODE
                    : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case ARRAY_CONTAINER_TYPE_CODE:
            *result_type =
                array_container_negation_range((const array_container_t *)c,
                                               range_start, range_end, &result)
                    ? BITSET_CONTAINER_TYPE_CODE
                    : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case RUN_CONTAINER_TYPE_CODE:
            *result_type = run_container_negation_range(
                (const run_container_t *)c, range_start, range_end, &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return NULL;
}

static inline void *container_inot(void *c, uint8_t typ, uint8_t *result_type) {
    c = get_writable_copy_if_shared(c, &typ);
    void *result = NULL;
    switch (typ) {
        case BITSET_CONTAINER_TYPE_CODE:
            *result_type = bitset_container_negation_inplace(
                               (bitset_container_t *)c, &result)
                               ? BITSET_CONTAINER_TYPE_CODE
                               : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case ARRAY_CONTAINER_TYPE_CODE:
            // will never be inplace
            result = bitset_container_create();
            *result_type = BITSET_CONTAINER_TYPE_CODE;
            array_container_negation((array_container_t *)c,
                                     (bitset_container_t *)result);
            array_container_free((array_container_t *)c);
            return result;
        case RUN_CONTAINER_TYPE_CODE:
            *result_type =
                run_container_negation_inplace((run_container_t *)c, &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return NULL;
}

static inline void *container_inot_range(void *c, uint8_t typ,
                                         uint32_t range_start,
                                         uint32_t range_end,
                                         uint8_t *result_type) {
    c = get_writable_copy_if_shared(c, &typ);
    void *result = NULL;
    switch (typ) {
        case BITSET_CONTAINER_TYPE_CODE:
            *result_type =
                bitset_container_negation_range_inplace(
                    (bitset_container_t *)c, range_start, range_end, &result)
                    ? BITSET_CONTAINER_TYPE_CODE
                    : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case ARRAY_CONTAINER_TYPE_CODE:
            *result_type =
                array_container_negation_range_inplace(
                    (array_container_t *)c, range_start, range_end, &result)
                    ? BITSET_CONTAINER_TYPE_CODE
                    : ARRAY_CONTAINER_TYPE_CODE;
            return result;
        case RUN_CONTAINER_TYPE_CODE:
            *result_type = run_container_negation_range_inplace(
                (run_container_t *)c, range_start, range_end, &result);
            return result;

        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return NULL;
}

/**
 * If the element of given rank is in this container, supposing that
 * the first
 * element has rank start_rank, then the function returns true and
 * sets element
 * accordingly.
 * Otherwise, it returns false and update start_rank.
 */
static inline bool container_select(const void *container, uint8_t typecode,
                                    uint32_t *start_rank, uint32_t rank,
                                    uint32_t *element) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_select((const bitset_container_t *)container,
                                           start_rank, rank, element);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_select((const array_container_t *)container,
                                          start_rank, rank, element);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_select((const run_container_t *)container,
                                        start_rank, rank, element);
        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return false;
}

static inline uint16_t container_maximum(const void *container,
                                         uint8_t typecode) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_maximum((const bitset_container_t *)container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_maximum((const array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_maximum((const run_container_t *)container);
        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return false;
}

static inline uint16_t container_minimum(const void *container,
                                         uint8_t typecode) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_minimum((const bitset_container_t *)container);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_minimum((const array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_minimum((const run_container_t *)container);
        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return false;
}

// number of values smaller or equal to x
static inline int container_rank(const void *container, uint8_t typecode,
                                 uint16_t x) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_rank((const bitset_container_t *)container, x);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_rank((const array_container_t *)container, x);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_rank((const run_container_t *)container, x);
        default:
            assert(false);
            __builtin_unreachable();
    }
    assert(false);
    __builtin_unreachable();
    return false;
}

/**
 * Add all values in range [min, max] to a given container.
 *
 * If the returned pointer is different from $container, then a new container
 * has been created and the caller is responsible for freeing it.
 * The type of the first container may change. Returns the modified
 * (and possibly new) container.
 */
static inline void *container_add_range(void *container, uint8_t type,
                                        uint32_t min, uint32_t max,
                                        uint8_t *result_type) {
    // NB: when selecting new container type, we perform only inexpensive checks
    switch (type) {
        case BITSET_CONTAINER_TYPE_CODE: {
            bitset_container_t *bitset = (bitset_container_t *) container;

            int32_t union_cardinality = 0;
            union_cardinality += bitset->cardinality;
            union_cardinality += max - min + 1;
            union_cardinality -= bitset_lenrange_cardinality(bitset->array, min, max-min);

            if (union_cardinality == INT32_C(0x10000)) {
                *result_type = RUN_CONTAINER_TYPE_CODE;
                return run_container_create_range(0, INT32_C(0x10000));
            } else {
                *result_type = BITSET_CONTAINER_TYPE_CODE;
                bitset_set_lenrange(bitset->array, min, max - min);
                bitset->cardinality = union_cardinality;
                return bitset;
            }
        }
        case ARRAY_CONTAINER_TYPE_CODE: {
            array_container_t *array = (array_container_t *) container;

            int32_t nvals_greater = count_greater(array->array, array->cardinality, max);
            int32_t nvals_less = count_less(array->array, array->cardinality - nvals_greater, min);
            int32_t union_cardinality = nvals_less + (max - min + 1) + nvals_greater;

            if (union_cardinality == INT32_C(0x10000)) {
                *result_type = RUN_CONTAINER_TYPE_CODE;
                return run_container_create_range(0, INT32_C(0x10000));
            } else if (union_cardinality <= DEFAULT_MAX_SIZE) {
                *result_type = ARRAY_CONTAINER_TYPE_CODE;
                array_container_add_range_nvals(array, min, max, nvals_less, nvals_greater);
                return array;
            } else {
                *result_type = BITSET_CONTAINER_TYPE_CODE;
                bitset_container_t *bitset = bitset_container_from_array(array);
                bitset_set_lenrange(bitset->array, min, max - min);
                bitset->cardinality = union_cardinality;
                return bitset;
            }
        }
        case RUN_CONTAINER_TYPE_CODE: {
            run_container_t *run = (run_container_t *) container;

            int32_t nruns_greater = rle16_count_greater(run->runs, run->n_runs, max);
            int32_t nruns_less = rle16_count_less(run->runs, run->n_runs - nruns_greater, min);

            int32_t run_size_bytes = (nruns_less + 1 + nruns_greater) * sizeof(rle16_t);
            int32_t bitset_size_bytes = BITSET_CONTAINER_SIZE_IN_WORDS * sizeof(uint64_t);

            if (run_size_bytes <= bitset_size_bytes) {
                run_container_add_range_nruns(run, min, max, nruns_less, nruns_greater);
                *result_type = RUN_CONTAINER_TYPE_CODE;
                return run;
            } else {
                *result_type = BITSET_CONTAINER_TYPE_CODE;
                return bitset_container_from_run_range(run, min, max);
            }
        }
        default:
            __builtin_unreachable();
    }
}

/*
 * Removes all elements in range [min, max].
 * Returns one of:
 *   - NULL if no elements left
 *   - pointer to the original container
 *   - pointer to a newly-allocated container (if it is more efficient)
 *
 * If the returned pointer is different from $container, then a new container
 * has been created and the caller is responsible for freeing the original container.
 */
static inline void *container_remove_range(void *container, uint8_t type,
                                           uint32_t min, uint32_t max,
                                           uint8_t *result_type) {
     switch (type) {
        case BITSET_CONTAINER_TYPE_CODE: {
            bitset_container_t *bitset = (bitset_container_t *) container;

            int32_t result_cardinality = bitset->cardinality -
                bitset_lenrange_cardinality(bitset->array, min, max-min);

            if (result_cardinality == 0) {
                return NULL;
            } else if (result_cardinality < DEFAULT_MAX_SIZE) {
                *result_type = ARRAY_CONTAINER_TYPE_CODE;
                bitset_reset_range(bitset->array, min, max+1);
                bitset->cardinality = result_cardinality;
                return array_container_from_bitset(bitset);
            } else {
                *result_type = BITSET_CONTAINER_TYPE_CODE;
                bitset_reset_range(bitset->array, min, max+1);
                bitset->cardinality = result_cardinality;
                return bitset;
            }
        }
        case ARRAY_CONTAINER_TYPE_CODE: {
            array_container_t *array = (array_container_t *) container;

            int32_t nvals_greater = count_greater(array->array, array->cardinality, max);
            int32_t nvals_less = count_less(array->array, array->cardinality - nvals_greater, min);
            int32_t result_cardinality = nvals_less + nvals_greater;

            if (result_cardinality == 0) {
                return NULL;
            } else {
                *result_type = ARRAY_CONTAINER_TYPE_CODE;
                array_container_remove_range(array, nvals_less,
                    array->cardinality - result_cardinality);
                return array;
            }
        }
        case RUN_CONTAINER_TYPE_CODE: {
            run_container_t *run = (run_container_t *) container;

            if (run->n_runs == 0) {
                return NULL;
            }
            if (min <= run_container_minimum(run) && max >= run_container_maximum(run)) {
                return NULL;
            }

            run_container_remove_range(run, min, max);

            if (run_container_serialized_size_in_bytes(run->n_runs) <=
                    bitset_container_serialized_size_in_bytes()) {
                *result_type = RUN_CONTAINER_TYPE_CODE;
                return run;
            } else {
                *result_type = BITSET_CONTAINER_TYPE_CODE;
                return bitset_container_from_run(run);
            }
        }
        default:
            __builtin_unreachable();
     }
}

#endif
