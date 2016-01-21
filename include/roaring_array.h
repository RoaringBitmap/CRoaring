#ifndef INCLUDE_ROARING_ARRAY_H
#define INCLUDE_ROARING_ARRAY_H

#include <stdint.h>
#include "containers.h"

#define MAX_CONTAINERS 65536

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
} roaring_array_t;

roaring_array_t *ra_create();
void ra_free(roaring_array_t *r);
int32_t ra_get_index( roaring_array_t *ra, uint16_t x);
void *ra_get_container_at_index(roaring_array_t *ra, uint16_t i, uint8_t *typecode);
uint16_t ra_get_key_at_index(roaring_array_t *ra, uint16_t i);
void ra_insert_new_key_value_at( roaring_array_t *ra, int32_t i, uint16_t key, void *container, uint8_t typecode);
void ra_append( roaring_array_t *ra, uint16_t s, void *c, uint8_t typecode);
void ra_set_container_at_index(roaring_array_t *ra, int32_t i, void *c, uint8_t typecode);
#endif
