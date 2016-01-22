/*
An implementation of Roaring Bitmaps in C.
*/

#ifndef ROARING_H
#define ROARING_H

#include "roaring_array.h"

typedef struct roaring_bitmap_s {
  roaring_array_t *high_low_container;
} roaring_bitmap_t;

/**
 * Creates a new bitmap (initially empty)
 */
roaring_bitmap_t *roaring_bitmap_create();

/**
 * Copies a  bitmap. This does memory allocation. The caller is responsible for memory management.
 */
roaring_bitmap_t *roaring_bitmap_copy(roaring_bitmap_t *r) ;


/**
 * Computes the intersection between two bitmaps and returns new bitmap. The caller is
 * responsible for memory management.
 *
 * TODO: create an in-place version
 */
roaring_bitmap_t *roaring_bitmap_and( roaring_bitmap_t *x1, roaring_bitmap_t *x2);

/**
 * Computes the union between two bitmaps and returns new bitmap. The caller is
 * responsible for memory management.
 *
 * TODO: create an in-place version
 */
roaring_bitmap_t *roaring_bitmap_or( roaring_bitmap_t *x1, roaring_bitmap_t *x2);


/**
 * Frees the memory.
 */
void roaring_bitmap_free(roaring_bitmap_t *r);


/**
 * Add value x
 *
 * TODO: add a way to remove values
 */
void roaring_bitmap_add( roaring_bitmap_t *r, uint32_t x);



/**
 * Get the cardinality of the bitmap (number elements).
 */
uint32_t roaring_bitmap_get_cardinality( roaring_bitmap_t *ra);



/**
 * Convert the bitmap to an array. Caller is responsible for memory management.
 */
uint32_t *roaring_bitmap_to_uint32_array( roaring_bitmap_t *ra);
#endif


/*
 * TODO: implement "equals", "string", serialization, contains
 */
