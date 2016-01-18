#ifndef CONTAINERS_CONTAINERS_H
#define CONTAINERS_CONTAINERS_H

#include "bitset.h"
#include "array.h"
#include "run.h"

// don't use an enum: needs constant folding
#define BITSET_CONTAINER_TYPE_CODE 3
#define ARRAY_CONTAINER_TYPE_CODE  1
#define RUN_CONTAINER_TYPE_CODE    2
//UNINITIALIZED_TYPE_CODE=0}; // can probably avoid using uninit code

extern  inline void container_free( void *container, uint8_t typecode) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    bitset_container_free( (bitset_container_t *) container); break;
#if 0
  case ARRAY_CONTAINER_TYPE_CODE:
    array_container_free(  (array_container_t *) container); break;
  case RUN_CONTAINER_TYPE_CODE:
    run_container_free( (run_container_t *) container); break;
#endif
    //  case UNINITIALIZED_TYPE_CODE: break;
  }
}

extern inline void *container_clone(uint8_t typecode, void *container) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    return bitset_container_clone( (bitset_container_t *) container);
#if 0
  case ARRAY_CONTAINER_TYPE_CODE:
    return array_container_clone(  (array_container_t *) container);
  case RUN_CONTAINER_TYPE_CODE:
    return run_container_clone( (run_container_t *) container);
#endif
  default:
    assert(0);
    *(NULL);
    return NULL;
  }
}


#if 0
// TODO enable and debug this equality stuff
extern inline bool container_equals(void *c1, uint8_t type1, void *c2, uint8_t type2) {
  switch (type1*4 + type2) {
  case BITSET_CONTAINER_TYPE_CODE*4 + BITSET_CONTAINER_TYPE_CODE:
    return bitset_container_equals( (bitset_container_t *) c1, (bitset_container_t *) c2);
  case BITSET_CONTAINER_TYPE_CODE*4 + RUN_CONTAINER_TYPE_CODE:
    return run_container_equals_bitset( (run_container_t *) c2, (bitset_container_t *) c1);
  case RUN_CONTAINER_TYPE_CODE*4 + BITSET_CONTAINER_TYPE_CODE:
    return run_container_equals_bitset( (run_container_t *) c1, (bitset_container_t *) c2);
  case BITSET_CONTAINER_TYPE_CODE*4 + ARRAY_CONTAINER_TYPE_CODE:
    return false;
  case ARRAY_CONTAINER_TYPE_CODE*4 + BITSET_CONTAINER_TYPE_CODE:
    return false;
  case ARRAY_CONTAINER_TYPE_CODE*4 + RUN_CONTAINER_TYPE_CODE:
    return run_container_equals_array( (run_container_t *) c2, (array_container_t *) c1);
  case RUN_CONTAINER_TYPE_CODE*4 + ARRAY_CONTAINER_TYPE_CODE:
    return run_container_equals_array( (run_container_t *) c1, (array_container_t *) c2);
  case ARRAY_CONTAINER_TYPE_CODE*4 + ARRAY_CONTAINER_TYPE_CODE:
    return array_container_equals( (array_container_t *) c1, (array_container_t *) c2);
  case RUN_CONTAINER_TYPE_CODE*4 + RUN_CONTAINER_TYPE_CODE:
    return run_container_equals( (run_container_t *) c1, (run_container_t *) c2);
  }
}
#endif

// macro-izations possibilities for generic non-inplace binary-op dispatch

extern inline void *container_and(void *c1, uint8_t type1, void *c2, uint8_t type2, uint8_t *result_type) {
void *result;
switch (type1*4 + type2) {
 case (BITSET_CONTAINER_TYPE_CODE*4 + BITSET_CONTAINER_TYPE_CODE):
   result = bitset_container_create();

    // temp temp, type signature is to return an int, destination param is third
    int result_card =  bitset_container_and( c1, c2, result);
if (result_card < SPARSE_THRESHOLD)  { // find whatever name DL uused
      // temp temp, container conversion?? Better not here!
*result_type = ARRAY_CONTAINER_TYPE_CODE;  // find DL name
       return array_container_from_bitset(result); // assume it recycles memory as necessary
    }
    *result_type = BITSET_CONTAINER_TYPE_CODE;
    return result;

#if 0
  case BITSET_CONTAINER_TYPE_CODE*4 + RUN_CONTAINER_TYPE_CODE:
    return run_container_and_bitset( (run_container_t *) c2, (bitset_container_t *) c1);
  case RUN_CONTAINER_TYPE_CODE*4 + BITSET_CONTAINER_TYPE_CODE:
    return run_container_and_bitset( (run_container_t *) c1, (bitset_container_t *) c2);
  case BITSET_CONTAINER_TYPE_CODE*4 + ARRAY_CONTAINER_TYPE_CODE:
    return bitset_container_and_array( (bitset_container t *) c1, (array_container_t *) c2);
  case ARRAY_CONTAINER_TYPE_CODE*4 + BITSET_CONTAINER_TYPE_CODE:
    return bitset_container_and_array( (bitset_container t *) c2, (array_container_t *) c1);
  case ARRAY_CONTAINER_TYPE_CODE*4 + RUN_CONTAINER_TYPE_CODE:
    return run_container_and_array( (run_container_t) c2, (array_container_t) c1);
  case RUN_CONTAINER_TYPE_CODE*4 + ARRAY_CONTAINER_TYPE_CODE:
    return run_container_and_array( (run_container_t) c1, (array_container_t) c2);
  case ARRAY_CONTAINER_TYPE_CODE*4 + ARRAY_CONTAINER_TYPE_CODE:
    return array_container_and( (array_container_t) c1, (array_container_t) c2);
  case RUN_CONTAINER_TYPE_CODE*4 + RUN_CONTAINER_TYPE_CODE:
    return run_container_and( (run_container_t) c1, (run_container_t) c2);
#endif
  }
}

#endif
