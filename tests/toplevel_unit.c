#include <stdio.h>
#include <assert.h>

#include "roaring.h"

int test_add(){
  printf("test add\n");
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  assert(r1);

  for (uint32_t i=0; i < 10000; ++i) {
	  assert(roaring_bitmap_get_cardinality(r1) == i);
	  roaring_bitmap_add(r1, 200*i);
      assert(roaring_bitmap_get_cardinality(r1) == i + 1);
  }
  return 1;
}


int test_intersection(){
  printf("test intersection\n");
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  roaring_bitmap_t *r2 = roaring_bitmap_create();
  assert(r1); assert(r2);

  for (uint32_t i=0; i < 100; ++i) {
    roaring_bitmap_add(r1, 2*i);                      
    roaring_bitmap_add(r2, 3*i);
    assert(roaring_bitmap_get_cardinality(r2) == i + 1);
    assert(roaring_bitmap_get_cardinality(r1) == i + 1);

  }
   
  roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
  assert(roaring_bitmap_get_cardinality(r1_and_r2) == 34);
  return 1;
}

int test_union(){
  printf("test union TODO \n");
/*  roaring_bitmap_t *r1 = roaring_bitmap_create();
  roaring_bitmap_t *r2 = roaring_bitmap_create();
  assert(r1); assert(r2);

  for (uint32_t i=0; i < 100; ++i) {
    roaring_bitmap_add(r1, 2*i);
    roaring_bitmap_add(r2, 3*i);
    assert(roaring_bitmap_get_cardinality(r2) == i + 1);
    assert(roaring_bitmap_get_cardinality(r1) == i + 1);

  }

  roaring_bitmap_t *r1_or_r2 = roaring_bitmap_or(r1, r2);
  assert(roaring_bitmap_get_cardinality(r1_or_r2) == 166);
  */
  return 1;
}

int main(){
  test_add();
  test_intersection();
  test_union();
  printf("done toplevel tests\n");

}
