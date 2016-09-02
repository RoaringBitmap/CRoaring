#ifndef INCLUDE_ROARING_ARRAY_H
#define INCLUDE_ROARING_ARRAY_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <roaring/array_util.h>
#include <roaring/containers/containers.h>

#define MAX_CONTAINERS 65536

#define SERIALIZATION_ARRAY_UINT32 1
#define SERIALIZATION_CONTAINER 2

enum {
    SERIAL_COOKIE_NO_RUNCONTAINER = 12346,
    SERIAL_COOKIE = 12347,
    NO_OFFSET_THRESHOLD = 4
};

/**
 * Roaring arrays are array-based key-value pairs having containers as values
 * and 16-bit integer keys. A roaring bitmap  might be implemented as such.
 */


/**
* An array of struct is probably more efficient
* than a struct of array. We often access key, typecode,
* and container in one go. And we rarely use SIMD instructions
* on keys (for example).
*/
typedef struct key_container_s {
    void *container;
    uint16_t key;
    uint8_t typecode;
} key_container_t;
/* sizeof(key_container_t) should be 16 bytes without packing,
 down to 11 bytes with packing... except that accessing 11-byte elements is
 likely to hurt performance.
 */


typedef struct roaring_array_s {
    key_container_t *keys_containers;
    int32_t size; // actual
    int32_t allocation_size;// capacity
} roaring_array_t;



/*
 *  good old binary search over key_container_t arrays, assume sorted by key
 *  and lenarray > 0
 */
inline int32_t key_container_binary_search(const key_container_t *array, int32_t lenarray,
                                   uint16_t ikey) {
    int32_t low = 0;
    int32_t high = lenarray - 1;
    while (low <= high) {
        int32_t middleIndex = (low + high) >> 1;
        uint16_t middleValue = array[middleIndex].key;
        if (middleValue < ikey) {
            low = middleIndex + 1;
        } else if (middleValue > ikey) {
            high = middleIndex - 1;
        } else {
            return middleIndex;
        }
    }
    return -(low + 1);
}

/**
 * Galloping search over key_container_t arrays, search for key value, assume sorted
 */
static inline int32_t key_container_advance_until(const key_container_t *array, int32_t pos,
                                   int32_t length, uint16_t min) {
    int32_t lower = pos + 1;

    if ((lower >= length) || (array[lower].key >= min)) {
        return lower;
    }

    int32_t spansize = 1;

    while ((lower + spansize < length) && (array[lower + spansize].key < min)) {
        spansize <<= 1;
    }
    int32_t upper = (lower + spansize < length) ? lower + spansize : length - 1;

    if (array[upper].key == min) {
        return upper;
    }
    if (array[upper].key < min) {
        // means
        // array
        // has no
        // item
        // >= min
        // pos = array.length;
        return length;
    }

    // we know that the next-smallest span was too small
    lower += (spansize >> 1);

    int32_t mid = 0;
    while (lower + 1 != upper) {
        mid = (lower + upper) >> 1;
        if (array[mid].key == min) {
            return mid;
        } else if (array[mid].key < min) {
            lower = mid;
        } else {
            upper = mid;
        }
    }
    return upper;
}

/**
 * Create a new roaring array
 */
bool ra_init(roaring_array_t *) __attribute__((warn_unused_result));

/**
 * Create a new roaring array with the specified capacity (in number
 * of containers)
 */
bool ra_init_with_capacity(roaring_array_t *, uint32_t cap) __attribute__((warn_unused_result));

/**
 * Copies this roaring array
 */
bool *ra_copy(const roaring_array_t *source, roaring_array_t *dest, bool copy_on_write) __attribute__((warn_unused_result));

/**
 * Frees the memory used by a roaring array
 */
void ra_clear(roaring_array_t *r);


/**
 * Frees the memory used by a roaring array, but does not free the containers
 */
void ra_clear_without_containers(roaring_array_t *r);

/**
 * Get the index corresponding to a 16-bit key
 */
inline int32_t ra_get_index(const roaring_array_t *ra, uint16_t x) {
    if (ra->size == 0) return ra->size - 1;
    return key_container_binary_search(ra->keys_containers, (int32_t)ra->size, x);
}

/**
 * Retrieves the container at index i, filling in the typecode
 */
inline void *ra_get_container_at_index(const roaring_array_t *ra, uint16_t i,
                                              uint8_t *typecode) {
	key_container_t * kt = ra->keys_containers + i;
    *typecode = kt->typecode;
    return kt->container;
}

/**
 * Retrieves the key-container at index i
 */
inline key_container_t * ra_key_container_at(const roaring_array_t *ra, uint16_t i) {
	return ra->keys_containers + i;
}

/**
 * Retrieves the key at index i
 */
uint16_t ra_get_key_at_index(const roaring_array_t *ra, uint16_t i);

/**
 * Add a new key-value pair at index i
 */
bool ra_insert_new_key_value_at(roaring_array_t *ra, int32_t i, uint16_t key,
                                void *container, uint8_t typecode);

/**
 * Append a new key-value pair
 */
bool ra_append(roaring_array_t *ra, uint16_t s, void *c, uint8_t typecode);

/**
 * Append a new key-value pair to ra, cloning (in COW sense) a value from sa
 * at index index
 */
bool ra_append_copy(roaring_array_t *ra, roaring_array_t *sa, uint16_t index,
                    bool copy_on_write);

/**
 * Append new key-value pairs to ra, cloning (in COW sense)  values from sa
 * at indexes
 * [start_index, uint16_t end_index)
 */
bool ra_append_copy_range(roaring_array_t *ra, roaring_array_t *sa,
                          uint16_t start_index, uint16_t end_index,
                          bool copy_on_write);

/** appends from sa to ra, ending with the greatest key that is
 * is less or equal stopping_key
 */
bool ra_append_copies_until(roaring_array_t *ra, roaring_array_t *sa,
                            uint16_t stopping_key, bool copy_on_write);


/** appends from sa to ra, starting with the smallest key that is
 * is strictly greater than before_start
 */

bool ra_append_copies_after(roaring_array_t *ra, roaring_array_t *sa,
                            uint16_t before_start, bool copy_on_write);

/**
 * Move the key-value pairs to ra from sa at indexes
 * [start_index, uint16_t end_index), old array should not be freed
 * (use ra_free_without_containers)
 **/
bool ra_append_move_range(roaring_array_t *ra, roaring_array_t *sa,
                          uint16_t start_index, uint16_t end_index);
/**
 * Append new key-value pairs to ra,  from sa at indexes
 * [start_index, uint16_t end_index)
 */
bool ra_append_range(roaring_array_t *ra, roaring_array_t *sa,
                     uint16_t start_index, uint16_t end_index,
                     bool copy_on_write);

/**
 * Set the container at the corresponding index using the specified
 * typecode.
 */
void ra_set_container_at_index(roaring_array_t *ra, int32_t i, void *c,
                               uint8_t typecode);

/**
 * If needed, increase the capacity of the array so that it can fit k values
 * (at
 * least);
 */
bool extend_array(roaring_array_t *ra, uint32_t k) __attribute__((warn_unused_result));

inline int32_t ra_get_size(const roaring_array_t *ra) { return ra->size; }

static inline int32_t ra_advance_until(const roaring_array_t *ra, uint16_t x,
                                       int32_t pos) {
    return key_container_advance_until(ra->keys_containers, pos, ra->size, x);
}

int32_t ra_advance_until_freeing(roaring_array_t *ra, uint16_t x, int32_t pos);

void ra_downsize(roaring_array_t *ra, int32_t new_length);

void ra_replace_key_and_container_at_index(roaring_array_t *ra, int32_t i,
                                           uint16_t key, void *c,
                                           uint8_t typecode);

// write set bits to an array
void ra_to_uint32_array(roaring_array_t *ra, uint32_t *ans);

// see ra_portable_serialize if you want a format that's compatible with
// Java
// and Go implementations
size_t ra_serialize(const roaring_array_t *ra, char *buf);

// see ra_portable_serialize if you want a format that's compatible with
// Java
// and Go implementations
bool ra_deserialize(roaring_array_t *, const void *buf);

/**
 * How many bytes are required to serialize this bitmap (NOT
 * compatible
 * with Java and Go versions)
 */
size_t ra_size_in_bytes(roaring_array_t *ra) ;

/**
 * write a bitmap to a buffer. This is meant to be compatible with
 * the
 * Java and Go versions. Return the size in bytes of the serialized
 * output (which should be ra_portable_size_in_bytes(ra)).
 */
size_t ra_portable_serialize(const roaring_array_t *ra, char *buf);

/**
 * read a bitmap from a serialized version. This is meant to be compatible
 * with
 * the
 * Java and Go versions.
 */
bool ra_portable_deserialize(roaring_array_t * dest, const char *buf) __attribute__((warn_unused_result));

/**
 * How many bytes are required to serialize this bitmap (meant to be
 * compatible
 * with Java and Go versions)
 */
size_t ra_portable_size_in_bytes(roaring_array_t *ra);

/**
 * return true if it contains at least one run container.
 */
bool ra_has_run_container(const roaring_array_t *ra) __attribute__((warn_unused_result));

/**
 * Size of the header when serializing (meant to be compatible
 * with Java and Go versions)
 */
uint32_t ra_portable_header_size(roaring_array_t *ra);

/**
 * If the container at the index i is share, unshare it (creating a local
 * copy if needed).
 */
static inline void ra_unshare_container_at_index(roaring_array_t *ra, uint16_t i) {
    assert(i < ra->size);
    ra->keys_containers[i].container =
        get_writable_copy_if_shared(ra->keys_containers[i].container, &ra->keys_containers[i].typecode);
}


/**
 * remove at index i, sliding over all entries after i
 */
void ra_remove_at_index(roaring_array_t *ra, int32_t i);

/**
 * remove at index i, sliding over all entries after i. Free removed container.
 */
void ra_remove_at_index_and_free(roaring_array_t *ra, int32_t i);

/**
 * remove a chunk of indices, sliding over entries after it
 */
// void ra_remove_index_range(roaring_array_t *ra, int32_t begin, int32_t end);

// used in inplace andNot only, to slide left the containers from
// the mutated RoaringBitmap that are after the largest container of
// the argument RoaringBitmap.  It is followed by a call to resize.
//
void ra_copy_range(roaring_array_t *ra, uint32_t begin, uint32_t end,
                   uint32_t new_begin);

#endif
