#include <stdio.h>
#include <assert.h>

#include "roaring.h"

int test_printf() {
	printf("[%s] %s\n", __FILE__, __func__);
	roaring_bitmap_t *r1 = roaring_bitmap_of(8,   1,2,3,100,1000,10000,1000000,20000000);
	roaring_bitmap_printf(r1); // does it crash?
	roaring_bitmap_free(r1);
	printf("\n");
	return 1;
}

int test_add(){
  printf("[%s] %s\n", __FILE__, __func__);
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  assert(r1);

  for (uint32_t i=0; i < 10000; ++i) {
	  assert(roaring_bitmap_get_cardinality(r1) == i);
	  roaring_bitmap_add(r1, 200*i);
      assert(roaring_bitmap_get_cardinality(r1) == i + 1);
  }
  roaring_bitmap_free(r1);
  return 1;
}

int test_contains(){
  printf("[%s] %s\n", __FILE__, __func__);
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  assert(r1);

  for (uint32_t i=0; i < 10000; ++i) {
	  assert(roaring_bitmap_get_cardinality(r1) == i);
	  roaring_bitmap_add(r1, 200*i);
      assert(roaring_bitmap_get_cardinality(r1) == i + 1);
  }
  for (uint32_t i=0; i < 200*10000; ++i) {
	  bool isset = (i % 200 == 0);
	  bool rset = roaring_bitmap_contains(r1, i);
	  assert(isset == rset);
  }
  roaring_bitmap_free(r1);
  return 1;
}

int test_intersection(){
  printf("[%s] %s\n", __FILE__, __func__);
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
  roaring_bitmap_free(r1);
  roaring_bitmap_free(r2);
  assert(roaring_bitmap_get_cardinality(r1_and_r2) == 34);
  roaring_bitmap_free(r1_and_r2);
  return 1;
}

int test_union(){
  printf("[%s] %s\n", __FILE__, __func__);
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  roaring_bitmap_t *r2 = roaring_bitmap_create();
  assert(r1); assert(r2);

  for (uint32_t i=0; i < 100; ++i) {
    roaring_bitmap_add(r1, 2*i);
    roaring_bitmap_add(r2, 3*i);
    assert(roaring_bitmap_get_cardinality(r2) == i + 1);
    assert(roaring_bitmap_get_cardinality(r1) == i + 1);

  }

  roaring_bitmap_t *r1_or_r2 = roaring_bitmap_or(r1, r2);
  roaring_bitmap_free(r1);
  roaring_bitmap_free(r2);
  assert(roaring_bitmap_get_cardinality(r1_or_r2) == 166);
  roaring_bitmap_free(r1_or_r2);
  return 1;
}

int main(){
  test_printf();
  test_add();
  test_contains();
  test_intersection();
  test_union();
  printf("done toplevel tests\n");
}
