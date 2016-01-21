#include <assert.h>
#include "roaring_array.h"
#include "roaring.h"
#include "util.h"

roaring_bitmap_t *roaring_bitmap_create() {
  roaring_bitmap_t *ans = (roaring_bitmap_t *) malloc( sizeof(*ans));
  if (!ans) {
    return NULL;
  }
  ans->high_low_container = ra_create();
  if (!ans->high_low_container) {
    free(ans);
    return NULL;
  }
  return ans;
}

void roaring_bitmap_free(roaring_bitmap_t *r) {
  ra_free(r->high_low_container);
  free(r);
}

void roaring_bitmap_add(roaring_bitmap_t *r, uint32_t val) {
  const uint16_t hb = val >> 16;
  const int i = ra_get_index(r->high_low_container, hb);
    uint8_t typecode;
    if (i>=0) {
      void *container = ra_get_container_at_index(r->high_low_container, i, &typecode);
      void *container2 = container_add(container, val & 0xFFFF, typecode, &typecode);
      ra_set_container_at_index(r->high_low_container, i, typecode);
    }
    else {
      array_container_t *newac = array_container_create();
      void *container =  container_add(newac, val & 0xFFFF, ARRAY_CONTAINER_TYPE_CODE, &typecode);
      // we could just assume that it stays an array container
      ra_insert_new_key_value_at(r->high_low_container, -i-1, hb, container, typecode); 
    }
}


// there should be some SIMD optimizations possible here
roaring_bitmap_t *roaring_bitmap_and( roaring_bitmap_t *x1, roaring_bitmap_t *x2) {
  uint8_t container_result_type;
  roaring_bitmap_t *answer = roaring_bitmap_create();
  const int length1 = x1->high_low_container->size, length2 = x2->high_low_container->size;

  int pos1 = 0, pos2 = 0;

  while (pos1 < length1 && pos2 < length2) {
    const uint16_t s1 = ra_get_key_at_index(x1->high_low_container, pos1);
    const uint16_t s2 = ra_get_key_at_index(x2->high_low_container, pos2);

    if (s1 == s2) {
      uint8_t container_type_1, container_type_2;
      void *c1 = ra_get_container_at_index( x1->high_low_container, pos1, &container_type_1);
      void *c2 = ra_get_container_at_index( x2->high_low_container, pos2, &container_type_2);
      void *c = container_and(c1, container_type_1, c2, container_type_2, &container_result_type);
      if (container_nonzero_cardinality(c, container_result_type)) {
        ra_append(answer->high_low_container, s1, c, container_result_type);
      }
      ++pos1;
      ++pos2;
    } else if (s1 < s2) { // s1 < s2
          pos1 = advanceUntil(x1->high_low_container->keys, s2, 0xffff, pos1);
    } else { // s1 > s2
          pos2 = advanceUntil(x2->high_low_container->keys, s1, 0xffff, pos2);
    }
  }
  return answer;
}

int32_t roaring_get_cardinality( roaring_bitmap_t *ra) {
  int32_t ans = 0;
  for( int i = 0; i < ra->high_low_container->size; ++i)
    ans += container_get_cardinality( ra->high_low_container->containers[i], 
                                      ra->high_low_container->typecodes[i]);
  return ans;
}

uint32_t *roaring_to_uint32_array( roaring_bitmap_t *ra) {
  uint32_t *ans = malloc( roaring_get_cardinality(ra) * sizeof( uint32_t));
  int ctr=0;

  for( int i = 0; i < ra->high_low_container->size; ++i) {
    container_to_uint32_array( ans+ctr, 
                               ra->high_low_container->containers[i], 
                               ra->high_low_container->typecodes[i],
                               ((uint32_t) i) << 16);
  }
  return ans;
}







