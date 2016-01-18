
#include "roaring_array.h"
#include "roaring.h"

// there should be some SIMD optimizations possible here
roaring_bitmap_t *roaring_bitmap_and( roaring_bitmap_t *x1, roaring_bitmap_t *x2) {
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
      void *c = container_and(c1, container_type_1, c2, container_type_2);
      if (container_get_cardinality() >  0) {
        ra_append(answer->high_low_container, s1, c);
      }
      ++pos1;
      ++pos2;
    } else if (Util.compareUnsigned(s1, s2) < 0) { // s1 < s2
      pos1 = x1->high_low_container->advanceUntil(s2,pos1);
    } else { // s1 > s2
      pos2 = x2->high_low_container->advanceUntil(s1,pos2);
    }
  }
  return answer;
}



}
