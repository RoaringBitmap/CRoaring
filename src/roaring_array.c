#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>


#include "roaring_array.h"
#include "bitset.h"
#include "containers.h"
#include "util.h"


// ported from RoaringArray.java
// Todo: optimization (eg branchless binary search)
// Go version has copy-on-write, has combo binary/sequential search
// AND: fast SIMD and on key sets; containerwise AND; SIMD partial sum
//    with +1 for nonempty containers, 0 for empty containers
//    then use this to pack the arrays for the result.

// Convention: [0,ra->size) all elements are initialized
//  [ra->size, ra->allocation_size) is junk and contains nothing needing freeing

extern int32_t ra_get_size(roaring_array_t *ra);


#define INITIAL_CAPACITY 4

roaring_array_t *ra_create() {
  roaring_array_t *new_ra = malloc(sizeof(roaring_array_t));
  if (!new_ra) return NULL;
  new_ra->keys = NULL;
  new_ra->containers = NULL;
  new_ra->typecodes = NULL;
  
  new_ra->allocation_size = INITIAL_CAPACITY;
  new_ra->keys = malloc(INITIAL_CAPACITY * sizeof(uint16_t));
  new_ra->containers = malloc(INITIAL_CAPACITY * sizeof(void *));
  new_ra->typecodes = malloc(INITIAL_CAPACITY * sizeof(uint8_t));
  if (!new_ra->keys || !new_ra->containers || !new_ra->typecodes) {
    free(new_ra);
    free(new_ra->keys);
    free(new_ra->containers);
    free(new_ra->typecodes);
	return NULL;
  }
  new_ra->size = 0;

  return new_ra;
}


roaring_array_t * ra_copy(roaring_array_t *r) {
	  roaring_array_t *new_ra = malloc(sizeof(roaring_array_t));
	  if (!new_ra) return NULL;
	  new_ra->keys = NULL;
	  new_ra->containers = NULL;
	  new_ra->typecodes = NULL;

	  const int32_t allocsize = r->allocation_size;
	  new_ra->allocation_size = allocsize;
	  new_ra->keys = malloc(allocsize * sizeof(uint16_t));
	  new_ra->containers = malloc(allocsize * sizeof(void *));
	  new_ra->typecodes = malloc(allocsize * sizeof(uint8_t));
	  if (!new_ra->keys || !new_ra->containers || !new_ra->typecodes) {
	    free(new_ra);
	    free(new_ra->keys);
	    free(new_ra->containers);
	    free(new_ra->typecodes);
		return NULL;
	  }
	  int32_t s = r->size;
	  new_ra->size = s;
	  memcpy(new_ra->keys,r->keys,s * sizeof(uint16_t));
	  memcpy(new_ra->containers,r->containers,s * sizeof(void *));
	  memcpy(new_ra->typecodes,r->typecodes,s * sizeof(uint8_t));
	  return new_ra;
}



static void ra_clear(roaring_array_t *ra) {
  free(ra->keys);
  // TODO: should the containers themselves be freed by this?
  // need to verify there are no cases where 2 roaring_arrays share containers
  // time being, assume no sharing and thus...
  for (int i=0; i < ra->size; ++i)
    container_free(ra->containers[i], ra->typecodes[i]);

  free(ra->containers);
  ra->containers = NULL; // paranoid
  free(ra->typecodes);
  ra->typecodes = NULL; // paranoid
}

void ra_free(roaring_array_t *ra) {
  ra_clear(ra);
  free(ra);
}


static void extend_array(roaring_array_t *ra, uint16_t k) {
  // corresponding Java code uses >= ??
        // fprintf(stderr,"\nextend_array\n");
  int desired_size = ra->size + (int) k;
  if (desired_size > ra->allocation_size) {
    int new_capacity = (ra->size < 1024) ? 
      2*desired_size : 5*desired_size/4;

    ra->keys = realloc( ra->keys, sizeof(uint16_t) * new_capacity);
    ra->containers = realloc( ra->containers, sizeof(void *) * new_capacity);
    ra->typecodes = realloc( ra->typecodes, sizeof(uint8_t) * new_capacity);

    if (!ra->keys || !ra->containers || !ra->typecodes) {
      fprintf(stderr,"[%s] %s\n", __FILE__, __func__);
      perror(0);
      exit(1);
    }

#if 0
    // should not be needed
    // mark the garbage entries
    for (int i = ra->allocation_size; i < new_capacity; ++i) {
      ra->typecodes[i] = UNINITIALIZED_TYPE_CODE;
      // should not be necessary
      ra->containers[i] = ra->keys[i] = 0;
    }                                               
#endif
    ra->allocation_size = new_capacity;
  }
}



void ra_append(roaring_array_t *ra, uint16_t key, void *container, uint8_t typecode) {
        //fprintf(stderr, "called ra_apend");
  extend_array(ra, 1);
  const int32_t pos = ra->size;

  ra->keys[pos] = key;
  ra->containers[pos] = container;
  ra->typecodes[pos] = typecode;
  ra->size++;
}




void ra_append_copy(roaring_array_t *ra, roaring_array_t *sa, uint16_t index) {
        //fprintf(stderr,"called ra_apend_copy");
  extend_array(ra, 1);
  const int32_t pos = ra->size;


  // old contents is junk not needing freeing
  ra->keys[pos] = sa->keys[index];
  ra->containers[pos] = container_clone(sa->containers[index], sa->typecodes[index]);
  ra->typecodes[pos] = sa->typecodes[index];
  ra->size++;
}



void ra_append_copy_range(roaring_array_t *ra, roaring_array_t *sa, 
                       uint16_t start_index, uint16_t end_index) {
        //fprintf(stderr,"called ra_apend_copy_range");
  extend_array(ra, end_index - start_index);

  for (uint16_t i = start_index; i < end_index; ++i) {
    const int32_t pos = ra->size;
    
    ra->keys[pos] = sa->keys[i];
    ra->containers[pos] = container_clone(sa->containers[i], sa->typecodes[i]);
    ra->typecodes[pos] = sa->typecodes[i];
    ra->size++;
  }
}


#if 0
// if actually used, should be documented and part of header file
void ra_append_copies_after(roaring_array_t *ra, roaring_array_t *sa,
                       uint16_t before_start) {
  int start_location = ra_get_index(sa, before_start);
  if (start_location >= 0) 
    ++start_location;
  else
    start_location = -start_location -1;

  extend_array(ra, sa->size - start_location);

  for (uint16_t i = start_location; i < sa->size; ++i) {
    const int32_t pos = ra->size;
    
    ra->keys[pos] = sa->keys[i];
    ra->containers[pos] = container_clone(sa->containers[i], sa->typecodes[i]);
    ra->typecodes[pos] = sa->typecodes[i];
    ra->size++;
  }
}
#endif


#if 0
// a form of deep equality. Keys must match and containers must test as equal in
// contents (check Java impl semantics regarding different representations of the same
// container contents, eg, run container vs one of the others)
bool equals( roaring_array_t *ra1, roaring_array_t *ra2) {
  if (ra1->size != ra2->size)
    return false;
  for (int i=0; i < ra1->size; ++i)
    if (ra1->keys[i] != ra2->keys[i] ||
        ! container_equals(ra1->containers[i], ra1->typecodes[i],
                           ra2->containers[i], ra2->typecodes[i]))
      return false;
  return true;
}
#endif


void *ra_get_container(roaring_array_t *ra, uint16_t x, uint8_t *typecode) {
  int i = binarySearch(ra->keys, (int32_t) ra->size, x);
  if (i < 0) return NULL;
  *typecode = ra->typecodes[i];
  return ra->containers[i];
}


void *ra_get_container_at_index(roaring_array_t *ra, uint16_t i, uint8_t *typecode) {
  assert(i < ra->size);
  *typecode = ra->typecodes[i];
  return ra->containers[i];
}


uint16_t ra_get_key_at_index(roaring_array_t *ra, uint16_t i) {
  return ra->keys[i];
}



int32_t ra_get_index( roaring_array_t *ra, uint16_t x) {
  // TODO: next line is possibly unsafe
//        fprintf(stderr,"rasize is %d\n", (int) ra->size);

        if (ra->size != 0) {
                //       printf(" ra->keys[ra->size-1] == %d\n",(int) ra->keys[ra->size-1]);
        }
        // the array element is uninitialized
        //fprintf(stderr," x is %d\n",(int) x);
        //fprintf(stderr,"finished prints\n");
        //fflush(stderr);

        if ( (ra->size == 0) || ra->keys[ra->size-1] == x)
                return ra->size-1;

        //if (ra->size == 4) exit(0); // temp temp


  return binarySearch(ra->keys, (int32_t) ra->size, x);
}


extern int32_t ra_advance_until( roaring_array_t *ra, uint16_t x, int32_t pos);


void ra_insert_new_key_value_at( roaring_array_t *ra, int32_t i, uint16_t key, void *container, uint8_t typecode) {
        //fprintf(stderr,"called ra_insert_new_key_value_at k=%d i=%d\n", (int) key, i);
  extend_array(ra,1);
  // May be an optimization opportunity with DIY memmove
  memmove( &(ra->keys[i+1]), &(ra->keys[i]), sizeof(uint16_t) * (ra->size - i));
  memmove( &(ra->containers[i+1]), &(ra->containers[i]), sizeof(void *) * (ra->size - i));
  memmove( &(ra->typecodes[i+1]), &(ra->typecodes[i]), sizeof(uint8_t) * (ra->size - i));
  ra->keys[i] = key; 
  ra->containers[i] = container;
  ra->typecodes[i] = typecode;
  ra->size++;
}

// note: Java routine set things to 0, enabling GC.
// Java called it "resize" but it was always used to downsize.
// Allowing upsize would break the conventions about
// valid containers below ra->size.

void ra_downsize( roaring_array_t *ra, int32_t new_length) {
  //assert(new_length <= ra->allocation_size);
  //printf("downsize from %d to %d \n", (int) ra->size, (int) new_length);
  assert(new_length <= ra->size);
  
  // free up the containers (assume not shared...)
  for (int i = new_length; i < ra->size; ++i) {
    container_free(ra->containers[i], ra->typecodes[i]);
    // by convention, these things will be above ra->size
    // and hence garbage not requiring freeing.
#if 0
    ra->containers[i] = NULL; // unnecessary, avoids dangling pointer
    ra->typecodes[i] = UNINITIALIZED_TYPE_CODE;
#endif
  }
  ra->size = new_length;
}


void ra_remove_at_index( roaring_array_t *ra, int32_t i) {
  container_free(ra->containers[i], ra->typecodes[i]);
  memmove(&(ra->containers[i]), &(ra->containers[i+1]), sizeof(void *) * (ra->size - i - 1)); 
  memmove(&(ra->keys[i]), &(ra->keys[i+1]), sizeof(uint16_t) * (ra->size - i -1));
  memmove(&(ra->typecodes[i]), &(ra->typecodes[i+1]), sizeof(uint8_t) * (ra->size - i - 1));
#if 0
  // ought to be unnecessary
  ra->keys[ra->size-1] = ra->containers[ra->size-1] = 0;
  ra->typecodes[ra->size-1] = UNINITIALIZED_TYPE_CODE;
#endif
  ra->size--;
}


void ra_remove_index_range( roaring_array_t *ra, int32_t begin, int32_t end) {
  if (end <= begin) return;

  const int range = end-begin;
  for (int i = begin; i < end; ++i) {
    container_free(ra->containers[i], ra->typecodes[i]);
    memmove(&(ra->containers[begin]), &(ra->containers[end]), sizeof(void *) * (ra->size - end));
    memmove(&(ra->keys[begin]), &(ra->keys[end]), sizeof(uint16_t) * (ra->size - end)); 
    memmove(&(ra->typecodes[begin]), &(ra->typecodes[end]), sizeof(uint8_t) * (ra->size - end)); 
  }
#if 0
  // should be unnecessary
  for (int i = 1; i <= range; ++i)
  ra->keys[ra->size-i] = ra->containers[ra->size-i] 
    = ra->typecodes[ra->size-i] = 0;
#endif
  ra->size -= range;
}

// used in inplace andNot only, to slide left the containers from
// the mutated RoaringBitmap that are after the largest container of
// the argument RoaringBitmap.  It is followed by a call to resize.
//
void ra_copy_range(roaring_array_t *ra, uint32_t begin, uint32_t end, uint32_t new_begin) {
  static bool warned_em = false;
  if (!warned_em) {
    fprintf(stderr,"[Warning] potential memory leak in ra_copy_range");
    warned_em = true;
  }
  assert(begin <= end);
  assert(new_begin < begin);

  const int range = end - begin;
  
  // TODO: there is a memory leak here, for any overwritten containers
  // that are not copied elsewhere

  memmove(&(ra->containers[new_begin]), &(ra->containers[begin]), sizeof(void *) * range);
  memmove(&(ra->keys[new_begin]), &(ra->keys[begin]), sizeof( uint16_t) * range);
  memmove(&(ra->typecodes[new_begin]), &(ra->typecodes[begin]), sizeof( uint8_t) * range);
}

void ra_set_container_at_index(roaring_array_t *ra, int32_t i, void *c, uint8_t typecode) {
  assert(i < ra->size);
  // valid container there already
  //container_free(ra->containers[i], ra->typecodes[i]);// too eager!
  // is there a possible memory leak here?
 
  ra->containers[i] = c;
  ra->typecodes[i] = typecode;
}

void ra_replace_key_and_container_at_index(roaring_array_t *ra, int32_t i, uint16_t key, void *c, uint8_t typecode) {
  assert(i < ra->size);
  //container_free(ra->containers[i], ra->typecodes[i]);//too eager!
  // is there a possible memory leak here, then?

  ra->keys[i] = key;
  ra->containers[i] = c;
  ra->typecodes[i] = typecode;
}


// just for debugging use
void show_structure(roaring_array_t *ra) {


 for( int i = 0; i <  ra->size; ++i) {
   printf(" i=%d\n",i); fflush(stdout);

   
   printf("Container %d has key %d and its type is %s  of card %d\n",
          i, (int) ra->keys[i], get_container_name( ra->typecodes[i]),
          container_get_cardinality(ra->containers[i], ra->typecodes[i]));
 }
}

// TODO : all serialization/deserialization., containerpointer abstraction.

