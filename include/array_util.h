#ifndef ARRAY_UTIL_H
#define ARRAY_UTIL_H

#include "stdint.h"

int32_t binarySearch(const uint16_t *source, int32_t n, uint16_t target);

int32_t advanceUntil(const uint16_t *array, int32_t pos, int32_t length,
                     uint16_t min);
/**
 * From Schlegel et al., Fast Sorted-Set Intersection using SIMD Instructions
 * Optimized by D. Lemire on May 3rd 2013
 */
int32_t intersect_vector16(const uint16_t *A, size_t s_a, const uint16_t *B,
                           size_t s_b, uint16_t *C);

/* Computes the intersection between one small and one large set of uint16_t.
 * Stores the result into buffer and return the number of elements. */
int32_t intersect_skewed_uint16(const uint16_t *small, size_t size_s,
                                const uint16_t *large, size_t size_l,
                                uint16_t *buffer);


/**
 * Generic intersection function. Passes unit tests.
 */
int32_t intersect_uint16(const uint16_t *A, const size_t lenA,
                                const uint16_t *B, const size_t lenB,
                                uint16_t *out);


/**
 * Generic union function.
 */
size_t union_uint16(const uint16_t *set_1, size_t size_1,
                           const uint16_t *set_2, size_t size_2,
                           uint16_t *buffer);
#endif
