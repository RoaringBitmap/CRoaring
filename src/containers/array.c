/*
 * array.c
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <x86intrin.h>

#include "array.h"
#include "util.h"

enum{DEFAULT_INIT_SIZE = 16};


extern int array_container_cardinality(const array_container_t *array);
extern bool array_container_nonzero_cardinality(const array_container_t *array);
extern void array_container_clear(array_container_t *array);


/* Create a new array with capacity size. Return NULL in case of failure. */
array_container_t *array_container_create_given_capacity(int32_t size) {
	array_container_t * arr;
	/* Allocate the array container itself. */

	if ((arr = malloc(sizeof(array_container_t))) == NULL) {
				return NULL;
	}
	if ((arr->array = malloc(sizeof(uint16_t) * size)) == NULL) {
		        free(arr);
				return NULL;
	}
	arr->capacity = size;
	arr->cardinality = 0;
	return arr;
}

/* Create a new array. Return NULL in case of failure. */
array_container_t *array_container_create() {
  return array_container_create_given_capacity( DEFAULT_INIT_SIZE);
}


/* Duplicate container */
array_container_t *array_container_clone( array_container_t *src) {
	array_container_t * answer = array_container_create_given_capacity(src->capacity);
	if(answer == NULL) return NULL;
	answer->cardinality = src->cardinality;
	memcpy(answer->array,src->array,src->cardinality*sizeof(uint16_t));
	return answer;
}

/* Free memory. */
void array_container_free(array_container_t *arr) {
	free(arr->array);
	arr->array = NULL;
	free(arr);
}


/**
 * increase capacity to at least min, and to no more than max. Whether the
 * existing data needs to be copied over depends on copy. If copy is false,
 * then the new content might be uninitialized.
 */
static void increaseCapacity(array_container_t *arr, int32_t min, int32_t max, bool copy) {
    int32_t newCapacity = (arr->capacity == 0) ? DEFAULT_INIT_SIZE :
    		arr->capacity < 64 ? arr->capacity * 2
            : arr->capacity < 1024 ? arr->capacity * 3 / 2
            : arr->capacity * 5 / 4;
    if(newCapacity < min) newCapacity = min;
    // never allocate more than we will ever need
    if (newCapacity > max)
        newCapacity = max;
    // if we are within 1/16th of the max, go to max
    if( newCapacity < max - max/16)
        newCapacity = max;
    arr->capacity = newCapacity;
    if(copy)
      arr->array = realloc(arr->array,arr->capacity * sizeof(uint16_t)) ;
    else {
      free(arr->array);
      arr->array = malloc(arr->capacity * sizeof(uint16_t)) ;
    }
    // TODO: handle the case where realloc fails
    if(arr->array == NULL) {
    	printf("Well, that's unfortunate. Did I say you could use this code in production?\n");
    }
}


/* Copy one container into another. We assume that they are distinct. */
void array_container_copy(array_container_t *source, array_container_t *dest) {
	if(source->cardinality < dest->capacity) {
		increaseCapacity(dest,source->cardinality,INT32_MAX, false);
	}
	dest->cardinality = source->cardinality;
	memcpy(dest->array,source->array,sizeof(uint16_t)*source->cardinality);

}


static void append(array_container_t *arr, uint16_t i) {
	if(arr->cardinality == arr->capacity) increaseCapacity(arr,arr->capacity+1,INT32_MAX, true);
	arr->array[arr->cardinality] = i;
	arr->cardinality++;
}

/* Add x to the set. Returns true if x was not already present.  */
bool array_container_add(array_container_t *arr, uint16_t x) {
	if (( (arr->cardinality > 0) && (arr->array[arr->cardinality-1] < x)) || (arr->cardinality == 0)) {
		append(arr, x);
		return true;
	}

	int32_t loc = binarySearch(arr->array,arr->cardinality, x);
	if (loc < 0) {// not already present
		if(arr->capacity == arr->capacity) increaseCapacity(arr,arr->capacity+1,INT32_MAX,true);
		int32_t i = -loc - 1;
		memmove(arr->array + i + 1,arr->array + i,(arr->cardinality - i)*sizeof(uint16_t));
		arr->array[i] = x;
		arr->cardinality ++;
		return true;
	} else return false;
}

/* Remove x from the set. Returns true if x was present.  */
bool array_container_remove(array_container_t *arr, uint16_t x) {
	int32_t loc = binarySearch(arr->array,arr->cardinality, x);
	if (loc >= 0) {
		memmove(arr->array + loc ,arr->array + loc + 1,(arr->cardinality - loc) * sizeof(uint16_t));
		arr->cardinality --;
		return true;
	} else return false;
}

/* Check whether x is present.  */
bool array_container_contains(const array_container_t *arr, uint16_t x) {
	int32_t loc = binarySearch(arr->array,arr->cardinality,x);
	return loc >= 0; // could possibly be faster...
}



// TODO: can one vectorize the computation of the union?
static int32_t union2by2(uint16_t * set1, int32_t lenset1,
		uint16_t * set2, int32_t lenset2, uint16_t * buffer){
	int32_t pos = 0;
	int32_t k1 = 0;
	int32_t k2 = 0;
	if (0 == lenset2) {
		memcpy(buffer,set1,lenset1*sizeof(uint16_t));// is this safe if buffer is set1 or set2?
		return lenset1;
	}
	if (0 == lenset1) {
		memcpy(buffer,set2,lenset2*sizeof(uint16_t));// is this safe if buffer is set1 or set2?
		return lenset2;
	}
	uint16_t s1 = set1[k1];
	uint16_t s2 = set2[k2];
	while(1) {
		if (s1 < s2) {
			buffer[pos] = s1;
			pos++;
			k1++;
			if (k1 >= lenset1) {
				memcpy(buffer + pos,set2 + k2,(lenset2-k2)*sizeof(uint16_t));// is this safe if buffer is set1 or set2?
				pos += lenset2 - k2;
				break;
			}
			s1 = set1[k1];
		} else if (s1 == s2) {
			buffer[pos] = s1;
			pos++;
			k1++;
			k2++;
			if (k1 >= lenset1) {
				memcpy(buffer + pos,set2 + k2,(lenset2-k2)*sizeof(uint16_t));// is this safe if buffer is set1 or set2?
				pos += lenset2 - k2;
				break;
			}
			if (k2 >= lenset2) {
				memcpy(buffer + pos,set1 + k1,(lenset1-k1)*sizeof(uint16_t));// is this safe if buffer is set1 or set2?
				pos += lenset1 - k1;
				break;
			}
			s1 = set1[k1];
			s2 = set2[k2];
		} else { // if (set1[k1]>set2[k2])
			buffer[pos] = s2;
			pos++;
			k2++;
			if (k2 >= lenset2) {
				// should be memcpy
				memcpy(buffer + pos,set1 + k1,(lenset1-k1)*sizeof(uint16_t));// is this safe if buffer is set1 or set2?
				pos += lenset1 - k1;
				break;
			}
			s2 = set2[k2];
		}
	}
	return pos;
}





/* Computes the union of array1 and array2 and write the result to arrayout.
 * It is assumed that arrayout is distinct from both array1 and array2.
 */
void array_container_union(const array_container_t *array1,
                        const array_container_t *array2,
                        array_container_t *arrayout) {
	int32_t totc = array1->cardinality +  array2->cardinality ;
	if(arrayout->capacity < totc)
		increaseCapacity(arrayout,totc,INT32_MAX,false);
	arrayout->cardinality =  union2by2(array1->array, array1->cardinality,
			array2->array, array2->cardinality, arrayout->array);
}





int32_t onesidedgallopingintersect2by2(
		uint16_t * smallset, int32_t lensmallset,
		uint16_t * largeset, int32_t lenlargeset,	uint16_t * buffer)  {

	if( 0 == lensmallset) {
		return 0;
	}
	int32_t k1 = 0;
	int32_t k2 = 0;
	int32_t pos = 0;
	uint16_t s1 = largeset[k1];
	uint16_t s2 = smallset[k2];
    while(true) {
		if (s1 < s2) {
			k1 = advanceUntil(largeset, k1, lenlargeset, s2);
			if (k1 == lenlargeset) {
				break;
			}
			s1 = largeset[k1];
		}
		if (s2 < s1) {
			k2++;
			if (k2 == lensmallset) {
				break;
			}
			s2 = smallset[k2];
		} else {

			buffer[pos] = s2;
			pos++;
			k2++;
			if (k2 == lensmallset) {
				break;
			}
			s2 = smallset[k2];
			k1 = advanceUntil(largeset, k1, lenlargeset, s2);
			if (k1 == lenlargeset) {
				break;
			}
			s1 = largeset[k1];
		}

	}
	return pos;
}

int32_t match_scalar(const uint16_t *A, const int32_t lenA,
                    const uint16_t *B, const int32_t lenB,
                    uint16_t *out) {

    const uint16_t *initout = out;
    if (lenA == 0 || lenB == 0) return 0;

    const uint16_t *endA = A + lenA;
    const uint16_t *endB = B + lenB;

    while (1) {
        while (*A < *B) {
SKIP_FIRST_COMPARE:
            if (++A == endA) goto FINISH;
        }
        while (*A > *B) {
            if (++B == endB) goto FINISH;
        }
        if (*A == *B) {
            *out++ = *A;
            if (++A == endA || ++B == endB) goto FINISH;
        } else {
            goto SKIP_FIRST_COMPARE;
        }
    }

FINISH:
    return (out - initout);
}




#ifdef USEAVX

// used by intersect_vector16
static const uint8_t shuffle_mask16[] __attribute__((aligned(0x1000))) = { -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, 0, 1, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, 2, 3, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 1, 2, 3, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 6, 7, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 6, 7, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 6, 7, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, 4, 5, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 1, 4, 5, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 6,
		7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, -1,
		-1, -1, -1, -1, -1, -1, -1, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, 0, 1, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, 2, 3, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,
		1, 2, 3, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 4, 5, 8, 9, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 8, 9, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 8, 9, -1, -1, -1, -1, -1, -1, -1,
		-1, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 6,
		7, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, 8, 9, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 8, 9, -1, -1, -1,
		-1, -1, -1, -1, -1, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, 0, 1, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 4,
		5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, -1, -1, -1, -1, -1, -1, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, 0, 1, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, 2, 3, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, 0, 1, 2, 3, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 4,
		5, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5,
		10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 10, 11, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 10, 11, -1, -1,
		-1, -1, -1, -1, -1, -1, 6, 7, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, 0, 1, 6, 7, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, 2, 3, 6, 7, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2,
		3, 6, 7, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 4, 5, 6, 7, 10, 11, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 6, 7, 10, 11, -1, -1,
		-1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 6, 7, 10, 11, -1, -1, -1, -1, -1,
		-1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 10, 11, -1, -1, -1, -1, -1, -1, 8,
		9, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 8, 9,
		10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 8, 9, 10, 11, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 8, 9, 10, 11, -1, -1,
		-1, -1, -1, -1, -1, -1, 4, 5, 8, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, 0, 1, 4, 5, 8, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 2,
		3, 4, 5, 8, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5,
		8, 9, 10, 11, -1, -1, -1, -1, -1, -1, 6, 7, 8, 9, 10, 11, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, 0, 1, 6, 7, 8, 9, 10, 11, -1, -1, -1, -1,
		-1, -1, -1, -1, 2, 3, 6, 7, 8, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1,
		-1, 0, 1, 2, 3, 6, 7, 8, 9, 10, 11, -1, -1, -1, -1, -1, -1, 4, 5, 6, 7,
		8, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 6, 7, 8, 9,
		10, 11, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1, -1,
		-1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1, -1, -1, -1,
		12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1,
		12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 12, 13,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 12, 13, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, 4, 5, 12, 13, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 12, 13, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, 2, 3, 4, 5, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, 0, 1, 2, 3, 4, 5, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 6, 7, 12,
		13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 6, 7, 12, 13,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, 12, 13, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 12, 13, -1, -1, -1, -1,
		-1, -1, -1, -1, 4, 5, 6, 7, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, 0, 1, 4, 5, 6, 7, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 4,
		5, 6, 7, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7,
		12, 13, -1, -1, -1, -1, -1, -1, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, 0, 1, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, 2, 3, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 1, 2, 3, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 4, 5, 8, 9,
		12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, 12,
		13, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 8, 9, 12, 13, -1, -1,
		-1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 8, 9, 12, 13, -1, -1, -1, -1,
		-1, -1, 6, 7, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,
		1, 6, 7, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, 8, 9,
		12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 8, 9, 12, 13,
		-1, -1, -1, -1, -1, -1, 4, 5, 6, 7, 8, 9, 12, 13, -1, -1, -1, -1, -1,
		-1, -1, -1, 0, 1, 4, 5, 6, 7, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1, 2,
		3, 4, 5, 6, 7, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5,
		6, 7, 8, 9, 12, 13, -1, -1, -1, -1, 10, 11, 12, 13, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, 0, 1, 10, 11, 12, 13, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, 2, 3, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, 0, 1, 2, 3, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1,
		4, 5, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4,
		5, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 10, 11,
		12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 10, 11, 12,
		13, -1, -1, -1, -1, -1, -1, 6, 7, 10, 11, 12, 13, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, 0, 1, 6, 7, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1,
		-1, -1, 2, 3, 6, 7, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0,
		1, 2, 3, 6, 7, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, 4, 5, 6, 7, 10,
		11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 6, 7, 10, 11,
		12, 13, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, -1,
		-1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, -1, -1, -1,
		-1, 8, 9, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1,
		8, 9, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 8, 9, 10,
		11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 8, 9, 10, 11,
		12, 13, -1, -1, -1, -1, -1, -1, 4, 5, 8, 9, 10, 11, 12, 13, -1, -1, -1,
		-1, -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, 10, 11, 12, 13, -1, -1, -1, -1,
		-1, -1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, 0, 1,
		2, 3, 4, 5, 8, 9, 10, 11, 12, 13, -1, -1, -1, -1, 6, 7, 8, 9, 10, 11,
		12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 6, 7, 8, 9, 10, 11, 12,
		13, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, -1, -1,
		-1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, -1, -1, -1, -1,
		4, 5, 6, 7, 8, 9, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 6,
		7, 8, 9, 10, 11, 12, 13, -1, -1, -1, -1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
		12, 13, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
		-1, -1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 1, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 14,
		15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 14, 15,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 4, 5, 14, 15, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 14, 15, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, 2, 3, 4, 5, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, 0, 1, 2, 3, 4, 5, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 6, 7,
		14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 6, 7, 14,
		15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, 14, 15, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 14, 15, -1, -1, -1,
		-1, -1, -1, -1, -1, 4, 5, 6, 7, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, 0, 1, 4, 5, 6, 7, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3,
		4, 5, 6, 7, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6,
		7, 14, 15, -1, -1, -1, -1, -1, -1, 8, 9, 14, 15, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, 0, 1, 8, 9, 14, 15, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, 2, 3, 8, 9, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 1, 2, 3, 8, 9, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 4, 5, 8, 9,
		14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, 14,
		15, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 8, 9, 14, 15, -1, -1,
		-1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 8, 9, 14, 15, -1, -1, -1, -1,
		-1, -1, 6, 7, 8, 9, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,
		1, 6, 7, 8, 9, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, 8, 9,
		14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 8, 9, 14, 15,
		-1, -1, -1, -1, -1, -1, 4, 5, 6, 7, 8, 9, 14, 15, -1, -1, -1, -1, -1,
		-1, -1, -1, 0, 1, 4, 5, 6, 7, 8, 9, 14, 15, -1, -1, -1, -1, -1, -1, 2,
		3, 4, 5, 6, 7, 8, 9, 14, 15, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5,
		6, 7, 8, 9, 14, 15, -1, -1, -1, -1, 10, 11, 14, 15, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, 0, 1, 10, 11, 14, 15, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, 2, 3, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, 0, 1, 2, 3, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1,
		4, 5, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4,
		5, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 10, 11,
		14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 10, 11, 14,
		15, -1, -1, -1, -1, -1, -1, 6, 7, 10, 11, 14, 15, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, 0, 1, 6, 7, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1,
		-1, -1, 2, 3, 6, 7, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,
		1, 2, 3, 6, 7, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, 4, 5, 6, 7, 10,
		11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 6, 7, 10, 11,
		14, 15, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 6, 7, 10, 11, 14, 15, -1,
		-1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 14, 15, -1, -1, -1,
		-1, 8, 9, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1,
		8, 9, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 8, 9, 10,
		11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 8, 9, 10, 11,
		14, 15, -1, -1, -1, -1, -1, -1, 4, 5, 8, 9, 10, 11, 14, 15, -1, -1, -1,
		-1, -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, 10, 11, 14, 15, -1, -1, -1, -1,
		-1, -1, 2, 3, 4, 5, 8, 9, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, 0, 1,
		2, 3, 4, 5, 8, 9, 10, 11, 14, 15, -1, -1, -1, -1, 6, 7, 8, 9, 10, 11,
		14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 6, 7, 8, 9, 10, 11, 14,
		15, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, 8, 9, 10, 11, 14, 15, -1, -1,
		-1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 14, 15, -1, -1, -1, -1,
		4, 5, 6, 7, 8, 9, 10, 11, 14, 15, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 6,
		7, 8, 9, 10, 11, 14, 15, -1, -1, -1, -1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
		14, 15, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15,
		-1, -1, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 1, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 12,
		13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 12, 13,
		14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 4, 5, 12, 13, 14, 15, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 12, 13, 14, 15, -1, -1, -1,
		-1, -1, -1, -1, -1, 2, 3, 4, 5, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1,
		-1, -1, 0, 1, 2, 3, 4, 5, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 6, 7,
		12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 6, 7, 12,
		13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, 12, 13, 14, 15,
		-1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 12, 13, 14, 15, -1,
		-1, -1, -1, -1, -1, 4, 5, 6, 7, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1,
		-1, -1, 0, 1, 4, 5, 6, 7, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 2, 3,
		4, 5, 6, 7, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6,
		7, 12, 13, 14, 15, -1, -1, -1, -1, 8, 9, 12, 13, 14, 15, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, 0, 1, 8, 9, 12, 13, 14, 15, -1, -1, -1, -1, -1,
		-1, -1, -1, 2, 3, 8, 9, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 1, 2, 3, 8, 9, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 4, 5, 8, 9,
		12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, 12,
		13, 14, 15, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 8, 9, 12, 13, 14, 15,
		-1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 8, 9, 12, 13, 14, 15, -1, -1,
		-1, -1, 6, 7, 8, 9, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,
		1, 6, 7, 8, 9, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 2, 3, 6, 7, 8, 9,
		12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 8, 9, 12, 13,
		14, 15, -1, -1, -1, -1, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, -1, -1, -1,
		-1, -1, -1, 0, 1, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, -1, -1, -1, -1, 2,
		3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5,
		6, 7, 8, 9, 12, 13, 14, 15, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, 0, 1, 10, 11, 12, 13, 14, 15, -1, -1, -1,
		-1, -1, -1, -1, -1, 2, 3, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1,
		-1, -1, -1, 0, 1, 2, 3, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1,
		4, 5, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4,
		5, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 2, 3, 4, 5, 10, 11,
		12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 10, 11, 12,
		13, 14, 15, -1, -1, -1, -1, 6, 7, 10, 11, 12, 13, 14, 15, -1, -1, -1,
		-1, -1, -1, -1, -1, 0, 1, 6, 7, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1,
		-1, -1, 2, 3, 6, 7, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0,
		1, 2, 3, 6, 7, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, 4, 5, 6, 7, 10,
		11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 6, 7, 10, 11,
		12, 13, 14, 15, -1, -1, -1, -1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14,
		15, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15, -1,
		-1, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1,
		8, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 2, 3, 8, 9, 10,
		11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 8, 9, 10, 11,
		12, 13, 14, 15, -1, -1, -1, -1, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15, -1,
		-1, -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1,
		-1, -1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, 0, 1,
		2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1, 6, 7, 8, 9, 10, 11,
		12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0, 1, 6, 7, 8, 9, 10, 11, 12,
		13, 14, 15, -1, -1, -1, -1, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		-1, -1, -1, -1, 0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1,
		4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, 0, 1, 4, 5, 6,
		7, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
		12, 13, 14, 15, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
		14, 15 };

/**
 * From Schlegel et al., Fast Sorted-Set Intersection using SIMD Instructions
 *
 * Optimized by D. Lemire on May 3rd 2013
 *
 * Question: Can this benefit from AVX?
 */
static int32_t intersect_vector16(const uint16_t *A, int32_t s_a,
		const uint16_t *B, int32_t s_b, uint16_t * C) {
	int32_t count = 0;
	int32_t i_a = 0, i_b = 0;

	const int32_t st_a = (s_a / 8) * 8;
	const int32_t st_b = (s_b / 8) * 8;
	__m128i v_a, v_b;
	if ((i_a < st_a) && (i_b < st_b)) {
		v_a = _mm_lddqu_si128((__m128i *) &A[i_a]);
		v_b = _mm_lddqu_si128((__m128i *) &B[i_b]);
		while ((A[i_a] == 0) || (B[i_b] == 0)) {
			const __m128i res_v = _mm_cmpestrm(v_b, 8, v_a, 8,
					_SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_BIT_MASK);
			const int r = _mm_extract_epi32(res_v, 0);
			__m128i sm16 = _mm_load_si128((const __m128i *) shuffle_mask16 + r);
			__m128i p = _mm_shuffle_epi8(v_a, sm16);
			_mm_storeu_si128((__m128i *) &C[count], p);
			count += _mm_popcnt_u32(r);
			const uint16_t a_max = A[i_a + 7];
			const uint16_t b_max = B[i_b + 7];
			if (a_max <= b_max) {
				i_a += 8;
				if (i_a == st_a)
					break;
				v_a = _mm_lddqu_si128((__m128i *) &A[i_a]);

			}
			if (b_max <= a_max) {
				i_b += 8;
				if (i_b == st_b)
					break;
				v_b = _mm_lddqu_si128((__m128i *) &B[i_b]);

			}
		}
		if ((i_a < st_a) && (i_b < st_b))
			while (true) {
				const __m128i res_v = _mm_cmpistrm(v_b, v_a,
						_SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_BIT_MASK);
				const int r = _mm_extract_epi32(res_v, 0);
				__m128i sm16 = _mm_load_si128(
						(const __m128i *) shuffle_mask16 + r);
				__m128i p = _mm_shuffle_epi8(v_a, sm16);
				_mm_storeu_si128((__m128i *) &C[count], p);
				count += _mm_popcnt_u32(r);
				const uint16_t a_max = A[i_a + 7];
				const uint16_t b_max = B[i_b + 7];
				if (a_max <= b_max) {
					i_a += 8;
					if (i_a == st_a)
						break;
					v_a = _mm_lddqu_si128((__m128i *) &A[i_a]);

				}
				if (b_max <= a_max) {
					i_b += 8;
					if (i_b == st_b)
						break;
					v_b = _mm_lddqu_si128((__m128i *) &B[i_b]);

				}
			}
	}
	// intersect the tail using scalar intersection
	while (i_a < s_a && i_b < s_b) {
		int16_t a = A[i_a];
		int16_t b = B[i_b];
		if (a < b) {
			i_a++;
		} else if (b < a) {
			i_b++;
		} else {
			C[count] = a;		//==b;
			count++;
			i_a++;
			i_b++;
		}
	}

	return count;
}



/*
 *
 * Useless crap kept around temporarily

static int32_t intersectV1avx_vector16(const uint16_t *A, const int32_t s_a, const uint16_t *B,
		 const int32_t s_b, uint16_t *C) {
	if (s_a > s_b)
		return intersectV1avx_vector16(B, s_b, A, s_a, C);
	int32_t count = 0;
	int32_t i_a = 0, i_b = 0;
	const int32_t st_a = s_a;
	const int32_t howmanyvec = 2;
	const int32_t numberofintspervec = howmanyvec* sizeof(__m256i)/ sizeof(uint16_t);

	const int32_t st_b = (s_b / numberofintspervec) * numberofintspervec;
	__m256i v_b1,v_b2;
	if ((i_a < st_a) && (i_b < st_b)) {
		while(i_a < st_a) {
			uint16_t a = A[i_a];
			const __m256i v_a = _mm256_set1_epi16(a);
			while (B[i_b + numberofintspervec - 1] < a) {
				i_b += numberofintspervec;
				if (i_b == st_b)
					goto FINISH_SCALAR;
			}
			v_b1 = _mm256_lddqu_si256((const __m256i *) &B[i_b]);
			v_b2 = _mm256_lddqu_si256((const __m256i *) &B[i_b] + 1);

			__m256i F0 = _mm256_cmpeq_epi16(v_a, v_b1);
			__m256i F1 = _mm256_cmpeq_epi16(v_a, v_b2);
			F0 = _mm256_or_si256(F0,F1);
			count += !_mm256_testz_si256(F0, F0);
			C[count] = a;
			++i_a;
		}
	}
	FINISH_SCALAR:
	// intersect the tail using scalar intersection
	while (i_a < s_a && i_b < s_b) {
		if (A[i_a] < B[i_b]) {
			i_a++;
		} else if (B[i_b] < A[i_a]) {
			i_b++;
		} else {
			C[count] = A[i_a];
			count++;
			i_a++;
			i_b++;
		}
	}
	return count;
}
*/
/*
 *
 * Useless crap kept around temporarily
static int32_t intersectV2avx_vector16(const uint16_t *A, const int32_t s_a, const uint16_t *B,
		 const int32_t s_b, uint16_t *C) {
	if (s_a > s_b)
		return intersectV1avx_vector16(B, s_b, A, s_a, C);
	int32_t count = 0;
	int32_t i_a = 0, i_b = 0;
	const int32_t st_a = s_a;
	const int32_t howmanyvec = 4;
	const int32_t numberofintspervec = howmanyvec* sizeof(__m256i)/ sizeof(uint16_t);

	const int32_t st_b = (s_b / numberofintspervec) * numberofintspervec;
	__m256i v_b1,v_b2,v_b3,v_b4;
	if ((i_a < st_a) && (i_b < st_b)) {
		while(i_a < st_a) {
			uint16_t a = A[i_a];
			const __m256i v_a = _mm256_set1_epi16(a);
			while (B[i_b + numberofintspervec - 1] < a) {
				i_b += numberofintspervec;
				if (i_b == st_b)
					goto FINISH_SCALAR;
			}
			v_b1 = _mm256_lddqu_si256((const __m256i *) &B[i_b]);
			v_b2 = _mm256_lddqu_si256((const __m256i *) &B[i_b] + 1);
			v_b3 = _mm256_lddqu_si256((const __m256i *) &B[i_b] + 2);
			v_b4 = _mm256_lddqu_si256((const __m256i *) &B[i_b] + 3);

			__m256i F0 = _mm256_cmpeq_epi16(v_a, v_b1);
			__m256i F1 = _mm256_cmpeq_epi16(v_a, v_b2);
			__m256i F2 = _mm256_cmpeq_epi16(v_a, v_b3);
			__m256i F3 = _mm256_cmpeq_epi16(v_a, v_b4);

			F0 = _mm256_or_si256(F0,F1);
			F2 = _mm256_or_si256(F2,F3);

			F0 = _mm256_or_si256(F0,F2);
			count += !_mm256_testz_si256(F0, F0);
			C[count] = a;
			++i_a;
		}
	}
	FINISH_SCALAR:
	// intersect the tail using scalar intersection
	while (i_a < s_a && i_b < s_b) {
		if (A[i_a] < B[i_b]) {
			i_a++;
		} else if (B[i_b] < A[i_a]) {
			i_b++;
		} else {
			C[count] = A[i_a];
			count++;
			i_a++;
			i_b++;
		}
	}
	return count;
}*/

int32_t intersection2by2(
		uint16_t * set1, int32_t lenset1,
		uint16_t * set2, int32_t lenset2,
		uint16_t * buffer) {
	const int32_t thres = 64; // TODO: adjust this threshold
	if (lenset1 * thres < lenset2) {
		// TODO: a SIMD-enabled galloping intersection algorithm should be designed
		return onesidedgallopingintersect2by2(set1, lenset1, set2, lenset2, buffer);
	} else if( lenset2 * thres < lenset1) {
		// TODO: a SIMD-enabled galloping intersection algorithm should be designed
		return onesidedgallopingintersect2by2(set2, lenset2, set1, lenset1, buffer);
	} else {
		return intersect_vector16(set1,lenset1,set2,lenset2,buffer);
	}
}

#else

int32_t intersection2by2(
	uint16_t * set1, int32_t lenset1,
	uint16_t * set2, int32_t lenset2,
	uint16_t * buffer)  {
	int32_t thres = 64; // TODO: adjust this threshold
	if (lenset1 * thres < lenset2) {
		return onesidedgallopingintersect2by2(set1, lenset1, set2, lenset2, buffer);
	} else if( lenset2 * thres < lenset1) {
		return onesidedgallopingintersect2by2(set2, lenset2, set1, lenset1, buffer);
	} else {
		return match_scalar(set1, lenset1, set2, lenset2, buffer);
	}
}

#endif // #ifdef USEAVX


/* computes the intersection of array1 and array2 and write the result to
 * arrayout.
 * It is assumed that arrayout is distinct from both array1 and array2.
 * */
void array_container_intersection(const array_container_t *array1,
                         const array_container_t *array2,
                         array_container_t *arrayout) {
	int32_t minc = array1->cardinality < array2->cardinality ? array1->cardinality : array2->cardinality;
	if(arrayout->capacity < minc)
		increaseCapacity(arrayout,minc,INT32_MAX,false);
	arrayout->cardinality =  intersection2by2(array1->array, array1->cardinality,
			array2->array, array2->cardinality, arrayout->array);
}


int array_container_to_uint32_array( uint32_t *out, const array_container_t *cont, uint32_t base) {
  int outpos = 0;
  for (int i = 0; i < cont->cardinality; ++i) {
    out[outpos++] = base + cont->array[i];
  }
  return outpos;
}
