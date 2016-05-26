/*
An implementation of Roaring Bitmaps in C.
*/

#ifndef ROARING_H
#define ROARING_H

#include <stdbool.h>
#include "roaring_array.h"
#include "roaring_types.h"

typedef struct roaring_bitmap_s {
    roaring_array_t *high_low_container;
    bool copy_on_write;  /* copy_on_write: whether you want to use copy-on-write
                          (saves memory and avoids
                          copies but needs more care in a threaded context). */
} roaring_bitmap_t;

/**
 * Creates a new bitmap (initially empty)
 */
roaring_bitmap_t *roaring_bitmap_create(void);

/**
 * Add all the values between min (included) and max (excluded) that are at a
 * distance k*step from min.
*/
roaring_bitmap_t *roaring_bitmap_from_range(uint32_t min, uint32_t max, uint32_t step);

/**
 * Creates a new bitmap (initially empty) with a provided
 * container-storage capacity (it is a performance hint).
 */
roaring_bitmap_t *roaring_bitmap_create_with_capacity(uint32_t cap);

/**
 * Creates a new bitmap from a pointer of uint32_t integers
 */
roaring_bitmap_t *roaring_bitmap_of_ptr(size_t n_args, const uint32_t *vals);

/**
 * Describe the inner structure of the bitmap.
 */
void roaring_bitmap_printf_describe(const roaring_bitmap_t *ra);

/**
 * Creates a new bitmap from a list of uint32_t integers
 */
roaring_bitmap_t *roaring_bitmap_of(size_t n, ...);

/**
 * Copies a  bitmap. This does memory allocation. The caller is responsible for
 * memory management.
 *
 */
roaring_bitmap_t *roaring_bitmap_copy(const roaring_bitmap_t *r);

/**
 * Print the content of the bitmap.
 */
void roaring_bitmap_printf(const roaring_bitmap_t *ra);

/**
 * Computes the intersection between two bitmaps and returns new bitmap. The
 * caller is
 * responsible for memory management.
 *
 */
roaring_bitmap_t *roaring_bitmap_and(const roaring_bitmap_t *x1,
                                     const roaring_bitmap_t *x2);

/**
 * Inplace version modifies x1
 */
void roaring_bitmap_and_inplace(roaring_bitmap_t *x1,
                                const roaring_bitmap_t *x2);

/**
 * Computes the union between two bitmaps and returns new bitmap. The caller is
 * responsible for memory management.
 */
roaring_bitmap_t *roaring_bitmap_or(const roaring_bitmap_t *x1,
                                    const roaring_bitmap_t *x2);

/**
 * Inplace version of roaring_bitmap_or, modifies x1
 *
 */
void roaring_bitmap_or_inplace(roaring_bitmap_t *x1,
                               const roaring_bitmap_t *x2);


/**
 * Compute the union of 'number' bitmaps. See also roaring_bitmap_or_many_heap.
 * Caller is responsible for freeing the
 * result.
 *
 */
roaring_bitmap_t *roaring_bitmap_or_many(size_t number,
                                         const roaring_bitmap_t **x);

/**
 * Compute the union of 'number' bitmaps using a heap. This can
 * sometimes be faster than roaring_bitmap_or_many which uses
 * a naive algorithm. Caller is responsible for freeing the
 * result.
 *
 */
roaring_bitmap_t *roaring_bitmap_or_many_heap(uint32_t number,
                                              const roaring_bitmap_t **x);


/**
 * Frees the memory.
 */
void roaring_bitmap_free(roaring_bitmap_t *r);

/**
 * Add value x
 *
 * TODO: add a way to remove values
 */
void roaring_bitmap_add(roaring_bitmap_t *r, uint32_t x);

/**
 * Check if value x is present
 */
bool roaring_bitmap_contains(const roaring_bitmap_t *r, uint32_t x);

/**
 * Get the cardinality of the bitmap (number elements).
 */
uint64_t roaring_bitmap_get_cardinality(const roaring_bitmap_t *ra);

/**
 * Convert the bitmap to an array. Array is allocated and caller is responsible
 * for eventually freeing it.
 */
uint32_t *roaring_bitmap_to_uint32_array(const roaring_bitmap_t *ra,
                                         uint32_t *cardinality);
#endif

/**
 *  Remove run-length encoding even when it is more space efficient
 *  return whether a change was applied
 */
bool roaring_bitmap_remove_run_compression(roaring_bitmap_t *r);

/** convert array and bitmap containers to run containers when it is more
 * efficient;
 * also convert from run containers when more space efficient.  Returns
 * true if the result has at least one run container.
*/
bool roaring_bitmap_run_optimize(roaring_bitmap_t *r);

// see roaring_bitmap_portable_serialize if you want a format that's compatible
// with Java and Go implementations
char *roaring_bitmap_serialize(roaring_bitmap_t *ra, uint32_t *serialize_len);

// see roaring_bitmap_portable_deserialize if you want a format that's
// compatible with Java and Go implementations
roaring_bitmap_t *roaring_bitmap_deserialize(const void *buf, uint32_t buf_len);

/**
 * read a bitmap from a serialized version. This is meant to be compatible with
 * the
 * Java and Go versions.
 */
roaring_bitmap_t *roaring_bitmap_portable_deserialize(const char *buf);

/**
 * How many bytes are required to serialize this bitmap (meant to be compatible
 * with Java and Go versions)
 */
size_t roaring_bitmap_portable_size_in_bytes(const roaring_bitmap_t *ra);

/**
 * write a bitmap to a char buffer. This is meant to be compatible with
 * the
 * Java and Go versions. Returns how many bytes were written which should be
 * roaring_bitmap_portable_size_in_bytes(ra).
 */
size_t roaring_bitmap_portable_serialize(const roaring_bitmap_t *ra, char *buf);

/**
 * Iterate over the bitmap elements. The function iterator is called once for
 *  all the values with ptr (can be NULL) as the second parameter of each call.
 *
 *  roaring_iterator is simply a pointer to a function that returns void,
 *  and takes (uint32_t,void*) as inputs.
 */
void roaring_iterate(roaring_bitmap_t *ra, roaring_iterator iterator,
                     void *ptr);

/**
 * Return true if the two bitmaps contain the same elements.
 */
bool roaring_bitmap_equals(roaring_bitmap_t *ra1, roaring_bitmap_t *ra2);

/**
 * (For expert users who seek high performance.)
 *
 * Computes the union between two bitmaps and returns new bitmap. The caller is
 * responsible for memory management.
 *
 * The lazy version defers some computations such as the maintenance of the
 * cardinality counts. Thus you need
 * to call roaring_bitmap_repair_after_lazy after executing "lazy" computations.
 * It is safe to repeatedly call roaring_bitmap_lazy_or_inplace on the result.
 *
 */
roaring_bitmap_t *roaring_bitmap_lazy_or(const roaring_bitmap_t *x1,
                                         const roaring_bitmap_t *x2);

/**
 * (For expert users who seek high performance.)
 * Inplace version of roaring_bitmap_lazy_or, modifies x1
 *
 */
void roaring_bitmap_lazy_or_inplace(roaring_bitmap_t *x1,
                                    const roaring_bitmap_t *x2);

/**
 * (For expert users who seek high performance.)
 *
 * Execute maintenance operations on a bitmap created from
 * roaring_bitmap_lazy_or
 * or modified with roaring_bitmap_lazy_or_inplace.
 */
void roaring_bitmap_repair_after_lazy(roaring_bitmap_t *x1);

/**
 * compute the negation of the roaring bitmap within a specified interval.
 * areas outside the range are passed through unchanged.
 */

roaring_bitmap_t *roaring_bitmap_flip(const roaring_bitmap_t *x1,
                                      uint64_t range_start, uint64_t range_end);

/**
 * compute (in place) the negation of the roaring bitmap within a specified
 * interval.
 * areas outside the range are passed through unchanged.
 */

void roaring_bitmap_flip_inplace(roaring_bitmap_t *x1, uint64_t range_start,
                                 uint64_t range_end);
