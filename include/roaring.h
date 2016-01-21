/*
An implementation of Roaring Bitmaps in C.
*/

#ifndef ROARING_H
#define ROARING_H

#include "roaring_array.h"

typedef struct roaring_bitmap_s {
  roaring_array_t *high_low_container;
} roaring_bitmap_t;

roaring_bitmap_t *roaring_bitmap_create();
roaring_bitmap_t *roaring_bitmap_and();
void roaring_bitmap_free(roaring_bitmap_t *r);
void roaring_bitmap_add( roaring_bitmap_t *r, uint32_t x);
int32_t roaring_bitmap_get_cardinality( roaring_bitmap_t *ra);
  uint32_t *roaring_bitmap_to_uint32_array( roaring_bitmap_t *ra);
#endif
