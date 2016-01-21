#include <stdio.h>
#include <assert.h>

#include "roaring.h"

int test1(){
  printf("test1\n");
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  assert(r1);

  for (int i=0; i < 100; ++i)
    roaring_bitmap_add(r1, 2*i);
    
  assert(roaring_bitmap_get_cardinality(r1) == 100);
  return 1;
}


int test2(){
  printf("test2\n");
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  roaring_bitmap_t *r2 = roaring_bitmap_create();
  assert(r1); assert(r2);

  for (int i=0; i < 100; ++i) {
    roaring_bitmap_add(r1, 2*i);                      
    roaring_bitmap_add(r2, 3*i);
  }
   
  roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
  assert(roaring_bitmap_get_cardinality(r1_and_r2) == 16);
  return 1;
}

int main(){

  test1();
  test2();
  printf("done toplevel tests");

}
