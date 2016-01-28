#include <stdio.h>
#include <assert.h>

#include "roaring.h"


void show_structure(roaring_array_t *);  // debug

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

static int array_equals(uint32_t  *a1, int32_t size1, uint32_t *a2, int32_t size2) {
  if (size1 != size2) return 0;
  for (int i=0; i < size1; ++i)
    if (a1[i] != a2[i]) {
      printf("array_equals a1[%d] is %d != a2[%d] is %d\n", i, a1[i], i, a2[i]);
      return 0;
    }
  return 1;
}



int test_conversion_to_int_array(){
  printf("[%s] %s\n", __FILE__, __func__);
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  int ans_ctr = 0;
  uint32_t *ans = calloc(100000, sizeof(int32_t));

  // a dense bitmap container  (best done with runs)
  for (uint32_t i=0; i < 50000; ++i) {
    if (i != 30000) { // making 2 runs
	  roaring_bitmap_add(r1, i);
          ans[ans_ctr++]=i;
    }
  }

  // a sparse one
  for (uint32_t i=70000; i < 130000; i += 17) {
	  roaring_bitmap_add(r1, i);
          ans[ans_ctr++]=i;
  }

  // a dense one but not good for runs

  for (uint32_t i= 65536*3; i < 65536*4; i++) {
    if (i % 3 != 0) {
	  roaring_bitmap_add(r1, i);
          ans[ans_ctr++]=i;
    }
  }

  // TODO: run_optimize it

  uint32_t card;
  uint32_t *arr = roaring_bitmap_to_uint32_array(r1, &card);

  printf("arrays have %d and %d\n",card, ans_ctr);
  show_structure(r1->high_low_container);
  assert(array_equals(arr,card, ans, ans_ctr));
  roaring_bitmap_free(r1);
  free(arr);
  return 1;
}



int main(){
  test_printf();
  test_add();
  test_contains();
  test_intersection();
  test_union();
  test_conversion_to_int_array();
  printf("done toplevel tests\n");
}
