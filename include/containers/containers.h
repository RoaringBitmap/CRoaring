#ifndef CONTAINERS_CONTAINERS_H
#define CONTAINERS_CONTAINERS_H

#include <stdbool.h>
#include <assert.h>

#include "bitset.h"
#include "array.h"
#include "run.h"


// don't use an enum: needs constant folding
// should revisit

#define BITSET_CONTAINER_TYPE_CODE 3
#define ARRAY_CONTAINER_TYPE_CODE  1
#define RUN_CONTAINER_TYPE_CODE    2
//UNINITIALIZED_TYPE_CODE=0}; // can probably avoid using uninit code

/**
 * Get the container name from the typecode
 */
inline char * get_container_name(uint8_t typecode) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    return "bitset";
  case ARRAY_CONTAINER_TYPE_CODE:
	return "array";
  case RUN_CONTAINER_TYPE_CODE:
    return "run";
  }
  return "unknown";
}


/**
 * Get the container cardinality (number of elements), requires a  typecode
 */
inline uint32_t container_get_cardinality(void *container, uint8_t typecode) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    return bitset_container_cardinality(container);
  case ARRAY_CONTAINER_TYPE_CODE:
    return array_container_cardinality(container);
  case RUN_CONTAINER_TYPE_CODE:
    return run_container_cardinality(container);
  }
  return 0; // unreached
}



/**
 * Checks whether a container is not empty, requires a  typecode
 */
inline bool container_nonzero_cardinality(void *container, uint8_t typecode) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    return bitset_container_nonzero_cardinality(container);
  case ARRAY_CONTAINER_TYPE_CODE:
    return array_container_nonzero_cardinality(container);
  case RUN_CONTAINER_TYPE_CODE:
    return run_container_nonzero_cardinality(container);
  }
  return 0;//unreached
}



/**
 * Recover memory from a container, requires a  typecode
 */
inline void container_free( void *container, uint8_t typecode) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    bitset_container_free( (bitset_container_t *) container); break;
  case ARRAY_CONTAINER_TYPE_CODE:
    array_container_free(  (array_container_t *) container); break;
  case RUN_CONTAINER_TYPE_CODE:
    run_container_free( (run_container_t *) container); break;
    //  case UNINITIALIZED_TYPE_CODE: break;
  }
}


/**
 * Convert a container to an array of values, requires a  typecode as well as a "base" (most significant values)
 */
inline void container_to_uint32_array( uint32_t *output, void *container, uint8_t typecode, uint32_t base) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    bitset_container_to_uint32_array( output, container, base); break;
  case ARRAY_CONTAINER_TYPE_CODE:
    array_container_to_uint32_array(  output, container, base); break;
  case RUN_CONTAINER_TYPE_CODE:
    run_container_to_uint32_array( output, container, base); break;
    //  case UNINITIALIZED_TYPE_CODE: break;
  }
}

/**
 * Add a value to a container, requires a  typecode, fills in new_typecode and return (possibly different) container.
 * This function may allocate a new container, and caller is responsible for memory deallocation
 */
inline void *container_add(  void *container, uint16_t val, uint8_t typecode, uint8_t *new_typecode) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    bitset_container_set( (bitset_container_t *) container, val);
    *new_typecode = BITSET_CONTAINER_TYPE_CODE;
    return container;
  case ARRAY_CONTAINER_TYPE_CODE: ;
    array_container_t *ac = (array_container_t *) container;
    array_container_add(ac, val);
    if (array_container_cardinality(ac)  > DEFAULT_MAX_SIZE) {
      *new_typecode = BITSET_CONTAINER_TYPE_CODE;
      return bitset_container_from_array(ac);
    }
    else {
      *new_typecode = ARRAY_CONTAINER_TYPE_CODE;
      return ac;
    }
  case RUN_CONTAINER_TYPE_CODE:
    // per Java, no container type adjustments are done (revisit?)
    run_container_add( (run_container_t *) container, val);
    *new_typecode = RUN_CONTAINER_TYPE_CODE;
    return container;
  default:
    assert(0);
    return NULL;
  }
}

/**
 * Check whether a value is in a container, requires a  typecode
 */
inline bool container_contains(  void *container, uint16_t val, uint8_t typecode) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    return bitset_container_get( (bitset_container_t *) container, val);
  case ARRAY_CONTAINER_TYPE_CODE: ;
    return array_container_contains( (array_container_t *) container, val);
  case RUN_CONTAINER_TYPE_CODE:
	return run_container_contains( (run_container_t *) container, val);
  default:
    assert(0);
    return NULL;
  }
}


/**
 * Copies a container, requires a typecode. This allocates new memory, caller
 * is responsible for deallocation.
 */
inline void *container_clone(void *container, uint8_t typecode) {
  switch (typecode) {
  case BITSET_CONTAINER_TYPE_CODE:
    return bitset_container_clone( (bitset_container_t *) container);
  case ARRAY_CONTAINER_TYPE_CODE:
    return array_container_clone(  (array_container_t *) container);
  case RUN_CONTAINER_TYPE_CODE:
    return run_container_clone( (run_container_t *) container);
  default:
    assert(0);
    return NULL;
  }
}


#if 0
// TODO enable and debug this equality stuff
inline bool container_equals(void *c1, uint8_t type1, void *c2, uint8_t type2) {
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


/**
 * Compute intersection between two containers, generate a new container (having type result_type), requires a typecode. This allocates new memory, caller
 * is responsible for deallocation.
 */
inline void *container_and(void *c1, uint8_t type1, void *c2, uint8_t type2,
		uint8_t *result_type) {
	void *result;
	switch (type1 * 4 + type2) {
	case (BITSET_CONTAINER_TYPE_CODE * 4 + BITSET_CONTAINER_TYPE_CODE):
		result = bitset_container_create();

		// temp temp, type signature is to return an int, destination param is third
		int result_card = bitset_container_and(c1, c2, result);
		if (result_card <= DEFAULT_MAX_SIZE) {
			// temp temp, container conversion?? Better not here!
			*result_type = ARRAY_CONTAINER_TYPE_CODE;
			return (void *) array_container_from_bitset(result, result_card); // assume it recycles memory as necessary
		}
		*result_type = BITSET_CONTAINER_TYPE_CODE;
		return result;
	case ARRAY_CONTAINER_TYPE_CODE * 4 + ARRAY_CONTAINER_TYPE_CODE:
		result = array_container_create();

		array_container_intersection(c1, c2, result);
		*result_type = ARRAY_CONTAINER_TYPE_CODE;
		return result;
	case RUN_CONTAINER_TYPE_CODE * 4 + RUN_CONTAINER_TYPE_CODE:
		result = run_container_create();
		run_container_intersection(c1, c2, result);
		*result_type = RUN_CONTAINER_TYPE_CODE;
		// ToDo, conversion to bitset or array
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
#endif
	}
	return 0; // unreached
}

/**
 * Compute union between two containers, generate a new container (having type result_type), requires a typecode. This allocates new memory, caller
 * is responsible for deallocation.
 */
inline void *container_or(void *c1, uint8_t type1, void *c2, uint8_t type2,
		uint8_t *result_type) {
	void *result;
	switch (type1 * 4 + type2) {
	case (BITSET_CONTAINER_TYPE_CODE * 4 + BITSET_CONTAINER_TYPE_CODE):
		result = bitset_container_create();
		//int result_card =
				bitset_container_or(c1, c2, result);
		*result_type = BITSET_CONTAINER_TYPE_CODE;
		return result;
	case ARRAY_CONTAINER_TYPE_CODE * 4 + ARRAY_CONTAINER_TYPE_CODE:
		result = array_container_create();
// TODO: this is not correct
		array_container_union(c1, c2, result);
		*result_type = ARRAY_CONTAINER_TYPE_CODE;
		return result;
	case RUN_CONTAINER_TYPE_CODE * 4 + RUN_CONTAINER_TYPE_CODE:
		result = run_container_create();
// TODO: this is not correct
		run_container_union(c1, c2, result);
		*result_type = RUN_CONTAINER_TYPE_CODE;
		// ToDo, conversion to bitset or array
		return result;
#if 0
		case BITSET_CONTAINER_TYPE_CODE*4 + RUN_CONTAINER_TYPE_CODE:
		return run_container_or_bitset( (run_container_t *) c2, (bitset_container_t *) c1);
		case RUN_CONTAINER_TYPE_CODE*4 + BITSET_CONTAINER_TYPE_CODE:
		return run_container_or_bitset( (run_container_t *) c1, (bitset_container_t *) c2);
		case BITSET_CONTAINER_TYPE_CODE*4 + ARRAY_CONTAINER_TYPE_CODE:
		return bitset_container_or_array( (bitset_container t *) c1, (array_container_t *) c2);
		case ARRAY_CONTAINER_TYPE_CODE*4 + BITSET_CONTAINER_TYPE_CODE:
		return bitset_container_or_array( (bitset_container t *) c2, (array_container_t *) c1);
		case ARRAY_CONTAINER_TYPE_CODE*4 + RUN_CONTAINER_TYPE_CODE:
		return run_container_or_array( (run_container_t) c2, (array_container_t) c1);
		case RUN_CONTAINER_TYPE_CODE*4 + ARRAY_CONTAINER_TYPE_CODE:
		return run_container_or_array( (run_container_t) c1, (array_container_t) c2);
#endif
	}
	return 0; // unreached
}

#endif
