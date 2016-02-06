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

enum { DEFAULT_INIT_SIZE = 16 };

extern int array_container_cardinality(const array_container_t *array);
extern bool array_container_nonzero_cardinality(const array_container_t *array);
extern void array_container_clear(array_container_t *array);

/* Create a new array with capacity size. Return NULL in case of failure. */
array_container_t *array_container_create_given_capacity(int32_t size) {
    array_container_t *container;

    if ((container = malloc(sizeof(array_container_t))) == NULL) {
        return NULL;
    }

    if ((container->array = malloc(sizeof(uint16_t) * size)) == NULL) {
        free(container);
        return NULL;
    }

    container->capacity = size;
    container->cardinality = 0;

    return container;
}

/* Create a new array. Return NULL in case of failure. */
array_container_t *array_container_create() {
    return array_container_create_given_capacity(DEFAULT_INIT_SIZE);
}

/* Duplicate container */
array_container_t *array_container_clone(array_container_t *src) {
    array_container_t *new =
        array_container_create_given_capacity(src->capacity);
    if (new == NULL) return NULL;

    new->cardinality = src->cardinality;

    memcpy(new->array, src->array, src->cardinality * sizeof(uint16_t));

    return new;
}

/* Free memory. */
void array_container_free(array_container_t *arr) {
    free(arr->array);
    arr->array = NULL;
    free(arr);
}

static inline int32_t grow_capacity(int32_t capacity) {
    return (capacity <= 0) ? DEFAULT_INIT_SIZE
                           : capacity < 64 ? capacity * 2
                                           : capacity < 1024 ? capacity * 3 / 2
                                                             : capacity * 5 / 4;
}

/**
 * increase capacity to at least min, and to no more than max. Whether the
 * existing data needs to be copied over depends on copy. If preserve is false,
 * then the new content will be uninitialized.
 */
static void array_container_grow(array_container_t *container, int32_t min,
                                 int32_t max, bool preserve) {
    int32_t new_capacity = clamp(grow_capacity(container->capacity), min, max);

    // if we are within 1/16th of the max, go to max
    if (new_capacity < max - max / 16) new_capacity = max;

    container->capacity = new_capacity;
    uint16_t *array = container->array;

    if (preserve) {
        container->array = realloc(array, new_capacity * sizeof(uint16_t));
    } else {
        free(array);
        container->array = malloc(new_capacity * sizeof(uint16_t));
    }

    // TODO: handle the case where realloc fails
    assert(container->array != NULL);
}

/* Copy one container into another. We assume that they are distinct. */
void array_container_copy(const array_container_t *src,
                          array_container_t *dst) {
    const int32_t cardinality = src->cardinality;
    if (cardinality < dst->capacity) {
        array_container_grow(dst, cardinality, INT32_MAX, false);
    }

    dst->cardinality = cardinality;
    memcpy(dst->array, src->array, cardinality * sizeof(uint16_t));
}

static void array_container_append(array_container_t *arr, uint16_t pos) {
    const int32_t capacity = arr->capacity;

    if (array_container_full(arr)) {
        array_container_grow(arr, capacity + 1, INT32_MAX, true);
    }

    arr->array[arr->cardinality++] = pos;
}

/* Add x to the set. Returns true if x was not already present.  */
bool array_container_add(array_container_t *arr, uint16_t pos) {
    const int32_t cardinality = arr->cardinality;

    // best case, we can append.
    if (array_container_empty(arr) || (arr->array[cardinality - 1] < pos)) {
        array_container_append(arr, pos);
        return true;
    }

    const int32_t loc = binarySearch(arr->array, cardinality, pos);
    const bool not_found = loc < 0;

    if (not_found) {
        if (array_container_full(arr)) {
            array_container_grow(arr, arr->capacity + 1, INT32_MAX, true);
        }
        const int32_t insert_idx = -loc - 1;
        memmove(arr->array + insert_idx + 1, arr->array + insert_idx,
                (cardinality - insert_idx) * sizeof(uint16_t));
        arr->array[insert_idx] = pos;
        arr->cardinality++;
    }

    return not_found;
}

/* Remove x from the set. Returns true if x was present.  */
bool array_container_remove(array_container_t *arr, uint16_t pos) {
    const int32_t idx = binarySearch(arr->array, arr->cardinality, pos);
    const bool is_present = idx >= 0;
    if (is_present) {
        memmove(arr->array + idx, arr->array + idx + 1,
                (arr->cardinality - idx) * sizeof(uint16_t));
        arr->cardinality--;
    }

    return is_present;
}

/* Check whether x is present.  */
bool array_container_contains(const array_container_t *arr, uint16_t x) {
    int32_t loc = binarySearch(arr->array, arr->cardinality, x);
    return loc >= 0;  // could possibly be faster...
}

// TODO: can one vectorize the computation of the union?
static int32_t union2by2(uint16_t *set1, int32_t lenset1, uint16_t *set2,
                         int32_t lenset2, uint16_t *buffer) {
    int32_t pos = 0;
    int32_t k1 = 0;
    int32_t k2 = 0;
    if (0 == lenset2) {
        memcpy(
            buffer, set1,
            lenset1 *
                sizeof(uint16_t));  // is this safe if buffer is set1 or set2?
        return lenset1;
    }
    if (0 == lenset1) {
        memcpy(
            buffer, set2,
            lenset2 *
                sizeof(uint16_t));  // is this safe if buffer is set1 or set2?
        return lenset2;
    }
    uint16_t s1 = set1[k1];
    uint16_t s2 = set2[k2];
    while (1) {
        if (s1 < s2) {
            buffer[pos] = s1;
            pos++;
            k1++;
            if (k1 >= lenset1) {
                memcpy(buffer + pos, set2 + k2,
                       (lenset2 - k2) * sizeof(uint16_t));  // is this safe if
                                                            // buffer is set1 or
                                                            // set2?
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
                memcpy(buffer + pos, set2 + k2,
                       (lenset2 - k2) * sizeof(uint16_t));  // is this safe if
                                                            // buffer is set1 or
                                                            // set2?
                pos += lenset2 - k2;
                break;
            }
            if (k2 >= lenset2) {
                memcpy(buffer + pos, set1 + k1,
                       (lenset1 - k1) * sizeof(uint16_t));  // is this safe if
                                                            // buffer is set1 or
                                                            // set2?
                pos += lenset1 - k1;
                break;
            }
            s1 = set1[k1];
            s2 = set2[k2];
        } else {  // if (set1[k1]>set2[k2])
            buffer[pos] = s2;
            pos++;
            k2++;
            if (k2 >= lenset2) {
                // should be memcpy
                memcpy(buffer + pos, set1 + k1,
                       (lenset1 - k1) * sizeof(uint16_t));  // is this safe if
                                                            // buffer is set1 or
                                                            // set2?
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
    int32_t totc = array1->cardinality + array2->cardinality;
    if (arrayout->capacity < totc)
        array_container_grow(arrayout, totc, INT32_MAX, false);
    arrayout->cardinality =
        union2by2(array1->array, array1->cardinality, array2->array,
                  array2->cardinality, arrayout->array);
}

int32_t onesidedgallopingintersect2by2(uint16_t *smallset, int32_t lensmallset,
                                       uint16_t *largeset, int32_t lenlargeset,
                                       uint16_t *buffer) {
    if (0 == lensmallset) {
        return 0;
    }
    int32_t k1 = 0;
    int32_t k2 = 0;
    int32_t pos = 0;
    uint16_t s1 = largeset[k1];
    uint16_t s2 = smallset[k2];
    while (true) {
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

int32_t match_scalar(const uint16_t *A, const int32_t lenA, const uint16_t *B,
                     const int32_t lenB, uint16_t *out) {
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
static const uint8_t shuffle_mask16[] __attribute__((aligned(0x1000))) = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 4,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 0,  1,  4,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 2,  3,  4,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,
    2,  3,  4,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 6,  7,  -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  6,  7,  -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  6,  7,  -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 0,  1,  2,  3,  6,  7,  -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 4,  5,  6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,
    1,  4,  5,  6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,
    6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,
    7,  -1, -1, -1, -1, -1, -1, -1, -1, 8,  9,  -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 0,  1,  8,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 2,  3,  8,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    0,  1,  2,  3,  8,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 4,  5,  8,
    9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  8,  9,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  8,  9,  -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  8,  9,  -1, -1, -1, -1,
    -1, -1, -1, -1, 6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 0,  1,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,
    6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  6,
    7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, -1, 4,  5,  6,  7,  8,  9,  -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  6,  7,  8,  9,  -1, -1, -1,
    -1, -1, -1, -1, -1, 2,  3,  4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1,
    -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, 10,
    11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  10, 11,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  10, 11, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  10, 11, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 4,  5,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 0,  1,  4,  5,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    2,  3,  4,  5,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,
    3,  4,  5,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 6,  7,  10, 11, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  6,  7,  10, 11, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 2,  3,  6,  7,  10, 11, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 0,  1,  2,  3,  6,  7,  10, 11, -1, -1, -1, -1, -1, -1, -1,
    -1, 4,  5,  6,  7,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,
    4,  5,  6,  7,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  6,
    7,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,
    10, 11, -1, -1, -1, -1, -1, -1, 8,  9,  10, 11, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 0,  1,  8,  9,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 2,  3,  8,  9,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,
    1,  2,  3,  8,  9,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 4,  5,  8,  9,
    10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  8,  9,  10,
    11, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  8,  9,  10, 11, -1, -1,
    -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  8,  9,  10, 11, -1, -1, -1,
    -1, -1, -1, 6,  7,  8,  9,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    0,  1,  6,  7,  8,  9,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  6,
    7,  8,  9,  10, 11, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  6,  7,
    8,  9,  10, 11, -1, -1, -1, -1, -1, -1, 4,  5,  6,  7,  8,  9,  10, 11, -1,
    -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  6,  7,  8,  9,  10, 11, -1, -1,
    -1, -1, -1, -1, 2,  3,  4,  5,  6,  7,  8,  9,  10, 11, -1, -1, -1, -1, -1,
    -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, -1, -1, -1, -1, 12, 13,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  12, 13, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  12, 13, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  12, 13, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 4,  5,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 0,  1,  4,  5,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,
    3,  4,  5,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,
    4,  5,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 6,  7,  12, 13, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  6,  7,  12, 13, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 2,  3,  6,  7,  12, 13, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 0,  1,  2,  3,  6,  7,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1,
    4,  5,  6,  7,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,
    5,  6,  7,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  6,  7,
    12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  12,
    13, -1, -1, -1, -1, -1, -1, 8,  9,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 0,  1,  8,  9,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 2,  3,  8,  9,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,
    2,  3,  8,  9,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 4,  5,  8,  9,  12,
    13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  8,  9,  12, 13,
    -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  8,  9,  12, 13, -1, -1, -1,
    -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  8,  9,  12, 13, -1, -1, -1, -1,
    -1, -1, 6,  7,  8,  9,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,
    1,  6,  7,  8,  9,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  6,  7,
    8,  9,  12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  6,  7,  8,
    9,  12, 13, -1, -1, -1, -1, -1, -1, 4,  5,  6,  7,  8,  9,  12, 13, -1, -1,
    -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  6,  7,  8,  9,  12, 13, -1, -1, -1,
    -1, -1, -1, 2,  3,  4,  5,  6,  7,  8,  9,  12, 13, -1, -1, -1, -1, -1, -1,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  12, 13, -1, -1, -1, -1, 10, 11, 12,
    13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  10, 11, 12, 13,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  10, 11, 12, 13, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  10, 11, 12, 13, -1, -1, -1, -1,
    -1, -1, -1, -1, 4,  5,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 0,  1,  4,  5,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,
    4,  5,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,
    5,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, 6,  7,  10, 11, 12, 13, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  6,  7,  10, 11, 12, 13, -1, -1, -1,
    -1, -1, -1, -1, -1, 2,  3,  6,  7,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1,
    -1, -1, 0,  1,  2,  3,  6,  7,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, 4,
    5,  6,  7,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,
    6,  7,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  6,  7,  10,
    11, 12, 13, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  10, 11,
    12, 13, -1, -1, -1, -1, 8,  9,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 0,  1,  8,  9,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1,
    2,  3,  8,  9,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,
    3,  8,  9,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, 4,  5,  8,  9,  10, 11,
    12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  8,  9,  10, 11, 12,
    13, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  8,  9,  10, 11, 12, 13, -1, -1,
    -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  8,  9,  10, 11, 12, 13, -1, -1, -1,
    -1, 6,  7,  8,  9,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,
    6,  7,  8,  9,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, 2,  3,  6,  7,  8,
    9,  10, 11, 12, 13, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  6,  7,  8,  9,
    10, 11, 12, 13, -1, -1, -1, -1, 4,  5,  6,  7,  8,  9,  10, 11, 12, 13, -1,
    -1, -1, -1, -1, -1, 0,  1,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, -1, -1,
    -1, -1, 2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, -1, -1, -1, -1, 0,
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, -1, -1, 14, 15, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  14, 15, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  14, 15, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  14, 15, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 4,  5,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    0,  1,  4,  5,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,
    5,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,
    14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 6,  7,  14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 0,  1,  6,  7,  14, 15, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 2,  3,  6,  7,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 0,  1,  2,  3,  6,  7,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 4,  5,
    6,  7,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  6,
    7,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  6,  7,  14, 15,
    -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  14, 15, -1,
    -1, -1, -1, -1, -1, 8,  9,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 0,  1,  8,  9,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,
    3,  8,  9,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,
    8,  9,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 4,  5,  8,  9,  14, 15, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  8,  9,  14, 15, -1, -1,
    -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  8,  9,  14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, 0,  1,  2,  3,  4,  5,  8,  9,  14, 15, -1, -1, -1, -1, -1, -1,
    6,  7,  8,  9,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  6,
    7,  8,  9,  14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  6,  7,  8,  9,
    14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  6,  7,  8,  9,  14,
    15, -1, -1, -1, -1, -1, -1, 4,  5,  6,  7,  8,  9,  14, 15, -1, -1, -1, -1,
    -1, -1, -1, -1, 0,  1,  4,  5,  6,  7,  8,  9,  14, 15, -1, -1, -1, -1, -1,
    -1, 2,  3,  4,  5,  6,  7,  8,  9,  14, 15, -1, -1, -1, -1, -1, -1, 0,  1,
    2,  3,  4,  5,  6,  7,  8,  9,  14, 15, -1, -1, -1, -1, 10, 11, 14, 15, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  10, 11, 14, 15, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  10, 11, 14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 0,  1,  2,  3,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1,
    -1, -1, 4,  5,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,
    1,  4,  5,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,
    10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  10,
    11, 14, 15, -1, -1, -1, -1, -1, -1, 6,  7,  10, 11, 14, 15, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 0,  1,  6,  7,  10, 11, 14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, 2,  3,  6,  7,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1,
    0,  1,  2,  3,  6,  7,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, 4,  5,  6,
    7,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  6,  7,
    10, 11, 14, 15, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  6,  7,  10, 11, 14,
    15, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  10, 11, 14, 15,
    -1, -1, -1, -1, 8,  9,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 0,  1,  8,  9,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,
    8,  9,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  8,
    9,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, 4,  5,  8,  9,  10, 11, 14, 15,
    -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  8,  9,  10, 11, 14, 15, -1,
    -1, -1, -1, -1, -1, 2,  3,  4,  5,  8,  9,  10, 11, 14, 15, -1, -1, -1, -1,
    -1, -1, 0,  1,  2,  3,  4,  5,  8,  9,  10, 11, 14, 15, -1, -1, -1, -1, 6,
    7,  8,  9,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  6,  7,
    8,  9,  10, 11, 14, 15, -1, -1, -1, -1, -1, -1, 2,  3,  6,  7,  8,  9,  10,
    11, 14, 15, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  6,  7,  8,  9,  10, 11,
    14, 15, -1, -1, -1, -1, 4,  5,  6,  7,  8,  9,  10, 11, 14, 15, -1, -1, -1,
    -1, -1, -1, 0,  1,  4,  5,  6,  7,  8,  9,  10, 11, 14, 15, -1, -1, -1, -1,
    2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 14, 15, -1, -1, -1, -1, 0,  1,  2,
    3,  4,  5,  6,  7,  8,  9,  10, 11, 14, 15, -1, -1, 12, 13, 14, 15, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  12, 13, 14, 15, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 2,  3,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 0,  1,  2,  3,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1,
    -1, 4,  5,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,
    4,  5,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  12,
    13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  12, 13,
    14, 15, -1, -1, -1, -1, -1, -1, 6,  7,  12, 13, 14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 0,  1,  6,  7,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1,
    -1, -1, 2,  3,  6,  7,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,
    1,  2,  3,  6,  7,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 4,  5,  6,  7,
    12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  6,  7,  12,
    13, 14, 15, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  6,  7,  12, 13, 14, 15,
    -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, -1,
    -1, -1, -1, 8,  9,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    0,  1,  8,  9,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  8,
    9,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  8,  9,
    12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 4,  5,  8,  9,  12, 13, 14, 15, -1,
    -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  8,  9,  12, 13, 14, 15, -1, -1,
    -1, -1, -1, -1, 2,  3,  4,  5,  8,  9,  12, 13, 14, 15, -1, -1, -1, -1, -1,
    -1, 0,  1,  2,  3,  4,  5,  8,  9,  12, 13, 14, 15, -1, -1, -1, -1, 6,  7,
    8,  9,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  6,  7,  8,
    9,  12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 2,  3,  6,  7,  8,  9,  12, 13,
    14, 15, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  6,  7,  8,  9,  12, 13, 14,
    15, -1, -1, -1, -1, 4,  5,  6,  7,  8,  9,  12, 13, 14, 15, -1, -1, -1, -1,
    -1, -1, 0,  1,  4,  5,  6,  7,  8,  9,  12, 13, 14, 15, -1, -1, -1, -1, 2,
    3,  4,  5,  6,  7,  8,  9,  12, 13, 14, 15, -1, -1, -1, -1, 0,  1,  2,  3,
    4,  5,  6,  7,  8,  9,  12, 13, 14, 15, -1, -1, 10, 11, 12, 13, 14, 15, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  10, 11, 12, 13, 14, 15, -1, -1,
    -1, -1, -1, -1, -1, -1, 2,  3,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, 0,  1,  2,  3,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1,
    4,  5,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  4,
    5,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 2,  3,  4,  5,  10, 11,
    12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  10, 11, 12,
    13, 14, 15, -1, -1, -1, -1, 6,  7,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1,
    -1, -1, -1, -1, 0,  1,  6,  7,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1,
    -1, 2,  3,  6,  7,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0,  1,
    2,  3,  6,  7,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, 4,  5,  6,  7,  10,
    11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  6,  7,  10, 11,
    12, 13, 14, 15, -1, -1, -1, -1, 2,  3,  4,  5,  6,  7,  10, 11, 12, 13, 14,
    15, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  10, 11, 12, 13, 14, 15,
    -1, -1, 8,  9,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, 0,
    1,  8,  9,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 2,  3,  8,  9,
    10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  8,  9,  10,
    11, 12, 13, 14, 15, -1, -1, -1, -1, 4,  5,  8,  9,  10, 11, 12, 13, 14, 15,
    -1, -1, -1, -1, -1, -1, 0,  1,  4,  5,  8,  9,  10, 11, 12, 13, 14, 15, -1,
    -1, -1, -1, 2,  3,  4,  5,  8,  9,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1,
    0,  1,  2,  3,  4,  5,  8,  9,  10, 11, 12, 13, 14, 15, -1, -1, 6,  7,  8,
    9,  10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, 0,  1,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, -1, -1, -1, -1, 2,  3,  6,  7,  8,  9,  10, 11, 12,
    13, 14, 15, -1, -1, -1, -1, 0,  1,  2,  3,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, -1, -1, 4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, -1, -1, -1,
    -1, 0,  1,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, -1, -1, 2,  3,
    4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, -1, -1, 0,  1,  2,  3,  4,
    5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15};

/**
 * From Schlegel et al., Fast Sorted-Set Intersection using SIMD Instructions
 *
 * Optimized by D. Lemire on May 3rd 2013
 *
 * Question: Can this benefit from AVX?
 */
static int32_t intersect_vector16(const uint16_t *A, int32_t s_a,
                                  const uint16_t *B, int32_t s_b, uint16_t *C) {
    int32_t count = 0;
    int32_t i_a = 0, i_b = 0;

    const int32_t st_a = (s_a / 8) * 8;
    const int32_t st_b = (s_b / 8) * 8;
    __m128i v_a, v_b;
    if ((i_a < st_a) && (i_b < st_b)) {
        v_a = _mm_lddqu_si128((__m128i *)&A[i_a]);
        v_b = _mm_lddqu_si128((__m128i *)&B[i_b]);
        while ((A[i_a] == 0) || (B[i_b] == 0)) {
            const __m128i res_v = _mm_cmpestrm(
                v_b, 8, v_a, 8,
                _SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_BIT_MASK);
            const int r = _mm_extract_epi32(res_v, 0);
            __m128i sm16 = _mm_load_si128((const __m128i *)shuffle_mask16 + r);
            __m128i p = _mm_shuffle_epi8(v_a, sm16);
            _mm_storeu_si128((__m128i *)&C[count], p);
            count += _mm_popcnt_u32(r);
            const uint16_t a_max = A[i_a + 7];
            const uint16_t b_max = B[i_b + 7];
            if (a_max <= b_max) {
                i_a += 8;
                if (i_a == st_a) break;
                v_a = _mm_lddqu_si128((__m128i *)&A[i_a]);
            }
            if (b_max <= a_max) {
                i_b += 8;
                if (i_b == st_b) break;
                v_b = _mm_lddqu_si128((__m128i *)&B[i_b]);
            }
        }
        if ((i_a < st_a) && (i_b < st_b))
            while (true) {
                const __m128i res_v = _mm_cmpistrm(
                    v_b, v_a,
                    _SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_BIT_MASK);
                const int r = _mm_extract_epi32(res_v, 0);
                __m128i sm16 =
                    _mm_load_si128((const __m128i *)shuffle_mask16 + r);
                __m128i p = _mm_shuffle_epi8(v_a, sm16);
                _mm_storeu_si128((__m128i *)&C[count], p);
                count += _mm_popcnt_u32(r);
                const uint16_t a_max = A[i_a + 7];
                const uint16_t b_max = B[i_b + 7];
                if (a_max <= b_max) {
                    i_a += 8;
                    if (i_a == st_a) break;
                    v_a = _mm_lddqu_si128((__m128i *)&A[i_a]);
                }
                if (b_max <= a_max) {
                    i_b += 8;
                    if (i_b == st_b) break;
                    v_b = _mm_lddqu_si128((__m128i *)&B[i_b]);
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
            C[count] = a;  //==b;
            count++;
            i_a++;
            i_b++;
        }
    }

    return count;
}

int32_t intersection2by2(uint16_t *set1, int32_t lenset1, uint16_t *set2,
                         int32_t lenset2, uint16_t *buffer) {
    const int32_t thres = 64;  // TODO: adjust this threshold
    if (lenset1 * thres < lenset2) {
        // TODO: a SIMD-enabled galloping intersection algorithm should be
        // designed
        return onesidedgallopingintersect2by2(set1, lenset1, set2, lenset2,
                                              buffer);
    } else if (lenset2 * thres < lenset1) {
        // TODO: a SIMD-enabled galloping intersection algorithm should be
        // designed
        return onesidedgallopingintersect2by2(set2, lenset2, set1, lenset1,
                                              buffer);
    } else {
        return intersect_vector16(set1, lenset1, set2, lenset2, buffer);
    }
}

#else

int32_t intersection2by2(uint16_t *set1, int32_t lenset1, uint16_t *set2,
                         int32_t lenset2, uint16_t *buffer) {
    int32_t thres = 64;  // TODO: adjust this threshold
    if (lenset1 * thres < lenset2) {
        return onesidedgallopingintersect2by2(set1, lenset1, set2, lenset2,
                                              buffer);
    } else if (lenset2 * thres < lenset1) {
        return onesidedgallopingintersect2by2(set2, lenset2, set1, lenset1,
                                              buffer);
    } else {
        return match_scalar(set1, lenset1, set2, lenset2, buffer);
    }
}

#endif  // #ifdef USEAVX

/* computes the intersection of array1 and array2 and write the result to
 * arrayout.
 * It is assumed that arrayout is distinct from both array1 and array2.
 * */
void array_container_intersection(const array_container_t *array1,
                                  const array_container_t *array2,
                                  array_container_t *arrayout) {
    int32_t minc = array1->cardinality < array2->cardinality
                       ? array1->cardinality
                       : array2->cardinality;
    if (arrayout->capacity < minc)
        array_container_grow(arrayout, minc, INT32_MAX, false);
    arrayout->cardinality =
        intersection2by2(array1->array, array1->cardinality, array2->array,
                         array2->cardinality, arrayout->array);
}

int array_container_to_uint32_array(uint32_t *out,
                                    const array_container_t *cont,
                                    uint32_t base) {
    int outpos = 0;
    for (int i = 0; i < cont->cardinality; ++i) {
        out[outpos++] = base + cont->array[i];
    }
    return outpos;
}

void array_container_printf(const array_container_t *v) {
    if (v->cardinality == 0) {
        printf("{}");
        return;
    }
    printf("{");
    printf("%d", v->array[0]);
    for (int i = 1; i < v->cardinality; ++i) {
        printf(",%d", v->array[i]);
    }
    printf("}");
}

void array_container_printf_as_uint32_array(const array_container_t *v,
                                            uint32_t base) {
    if (v->cardinality == 0) {
        return;
    }
    printf("%d", v->array[0] + base);
    for (int i = 1; i < v->cardinality; ++i) {
        printf(",%d", v->array[i] + base);
    }
}
