/*
An implementation of Roaring Bitmaps in C.
*/

#ifndef ROARING_H
#define ROARING_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <roaring/roaring_array.h>
#include <roaring/roaring_types.h>

typedef struct roaring_bitmap_s {
    roaring_array_t *high_low_container;
    bool copy_on_write; /* copy_on_write: whether you want to use copy-on-write
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
roaring_bitmap_t *roaring_bitmap_from_range(uint32_t min, uint32_t max,
                                            uint32_t step);

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
 * Inplace version modifies x1.  TODO: decide whether x1 == x2 allowed
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
 * Inplace version of roaring_bitmap_or, modifies x1. TDOO: decide whether x1 ==
 *x2 ok
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
 * Computes the symmetric difference (xor) between two bitmaps
 * and returns new bitmap. The caller is responsible for memory management.
 */
roaring_bitmap_t *roaring_bitmap_xor(const roaring_bitmap_t *x1,
                                     const roaring_bitmap_t *x2);

/**
 * Inplace version of roaring_bitmap_xor, modifies x1. x1 != x2.
 *
 */
void roaring_bitmap_xor_inplace(roaring_bitmap_t *x1,
                                const roaring_bitmap_t *x2);

/**
 * Compute the xor of 'number' bitmaps. See also roaring_bitmap_xor_many_heap.
 * Caller is responsible for freeing the
 * result.
 *
 */
roaring_bitmap_t *roaring_bitmap_xor_many(size_t number,
                                          const roaring_bitmap_t **x);

/**
 * Computes the  difference (andnot) between two bitmaps
 * and returns new bitmap. The caller is responsible for memory management.
 */
roaring_bitmap_t *roaring_bitmap_andnot(const roaring_bitmap_t *x1,
                                        const roaring_bitmap_t *x2);

/**
 * Inplace version of roaring_bitmap_andnot, modifies x1. x1 != x2.
 *
 */
void roaring_bitmap_andnot_inplace(roaring_bitmap_t *x1,
                                   const roaring_bitmap_t *x2);

/**
 * Compute the xor of 'number' bitmaps using a heap. This can
 * sometimes be faster than roaring_bitmap_xor_many which uses
 * a naive algorithm. Caller is responsible for freeing the
 * result.
 *
 * TODO: consider implementing
 * roaring_bitmap_t *roaring_bitmap_xor_many_heap(uint32_t number,
 *                                              const roaring_bitmap_t **x);
 */

/**
 * Frees the memory.
 */
void roaring_bitmap_free(roaring_bitmap_t *r);

/**
 * Add value x
 *
 */
void roaring_bitmap_add(roaring_bitmap_t *r, uint32_t x);

/**
 * Remove value x
 *
 */
void roaring_bitmap_remove(roaring_bitmap_t *r, uint32_t x);

/**
 * Check if value x is present
 */
inline bool roaring_bitmap_contains(const roaring_bitmap_t *r,
                                           uint32_t val) {
    const uint16_t hb = val >> 16;
    /*
     * here it is possible to bypass the binary search and the ra_get_index
     * call with the following call that might often come true
     */
    int i;
    if ((r->high_low_container->size > hb) &&
        (r->high_low_container->keys[hb] == hb))
        i = hb;  // we got lucky!
    else {
        // next call involves a binary search (maybe expensive)
        i = ra_get_index(r->high_low_container, hb);
        if (i < 0) return false;
    }
    uint8_t typecode;
    // next call ought to be cheap
    void *container =
        ra_get_container_at_index(r->high_low_container, i, &typecode);
    // rest might be a tad expensive
    return container_contains(container, val & 0xFFFF, typecode);
}

/**
 * Get the cardinality of the bitmap (number of elements).
 */
uint64_t roaring_bitmap_get_cardinality(const roaring_bitmap_t *ra);

/**
* Returns true if the bitmap is empty (cardinality is zero).
*/
bool roaring_bitmap_is_empty(const roaring_bitmap_t *ra);

/**
 * Convert the bitmap to an array. Write the output to "ans",
 * caller is responsible to ensure that there is enough memory
 * allocated
 * (e.g., ans = malloc(roaring_bitmap_get_cardinality(mybitmap)
 *   * sizeof(uint32_t))
 */
void roaring_bitmap_to_uint32_array(const roaring_bitmap_t *ra, uint32_t *ans);

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

//
// write the bitmap to an output pointer, this output buffer should refer to
// at least roaring_bitmap_size_in_bytes(ra) allocated bytes.
//
// see roaring_bitmap_portable_serialize if you want a format that's compatible
// with Java and Go implementations
//
// this format has the benefit of being sometimes more space efficient than roaring_bitmap_portable_serialize
// e.g., when the data is sparse.
//
// Returns how many bytes were written which should be
// roaring_bitmap_size_in_bytes(ra).
size_t roaring_bitmap_serialize(roaring_bitmap_t *ra, char *buf);

//  use with roaring_bitmap_serialize
// see roaring_bitmap_portable_deserialize if you want a format that's
// compatible with Java and Go implementations
roaring_bitmap_t *roaring_bitmap_deserialize(const void *buf);


/**
 * How many bytes are required to serialize this bitmap (NOT compatible
 * with Java and Go versions)
 */
size_t roaring_bitmap_size_in_bytes(const roaring_bitmap_t *ra);


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
 * write a bitmap to a char buffer.  The output buffer should refer to at least
 *  roaring_bitmap_portable_size_in_bytes(ra) bytes of allocated memory.
 * This is meant to be compatible with
 * the
 * Java and Go versions. Returns how many bytes were written which should be
 * roaring_bitmap_portable_size_in_bytes(ra).
 */
size_t roaring_bitmap_portable_serialize(const roaring_bitmap_t *ra, char *buf);

/**
 * Iterate over the bitmap elements. The function iterator is called once for
 *  all the values with ptr (can be NULL) as the second parameter of each call.
 *
 *  roaring_iterator is simply a pointer to a function that returns bool
 *  (true means that the iteration should continue while false means that it
 * should stop),
 *  and takes (uint32_t,void*) as inputs.
 *
 *  Returns true if the roaring_iterator returned true throughout (so that
 *  all data points were necessarily visited).
 */
bool roaring_iterate(const roaring_bitmap_t *ra, roaring_iterator iterator,
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
 * The bitsetconversion conversion is a flag which determines
 * whether container-container operations force a bitset conversion.
 **/
roaring_bitmap_t *roaring_bitmap_lazy_or(const roaring_bitmap_t *x1,
                                         const roaring_bitmap_t *x2,
                                         const bool bitsetconversion);

/**
 * (For expert users who seek high performance.)
 * Inplace version of roaring_bitmap_lazy_or, modifies x1
 * The bitsetconversion conversion is a flag which determines
 * whether container-container operations force a bitset conversion.
 */
void roaring_bitmap_lazy_or_inplace(roaring_bitmap_t *x1,
                                    const roaring_bitmap_t *x2,
                                    const bool bitsetconversion);

/**
 * (For expert users who seek high performance.)
 *
 * Execute maintenance operations on a bitmap created from
 * roaring_bitmap_lazy_or
 * or modified with roaring_bitmap_lazy_or_inplace.
 */
void roaring_bitmap_repair_after_lazy(roaring_bitmap_t *x1);

/**
 * Computes the symmetric difference between two bitmaps and returns new bitmap.
 *The caller is
 * responsible for memory management.
 *
 * The lazy version defers some computations such as the maintenance of the
 * cardinality counts. Thus you need
 * to call roaring_bitmap_repair_after_lazy after executing "lazy" computations.
 * It is safe to repeatedly call roaring_bitmap_lazy_xor_inplace on the result.
 *
 */
roaring_bitmap_t *roaring_bitmap_lazy_xor(const roaring_bitmap_t *x1,
                                          const roaring_bitmap_t *x2);

/**
 * (For expert users who seek high performance.)
 * Inplace version of roaring_bitmap_lazy_xor, modifies x1. x1 != x2
 *
 */
void roaring_bitmap_lazy_xor_inplace(roaring_bitmap_t *x1,
                                     const roaring_bitmap_t *x2);

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

/**
 * If the size of the roaring bitmap is strictly greater than rank, then this
   function returns true and set element to the element of given rank.
   Otherwise, it returns false.
 */
bool roaring_bitmap_select(const roaring_bitmap_t *ra, uint32_t rank,
                           uint32_t *element);

/**
*  (For advanced users.)
* Collect statistics about the bitmap, see roaring_types.h for
* a description of roaring_statistics_t
*/
void roaring_bitmap_statistics(const roaring_bitmap_t *ra,
                               roaring_statistics_t *stat);

#ifdef __cplusplus
}
#endif

#endif
