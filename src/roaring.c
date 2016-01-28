#include <stdio.h>
#include <assert.h>
#include <stdio.h>
#include "roaring_array.h"
#include "roaring.h"
#include <stdint.h>
#include <stdarg.h>
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

roaring_bitmap_t *roaring_bitmap_of(size_t n_args, ...) {
	// todo: could be greatly optimized
	roaring_bitmap_t * answer = roaring_bitmap_create();
	va_list ap;
	va_start(ap, n_args);
	for (size_t i = 1; i <= n_args; i++) {
		uint32_t val = va_arg(ap, uint32_t);
		roaring_bitmap_add(answer, val);

	}
	va_end(ap);
	return answer;
}


void roaring_bitmap_printf(roaring_bitmap_t *ra) {
	printf("{");
	for (int i = 0; i < ra->high_low_container->size; ++i) {
		container_printf_as_uint32_array(ra->high_low_container->containers[i],
				ra->high_low_container->typecodes[i],
				((uint32_t) ra->high_low_container->keys[i]) << 16);
		if(i+1 < ra->high_low_container->size)
			printf(",");
	}
	printf("}");

}

roaring_bitmap_t *roaring_bitmap_copy(roaring_bitmap_t *r) {
  roaring_bitmap_t *ans = (roaring_bitmap_t *) malloc( sizeof(*ans));
  if (!ans) {
    return NULL;
  }
  ans->high_low_container = ra_copy(r->high_low_container);
  if (!ans->high_low_container) {
    free(ans);
    return NULL;
  }
  return ans;
}

void roaring_bitmap_free(roaring_bitmap_t *r) {
  ra_free(r->high_low_container);
  r->high_low_container = NULL; // paranoid
  free(r);
}

void roaring_bitmap_add(roaring_bitmap_t *r, uint32_t val) {
	const uint16_t hb = val >> 16;
	const int i = ra_get_index(r->high_low_container, hb);
	uint8_t typecode;
	if (i >= 0) {
		void *container = ra_get_container_at_index(r->high_low_container, i,
				&typecode);
		uint8_t newtypecode = typecode;
		void *container2 = container_add(container, val & 0xFFFF, typecode,
				&newtypecode);
		if (container2 != container) {
			container_free(container, typecode);
			ra_set_container_at_index(r->high_low_container, i, container2,
					newtypecode);
		}
	} else {
		array_container_t *newac = array_container_create();
		void *container = container_add(newac, val & 0xFFFF,
				ARRAY_CONTAINER_TYPE_CODE, &typecode);
		// we could just assume that it stays an array container
		ra_insert_new_key_value_at(r->high_low_container, -i - 1, hb, container,
				typecode);
	}
}

bool roaring_bitmap_contains(roaring_bitmap_t *r, uint32_t val) {
	const uint16_t hb = val >> 16;
	const int i = ra_get_index(r->high_low_container, hb);
	uint8_t typecode;
	if (i >= 0) {
		void *container = ra_get_container_at_index(r->high_low_container, i,
				&typecode);
		return container_contains(container, val & 0xFFFF, typecode);
	} else {
		return false;
	}
}

// there should be some SIMD optimizations possible here
roaring_bitmap_t *roaring_bitmap_and(roaring_bitmap_t *x1, roaring_bitmap_t *x2) {
	uint8_t container_result_type = 0;
	roaring_bitmap_t *answer = roaring_bitmap_create();
	const int length1 = x1->high_low_container->size, length2 =
			x2->high_low_container->size;

	int pos1 = 0, pos2 = 0;

	while (pos1 < length1 && pos2 < length2) {
		const uint16_t s1 = ra_get_key_at_index(x1->high_low_container, pos1);
		const uint16_t s2 = ra_get_key_at_index(x2->high_low_container, pos2);

		if (s1 == s2) {
			uint8_t container_type_1, container_type_2;
			void *c1 = ra_get_container_at_index(x1->high_low_container, pos1,
					&container_type_1);
			void *c2 = ra_get_container_at_index(x2->high_low_container, pos2,
					&container_type_2);
			void *c = container_and(c1, container_type_1, c2, container_type_2,
					&container_result_type);
			if (container_nonzero_cardinality(c, container_result_type)) {
				ra_append(answer->high_low_container, s1, c,
						container_result_type);
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

roaring_bitmap_t *roaring_bitmap_or(roaring_bitmap_t *x1, roaring_bitmap_t *x2) {
	uint8_t container_result_type = 0;
	roaring_bitmap_t *answer = roaring_bitmap_create();
	const int length1 = x1->high_low_container->size, length2 =
			x2->high_low_container->size;
	if (0 == length1) {
		return roaring_bitmap_copy(x1);
	}
	if (0 == length2) {
		return roaring_bitmap_copy(x2);
	}
	int pos1 = 0, pos2 = 0;
	uint8_t container_type_1, container_type_2;
	uint16_t s1 = ra_get_key_at_index(x1->high_low_container, pos1);
	uint16_t s2 = ra_get_key_at_index(x2->high_low_container, pos2);
	while (true) {
		if (s1 == s2) {
			void *c1 = ra_get_container_at_index(x1->high_low_container, pos1,
					&container_type_1);
			void *c2 = ra_get_container_at_index(x2->high_low_container, pos2,
					&container_type_2);
			void *c = container_or(c1, container_type_1, c2, container_type_2,
					&container_result_type);
			if (container_nonzero_cardinality(c, container_result_type)) {
				ra_append(answer->high_low_container, s1, c,
						container_result_type);
			}
			++pos1;
			++pos2;
			if(pos1 == length1) break;
			if(pos2 == length2) break;
			s1 = ra_get_key_at_index(x1->high_low_container, pos1);
			s2 = ra_get_key_at_index(x2->high_low_container, pos2);

		} else if (s1 < s2) { // s1 < s2
			void *c1 = ra_get_container_at_index(x1->high_low_container, pos1,
					&container_type_1);
			ra_append(answer->high_low_container, s1, c1,
					container_type_1);
			pos1++;
			if(pos1 == length1) break;
			s1 = ra_get_key_at_index(x1->high_low_container, pos1);

		} else { // s1 > s2
			void *c2 = ra_get_container_at_index(x2->high_low_container, pos2,
					&container_type_2);
			ra_append(answer->high_low_container, s2, c2,
					container_type_2);
			pos2++;
			if(pos2 == length2) break;
			s2 = ra_get_key_at_index(x2->high_low_container, pos2);
		}
	}
	if (pos1 == length1) {
		ra_append_copy_range(answer->high_low_container, x2->high_low_container,
				pos2, length2) ;
	} else if (pos2 == length2) {
		ra_append_copy_range(answer->high_low_container, x1->high_low_container,
				pos1, length1) ;
	}
	return answer;
}

uint32_t roaring_bitmap_get_cardinality( roaring_bitmap_t *ra) {
  uint32_t ans = 0;
  for( int i = 0; i < ra->high_low_container->size; ++i)
    ans += container_get_cardinality( ra->high_low_container->containers[i], 
                                      ra->high_low_container->typecodes[i]);
  return ans;
}

uint32_t *roaring_bitmap_to_uint32_array( roaring_bitmap_t *ra, uint32_t *cardinality) {
  uint32_t *ans = malloc( roaring_bitmap_get_cardinality(ra) * sizeof( uint32_t));
  int ctr=0;

  for( int i = 0; i < ra->high_low_container->size; ++i) {
    int num_added = container_to_uint32_array( ans+ctr, 
                                               ra->high_low_container->containers[i], 
                                               ra->high_low_container->typecodes[i],
                                               ((uint32_t) ra->high_low_container->keys[i]) << 16);
    ctr += num_added;
  }
  *cardinality = ctr;
  return ans;
}

/** convert array and bitmap containers to run containers when it is more efficient;
 * also convert from run containers when more space efficient.  Returns
 * true if the result has at least one run container.
*/
bool roaring_bitmap_run_optimize(roaring_bitmap_t *r) {
  bool answer = false;
  for (int i = 0; i < r->high_low_container->size; i++) {
    uint8_t typecode_original, typecode_after;
    void *c =  ra_get_container_at_index(r->high_low_container, i, &typecode_original);
    
    void *c1 = convert_run_optimize(c, typecode_original, &typecode_after);
    if (typecode_after == RUN_CONTAINER_TYPE_CODE) answer = true;
    ra_set_container_at_index(r->high_low_container, i, c1, typecode_after);
  }
  return answer;
}


/**
 *  Remove run-length encoding even when it is more space efficient
 *  return whether a change was applied
 */
bool roaring_bitmap_remove_run_compression(roaring_bitmap_t *r) {
  bool answer = false;
  for (int i = 0; i < r->high_low_container->size; i++) {
    uint8_t typecode_original, typecode_after;
    void *c =  ra_get_container_at_index(r->high_low_container, i, &typecode_original);
    if (typecode_original == RUN_CONTAINER_TYPE_CODE) {
      answer = true;
      int32_t card = run_container_cardinality(c);
      void *c1 = convert_to_bitset_or_array_container(c, card, &typecode_after);
      ra_set_container_at_index(r->high_low_container, i, c1, typecode_after);
      run_container_free(c);
    }
  }
  return answer;
}




