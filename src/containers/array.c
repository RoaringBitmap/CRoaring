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
extern bool array_container_empty(const array_container_t *array);
extern bool array_container_full(const array_container_t *array);

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

static inline int32_t clamp(int32_t val, int32_t min, int32_t max) {
    return ((val < min) ? min : (val > max) ? max : val);
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
    if (new_capacity > max - max / 16) new_capacity = max;

    container->capacity = new_capacity;
    uint16_t *array = container->array;

    if (preserve) {
        container->array = realloc(array, new_capacity * sizeof(uint16_t));
        if(container->array == NULL) free(array);
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
bool array_container_contains(const array_container_t *arr, uint16_t pos) {
    return binarySearch(arr->array, arr->cardinality, pos) >= 0;
}

// TODO: can one vectorize the computation of the union?
static size_t union_uint16(const uint16_t *set_1, size_t size_1,
                           const uint16_t *set_2, size_t size_2,
                           uint16_t *buffer) {
    size_t pos = 0, idx_1 = 0, idx_2 = 0;

    if (0 == size_2) {
        memcpy(buffer, set_1, size_1 * sizeof(uint16_t));
        return size_1;
    }
    if (0 == size_1) {
        memcpy(buffer, set_2, size_2 * sizeof(uint16_t));
        return size_2;
    }

    uint16_t val_1 = set_1[idx_1], val_2 = set_2[idx_2];

    while (true) {
        if (val_1 < val_2) {
            buffer[pos++] = val_1;
            ++idx_1;
            if (idx_1 >= size_1) break;
            val_1 = set_1[idx_1];
        } else if (val_2 < val_1) {
            buffer[pos++] = val_2;
            ++idx_2;
            if (idx_2 >= size_2) break;
            val_2 = set_2[idx_2];
        } else {
            buffer[pos++] = val_1;
            ++idx_1;
            ++idx_2;
            if (idx_1 >= size_1 || idx_2 >= size_2) break;
            val_1 = set_1[idx_1];
            val_2 = set_2[idx_2];
        }
    }

    if (idx_1 < size_1) {
        const size_t n_elems = size_1 - idx_1;
        memcpy(buffer + pos, set_1 + idx_1, n_elems * sizeof(uint16_t));
        pos += n_elems;
    } else if (idx_2 < size_2) {
        const size_t n_elems = size_2 - idx_2;
        memcpy(buffer + pos, set_2 + idx_2, n_elems * sizeof(uint16_t));
        pos += n_elems;
    }

    return pos;
}

/* Computes the union of array1 and array2 and write the result to arrayout.
 * It is assumed that arrayout is distinct from both array1 and array2.
 */
void array_container_union(const array_container_t *array_1,
                           const array_container_t *array_2,
                           array_container_t *out) {
    const int32_t card_1 = array_1->cardinality, card_2 = array_2->cardinality;
    const int32_t max_cardinality = card_1 + card_2;

    if (out->capacity < max_cardinality)
        array_container_grow(out, max_cardinality, INT32_MAX, false);

    // compute union with smallest array first
    if (card_1 < card_2) {
        out->cardinality = union_uint16(array_1->array, card_1, array_2->array,
                                        card_2, out->array);
    } else {
        out->cardinality = union_uint16(array_2->array, card_2, array_1->array,
                                        card_1, out->array);
    }
}

/* Computes the intersection between one small and one large set of uint16_t.
 * Stores the result into buffer and return the number of elements. */
int32_t intersect_skewed_uint16(const uint16_t *small, size_t size_s,
                                const uint16_t *large, size_t size_l,
                                uint16_t *buffer) {
    size_t pos = 0, idx_l = 0, idx_s = 0;

    if (0 == size_s) {
        return 0;
    }

    uint16_t val_l = large[idx_l], val_s = small[idx_s];

    while (true) {
        if (val_l < val_s) {
            idx_l = advanceUntil(large, idx_l, size_l, val_s);
            if (idx_l == size_l) break;
            val_l = large[idx_l];
        } else if (val_s < val_l) {
            idx_s++;
            if (idx_s == size_s) break;
            val_s = small[idx_s];
        } else {
            buffer[pos++] = val_s;
            idx_s++;
            if (idx_s == size_s) break;
            val_s = small[idx_s];
            idx_l = advanceUntil(large, idx_l, size_l, val_s);
            if (idx_l == size_l) break;
            val_l = large[idx_l];
        }
    }

    return pos;
}


#ifndef USEAVX
/**
 * Generic intersection function. Passes unit tests.
 */
static int32_t intersect_uint16(const uint16_t *A, const size_t lenA,
              const uint16_t *B, const size_t lenB, uint16_t *out) {
    const uint16_t * initout = out;
    if (lenA == 0 || lenB == 0)
        return 0;
    const uint16_t *endA = A + lenA;
    const uint16_t *endB = B + lenB;

    while (1) {
        while (*A < *B) {
SKIP_FIRST_COMPARE:
            if (++A == endA)
                return (out - initout);
        }
        while (*A > *B) {
            if (++B == endB)
                return (out - initout);
        }
        if (*A == *B) {
            *out++ = *A;
            if (++A == endA || ++B == endB)
                return (out - initout);
        } else {
            goto SKIP_FIRST_COMPARE;
        }
    }
    return (out - initout); // NOTREACHED
}
#endif

extern int32_t intersect_vector16(const uint16_t *set_1, size_t size_1,
                                  const uint16_t *set_2, size_t size_2,
                                  uint16_t *out);

#define THRESHOLD 64

static inline int32_t minimum(int32_t a, int32_t b) { return (a < b) ? a : b; }

/* computes the intersection of array1 and array2 and write the result to
 * arrayout.
 * It is assumed that arrayout is distinct from both array1 and array2.
 * */
void array_container_intersection(const array_container_t *array1,
                                  const array_container_t *array2,
                                  array_container_t *out) {
    int32_t card_1 = array1->cardinality, card_2 = array2->cardinality,
            min_card = minimum(card_1, card_2);

    if (out->capacity < min_card)
        array_container_grow(out, min_card, INT32_MAX, false);

    if (card_1 * THRESHOLD < card_2) {
        out->cardinality = intersect_skewed_uint16(
            array1->array, card_1, array2->array, card_2, out->array);
    } else if (card_2 * THRESHOLD < card_1) {
        out->cardinality = intersect_skewed_uint16(
            array2->array, card_2, array1->array, card_1, out->array);
    } else {
#ifdef USEAVX
        out->cardinality = intersect_vector16(
            array1->array, card_1, array2->array, card_2, out->array);
#else
        out->cardinality = intersect_uint16(array1->array, card_1,
                                            array2->array, card_2, out->array);
#endif
    }
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
