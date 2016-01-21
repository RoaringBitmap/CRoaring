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
 * Computes the intersection between two bitmaps and return new bitmap
 */
roaring_bitmap_t *roaring_bitmap_and( roaring_bitmap_t *x1, roaring_bitmap_t *x2);

/**
 * Computes the union between two bitmaps and return new bitmap
 */
roaring_bitmap_t *roaring_bitmap_and( roaring_bitmap_t *x1, roaring_bitmap_t *x2);



/**
 * Frees the memory
 */
void roaring_bitmap_free(roaring_bitmap_t *r);


/**
 * Add value x
 */
void roaring_bitmap_add( roaring_bitmap_t *r, uint32_t x);



/**
 * Get the cardinality of the bitmap (number elements)
 */
uint32_t roaring_bitmap_get_cardinality( roaring_bitmap_t *ra);



/**
 * Convert the bitmap to an array
 */
uint32_t *roaring_bitmap_to_uint32_array( roaring_bitmap_t *ra);
#endif
