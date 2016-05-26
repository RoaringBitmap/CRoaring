#ifndef INCLUDE_ROARING_ARRAY_H
#define INCLUDE_ROARING_ARRAY_H

#include <stdbool.h>
#include <stdint.h>
#include "array_util.h"
#include "containers/containers.h"

#define MAX_CONTAINERS 65536

#define SERIALIZATION_ARRAY_UINT32  1
#define SERIALIZATION_CONTAINER     2


enum {
    SERIAL_COOKIE_NO_RUNCONTAINER = 12346,
    SERIAL_COOKIE = 12347,
    NO_OFFSET_THRESHOLD = 4
};

/**
 * Roaring arrays are array-based key-value pairs having containers as values
 * and 16-bit integer keys. A roaring bitmap  might be implemented as such.
 */

// parallel arrays.  Element sizes quite different.
// Alternative is array
// of structs.  Which would have better
// cache performance through binary searches?

typedef struct roaring_array_s {
    int32_t size;
    int32_t allocation_size;
    uint16_t *keys;
    void **containers;
    uint8_t *typecodes;
    uint8_t *shared; /* for COW, used as a bitset*/
} roaring_array_t;

/**
 * Create a new roaring array
 */
roaring_array_t *ra_create(void);

/**
 * Create a new roaring array with the specified capacity (in number
 * of containers)
 */
roaring_array_t *ra_create_with_capacity(uint32_t cap);

/**
 * Copies this roaring array (caller is responsible for memory management)
 */
roaring_array_t *ra_copy(roaring_array_t *r, bool copy_on_write);

/**
 * Frees the memory used by a roaring array
 */
void ra_free(roaring_array_t *r);

/**
 * Frees the memory used by a roaring array, but does not free the containers
 */
void ra_free_without_containers(roaring_array_t *r);

/**
 * Get the index corresponding to a 16-bit key
 */
int32_t ra_get_index(roaring_array_t *ra, uint16_t x);

/**
 * Retrieves the container at index i, filling in the typecode
 */
void *ra_get_container_at_index(roaring_array_t *ra, uint16_t i,
                                uint8_t *typecode);


/**
 * Retrieves the key at index i
 */
uint16_t ra_get_key_at_index(roaring_array_t *ra, uint16_t i);

/**
 * Add a new key-value pair at index i
 */
void ra_insert_new_key_value_at(roaring_array_t *ra, int32_t i, uint16_t key,
                                void *container, uint8_t typecode);

/**
 * Append a new key-value pair
 */
void ra_append(roaring_array_t *ra, uint16_t s, void *c, uint8_t typecode);

/**
 * Append a new key-value pair to ra, cloning (in COW sense) a value from sa at index index
 */
void ra_append_copy(roaring_array_t *ra, roaring_array_t *sa, uint16_t index, bool copy_on_write);

/**
 * Append new key-value pairs to ra, cloning (in COW sense)  values from sa at indexes
 * [start_index, uint16_t end_index)
 */
void ra_append_copy_range(roaring_array_t *ra, roaring_array_t *sa,
                          uint16_t start_index, uint16_t end_index, bool copy_on_write);

/** appends from sa to ra, ending with the greatest key that is
 * is less or equal stopping_key
 */
void ra_append_copies_until(roaring_array_t *ra, roaring_array_t *sa,
                            uint16_t stopping_key, bool copy_on_write);

/** appends from sa to ra, starting with the smallest key that is
 * is strictly greater than before_start
 */

void ra_append_copies_after(roaring_array_t *ra, roaring_array_t *sa,
                            uint16_t before_start, bool copy_on_write);

/**
 * Move the key-value pairs to ra from sa at indexes
 * [start_index, uint16_t end_index), old array should not be freed
 * (use ra_free_without_containers)
 **/
void ra_append_move_range(roaring_array_t *ra, roaring_array_t *sa,
                          uint16_t start_index, uint16_t end_index);
/**
 * Append new key-value pairs to ra,  from sa at indexes
 * [start_index, uint16_t end_index)
 */
void ra_append_range(roaring_array_t *ra, roaring_array_t *sa,
                          uint16_t start_index, uint16_t end_index, bool copy_on_write);

/**
 * Set the container at the corresponding index using the specified typecode.
 */
void ra_set_container_at_index(roaring_array_t *ra, int32_t i, void *c,
                               uint8_t typecode);

/**
 * If needed, increase the capacity of the array so that it can fit k values (at
 * least);
 */
void extend_array(roaring_array_t *ra, uint32_t k);

static inline int32_t ra_get_size(roaring_array_t *ra) { return ra->size; }

static inline int32_t ra_advance_until(roaring_array_t *ra, uint16_t x,
                                       int32_t pos) {
    return advanceUntil(ra->keys, pos, ra->size, x);
}

int32_t ra_advance_until_freeing(roaring_array_t *ra, uint16_t x, int32_t pos);

void ra_downsize(roaring_array_t *ra, int32_t new_length);

void ra_replace_key_and_container_at_index(roaring_array_t *ra, int32_t i,
                                           uint16_t key, void *c,
                                           uint8_t typecode);

// see ra_portable_serialize if you want a format that's compatible with Java
// and Go implementations
char *ra_serialize(roaring_array_t *ra, uint32_t *serialize_len,
                   uint8_t *retry_with_array);

// see ra_portable_serialize if you want a format that's compatible with Java
// and Go implementations
roaring_array_t *ra_deserialize(const void *buf, uint32_t buf_len);

/**
 * write a bitmap to a buffer. This is meant to be compatible with
 * the
 * Java and Go versions. Return the size in bytes of the serialized
 * output (which should be ra_portable_size_in_bytes(ra)).
 */
size_t ra_portable_serialize(roaring_array_t *ra, char *buf);

/**
 * read a bitmap from a serialized version. This is meant to be compatible with
 * the
 * Java and Go versions.
 */
roaring_array_t *ra_portable_deserialize(const char *buf);

/**
 * How many bytes are required to serialize this bitmap (meant to be compatible
 * with Java and Go versions)
 */
size_t ra_portable_size_in_bytes(roaring_array_t *ra);

/**
 * return true if it contains at least one run container.
 */
bool ra_has_run_container(roaring_array_t *ra);

/**
 * Size of the header when serializing (meant to be compatible
 * with Java and Go versions)
 */
uint32_t ra_portable_header_size(roaring_array_t *ra);

/**
 * If the container at the index i is share, unshare it (creating a local copy if needed).
 */
void ra_unshare_container_at_index(roaring_array_t *ra, uint16_t i) ;

/**
 * remove at index i, sliding over all entries after i
 */
void ra_remove_at_index(roaring_array_t *ra, int32_t i);

/**
 * remove a chunk of indices, sliding over entries after it
 */
// void ra_remove_index_range(roaring_array_t *ra, int32_t begin, int32_t end);

#endif
