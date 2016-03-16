/*
An implementation of Roaring Bitmaps in C.
*/

#ifndef ROARING_H
#define ROARING_H

#include <stdbool.h>
#include "roaring_array.h"
#include "roaring_types.h"

typedef struct roaring_bitmap_s {
    roaring_array_t *high_low_container;
} roaring_bitmap_t;

// TODO sprinkle in consts

/**
 * Creates a new bitmap (initially empty)
 */
roaring_bitmap_t *roaring_bitmap_create();

/**
 * Creates a new bitmap from a pointer of uint32_t integers
 */
roaring_bitmap_t *roaring_bitmap_of_ptr(size_t n_args, uint32_t *vals);

/**
 * Creates a new bitmap from a list of uint32_t integers
 */
roaring_bitmap_t *roaring_bitmap_of(size_t n, ...);

/**
 * Copies a  bitmap. This does memory allocation. The caller is responsible for
 * memory management.
 */
roaring_bitmap_t *roaring_bitmap_copy(roaring_bitmap_t *r);

/**
 * Print the content of the bitmap.
 */
void roaring_bitmap_printf(roaring_bitmap_t *ra);

/**
 * Computes the intersection between two bitmaps and returns new bitmap. The
 * caller is
 * responsible for memory management.
 *
 */
roaring_bitmap_t *roaring_bitmap_and(roaring_bitmap_t *x1,
                                     roaring_bitmap_t *x2);

/**
 * Inplace version modifies x1
 */
void roaring_bitmap_and_inplace(roaring_bitmap_t *x1,
                                const roaring_bitmap_t *x2);

/**
 * Computes the union between two bitmaps and returns new bitmap. The caller is
 * responsible for memory management.
 *
 */
roaring_bitmap_t *roaring_bitmap_or(roaring_bitmap_t *x1, roaring_bitmap_t *x2);

/**
 * Inplace version modifies x1
 */
void roaring_bitmap_or_inplace(roaring_bitmap_t *x1,
                               const roaring_bitmap_t *x2);

/**
 * Frees the memory.
 */
void roaring_bitmap_free(roaring_bitmap_t *r);

/**
 * Add value x
 *
 * TODO: add a way to remove values
 */
void roaring_bitmap_add(roaring_bitmap_t *r, uint32_t x);

/**
 * Check if value x is present
 */
bool roaring_bitmap_contains(roaring_bitmap_t *r, uint32_t x);

/**
 * Get the cardinality of the bitmap (number elements).
 */
uint32_t roaring_bitmap_get_cardinality(roaring_bitmap_t *ra);

/**
 * Convert the bitmap to an array. Array is allocated and caller is responsible
 * for eventually freeing it.
 */
uint32_t *roaring_bitmap_to_uint32_array(roaring_bitmap_t *ra,
                                         uint32_t *cardinality);
#endif

/**
 *  Remove run-length encoding even when it is more space efficient
 *  return whether a change was applied
 */
bool roaring_bitmap_remove_run_compression(roaring_bitmap_t *r);

/** convert array and bitmap containers to run containers when it is more
 * efficient;
 * also convert from run containers when more space efficient.  Returns
 * true if the result has at least one run container.
*/
bool roaring_bitmap_run_optimize(roaring_bitmap_t *r);

char *roaring_bitmap_serialize(roaring_bitmap_t *ra, uint32_t *serialize_len);

roaring_bitmap_t *roaring_bitmap_deserialize(char *buf, uint32_t buf_len);

/** * Iterate the bitmap elements
 */
void roaring_iterate(roaring_bitmap_t *ra, roaring_iterator iterator,
                     void *ptr);

/*
 * TODO: implement "equals", "string", serialization, contains
 */
