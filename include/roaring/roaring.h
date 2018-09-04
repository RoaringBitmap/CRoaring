/*
An implementation of Roaring Bitmaps in C.
*/

#ifndef ROARING_H
#define ROARING_H
#ifdef __cplusplus
extern "C" {
#endif

#include <roaring/roaring_array.h>
#include <roaring/roaring_types.h>
#include <roaring/roaring_version.h>
#include <stdbool.h>

typedef struct roaring_bitmap_s {
    roaring_array_t high_low_container;
    bool copy_on_write; /* copy_on_write: whether you want to use copy-on-write
                         (saves memory and avoids
                         copies but needs more care in a threaded context).
                         Most users should ignore this flag.
                         Note: if you do turn this flag to 'true', enabling
                         COW, then ensure that you do so for all of your bitmaps since
                         interactions between bitmaps with and without COW is unsafe. */
} roaring_bitmap_t;

/**
 * Creates a new bitmap (initially empty)
 */
roaring_bitmap_t *roaring_bitmap_create(void);

/**
 * Add all the values between min (included) and max (excluded) that are at a
 * distance k*step from min.
*/
roaring_bitmap_t *roaring_bitmap_from_range(uint64_t min, uint64_t max,
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
 * Copies a  bitmap from src to dest. It is assumed that the pointer dest
 * is to an already allocated bitmap. The content of the dest bitmap is
 * freed/deleted.
 *
 * It might be preferable and simpler to call roaring_bitmap_copy except
 * that roaring_bitmap_overwrite can save on memory allocations.
 *
 */
bool roaring_bitmap_overwrite(roaring_bitmap_t *dest,
                                     const roaring_bitmap_t *src);

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
 * Computes the size of the intersection between two bitmaps.
 *
 */
uint64_t roaring_bitmap_and_cardinality(const roaring_bitmap_t *x1,
                                        const roaring_bitmap_t *x2);


/**
 * Check whether two bitmaps intersect.
 *
 */
bool roaring_bitmap_intersect(const roaring_bitmap_t *x1,
                                     const roaring_bitmap_t *x2);

/**
 * Computes the Jaccard index between two bitmaps. (Also known as the Tanimoto
 * distance,
 * or the Jaccard similarity coefficient)
 *
 * The Jaccard index is undefined if both bitmaps are empty.
 *
 */
double roaring_bitmap_jaccard_index(const roaring_bitmap_t *x1,
                                    const roaring_bitmap_t *x2);

/**
 * Computes the size of the union between two bitmaps.
 *
 */
uint64_t roaring_bitmap_or_cardinality(const roaring_bitmap_t *x1,
                                       const roaring_bitmap_t *x2);

/**
 * Computes the size of the difference (andnot) between two bitmaps.
 *
 */
uint64_t roaring_bitmap_andnot_cardinality(const roaring_bitmap_t *x1,
                                           const roaring_bitmap_t *x2);

/**
 * Computes the size of the symmetric difference (andnot) between two bitmaps.
 *
 */
uint64_t roaring_bitmap_xor_cardinality(const roaring_bitmap_t *x1,
                                        const roaring_bitmap_t *x2);

/**
 * Inplace version modifies x1, x1 == x2 is allowed
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
 * Compute the xor of 'number' bitmaps.
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
 * TODO: consider implementing:
 * Compute the xor of 'number' bitmaps using a heap. This can
 * sometimes be faster than roaring_bitmap_xor_many which uses
 * a naive algorithm. Caller is responsible for freeing the
 * result.
 *
 * roaring_bitmap_t *roaring_bitmap_xor_many_heap(uint32_t number,
 *                                              const roaring_bitmap_t **x);
 */

/**
 * Frees the memory.
 */
void roaring_bitmap_free(roaring_bitmap_t *r);

/**
 * Add value n_args from pointer vals, faster than repeatedly calling
 * roaring_bitmap_add
 *
 */
void roaring_bitmap_add_many(roaring_bitmap_t *r, size_t n_args,
                             const uint32_t *vals);

/**
 * Add value x
 *
 */
void roaring_bitmap_add(roaring_bitmap_t *r, uint32_t x);

/**
 * Add value x
 * Returns true if a new value was added, false if the value was already existing.
 */
bool roaring_bitmap_add_checked(roaring_bitmap_t *r, uint32_t x);

/**
 * Add all values in range [min, max]
 */
void roaring_bitmap_add_range_closed(roaring_bitmap_t *ra, uint32_t min, uint32_t max);

/**
 * Add all values in range [min, max)
 */
inline void roaring_bitmap_add_range(roaring_bitmap_t *ra, uint64_t min, uint64_t max) {
  if(max == min) return;
  roaring_bitmap_add_range_closed(ra, (uint32_t)min, (uint32_t)(max - 1));
}

/**
 * Remove value x
 *
 */
void roaring_bitmap_remove(roaring_bitmap_t *r, uint32_t x);

/** Remove all values in range [min, max] */
void roaring_bitmap_remove_range_closed(roaring_bitmap_t *ra, uint32_t min, uint32_t max);

/** Remove all values in range [min, max) */
inline void roaring_bitmap_remove_range(roaring_bitmap_t *ra, uint64_t min, uint64_t max) {
    if(max == min) return;
    roaring_bitmap_remove_range_closed(ra, (uint32_t)min, (uint32_t)(max - 1));
}

/** Remove multiple values */
void roaring_bitmap_remove_many(roaring_bitmap_t *r, size_t n_args,
                                const uint32_t *vals);

/**
 * Remove value x
 * Returns true if a new value was removed, false if the value was not existing.
 */
bool roaring_bitmap_remove_checked(roaring_bitmap_t *r, uint32_t x);

/**
 * Check if value x is present
 */
inline bool roaring_bitmap_contains(const roaring_bitmap_t *r, uint32_t val) {
    const uint16_t hb = val >> 16;
    /*
     * the next function call involves a binary search and lots of branching.
     */
    int32_t i = ra_get_index(&r->high_low_container, hb);
    if (i < 0) return false;

    uint8_t typecode;
    // next call ought to be cheap
    void *container =
        ra_get_container_at_index(&r->high_low_container, i, &typecode);
    // rest might be a tad expensive, possibly involving another round of binary search
    return container_contains(container, val & 0xFFFF, typecode);
}

/**
 * Check whether a range of values from range_start (included) to range_end (excluded) is present
 */
bool roaring_bitmap_contains_range(const roaring_bitmap_t *r, uint64_t range_start, uint64_t range_end);

/**
 * Get the cardinality of the bitmap (number of elements).
 */
uint64_t roaring_bitmap_get_cardinality(const roaring_bitmap_t *ra);

/**
* Returns true if the bitmap is empty (cardinality is zero).
*/
bool roaring_bitmap_is_empty(const roaring_bitmap_t *ra);


/**
* Empties the bitmap
*/
void roaring_bitmap_clear(roaring_bitmap_t *ra);

/**
 * Convert the bitmap to an array. Write the output to "ans",
 * caller is responsible to ensure that there is enough memory
 * allocated
 * (e.g., ans = malloc(roaring_bitmap_get_cardinality(mybitmap)
 *   * sizeof(uint32_t))
 */
void roaring_bitmap_to_uint32_array(const roaring_bitmap_t *ra, uint32_t *ans);


/**
 * Convert the bitmap to an array from "offset" by "limit". Write the output to "ans".
 * so, you can get data in paging.
 * caller is responsible to ensure that there is enough memory
 * allocated
 * (e.g., ans = malloc(roaring_bitmap_get_cardinality(limit)
 *   * sizeof(uint32_t))
 * Return false in case of failure (e.g., insufficient memory)
 */
bool roaring_bitmap_range_uint32_array(const roaring_bitmap_t *ra, size_t offset, size_t limit, uint32_t *ans);

/**
 *  Remove run-length encoding even when it is more space efficient
 *  return whether a change was applied
 */
bool roaring_bitmap_remove_run_compression(roaring_bitmap_t *r);

/** convert array and bitmap containers to run containers when it is more
 * efficient;
 * also convert from run containers when more space efficient.  Returns
 * true if the result has at least one run container.
 * Additional savings might be possible by calling shrinkToFit().
 */
bool roaring_bitmap_run_optimize(roaring_bitmap_t *r);

/**
 * If needed, reallocate memory to shrink the memory usage. Returns
 * the number of bytes saved.
*/
size_t roaring_bitmap_shrink_to_fit(roaring_bitmap_t *r);

/**
* write the bitmap to an output pointer, this output buffer should refer to
* at least roaring_bitmap_size_in_bytes(ra) allocated bytes.
*
* see roaring_bitmap_portable_serialize if you want a format that's compatible
* with Java and Go implementations
*
* this format has the benefit of being sometimes more space efficient than
* roaring_bitmap_portable_serialize
* e.g., when the data is sparse.
*
* Returns how many bytes were written which should be
* roaring_bitmap_size_in_bytes(ra).
*/
size_t roaring_bitmap_serialize(const roaring_bitmap_t *ra, char *buf);

/**  use with roaring_bitmap_serialize
* see roaring_bitmap_portable_deserialize if you want a format that's
* compatible with Java and Go implementations
*/
roaring_bitmap_t *roaring_bitmap_deserialize(const void *buf);

/**
 * How many bytes are required to serialize this bitmap (NOT compatible
 * with Java and Go versions)
 */
size_t roaring_bitmap_size_in_bytes(const roaring_bitmap_t *ra);

/**
 * read a bitmap from a serialized version. This is meant to be compatible with
 * the Java and Go versions. See format specification at
 * https://github.com/RoaringBitmap/RoaringFormatSpec
 * In case of failure, a null pointer is returned.
 * This function is unsafe in the sense that if there is no valid serialized
 * bitmap at the pointer, then many bytes could be read, possibly causing a buffer
 * overflow. For a safer approach,
 * call roaring_bitmap_portable_deserialize_safe.
 */
roaring_bitmap_t *roaring_bitmap_portable_deserialize(const char *buf);

/**
 * read a bitmap from a serialized version in a safe manner (reading up to maxbytes).
 * This is meant to be compatible with
 * the Java and Go versions. See format specification at
 * https://github.com/RoaringBitmap/RoaringFormatSpec
 * In case of failure, a null pointer is returned.
 */
roaring_bitmap_t *roaring_bitmap_portable_deserialize_safe(const char *buf, size_t maxbytes);

/**
 * Check how many bytes would be read (up to maxbytes) at this pointer if there
 * is a bitmap, returns zero if there is no valid bitmap.
 * This is meant to be compatible with
 * the Java and Go versions. See format specification at
 * https://github.com/RoaringBitmap/RoaringFormatSpec
 */
size_t roaring_bitmap_portable_deserialize_size(const char *buf, size_t maxbytes);


/**
 * How many bytes are required to serialize this bitmap (meant to be compatible
 * with Java and Go versions).  See format specification at
 * https://github.com/RoaringBitmap/RoaringFormatSpec
 */
size_t roaring_bitmap_portable_size_in_bytes(const roaring_bitmap_t *ra);

/**
 * write a bitmap to a char buffer.  The output buffer should refer to at least
 *  roaring_bitmap_portable_size_in_bytes(ra) bytes of allocated memory.
 * This is meant to be compatible with
 * the
 * Java and Go versions. Returns how many bytes were written which should be
 * roaring_bitmap_portable_size_in_bytes(ra).  See format specification at
 * https://github.com/RoaringBitmap/RoaringFormatSpec
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

bool roaring_iterate64(const roaring_bitmap_t *ra, roaring_iterator64 iterator,
                       uint64_t high_bits, void *ptr);

/**
 * Return true if the two bitmaps contain the same elements.
 */
bool roaring_bitmap_equals(const roaring_bitmap_t *ra1,
                           const roaring_bitmap_t *ra2);

/**
 * Return true if all the elements of ra1 are also in ra2.
 */
bool roaring_bitmap_is_subset(const roaring_bitmap_t *ra1,
                              const roaring_bitmap_t *ra2);

/**
 * Return true if all the elements of ra1 are also in ra2 and ra2 is strictly
 * greater
 * than ra1.
 */
bool roaring_bitmap_is_strict_subset(const roaring_bitmap_t *ra1,
                                            const roaring_bitmap_t *ra2);

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
 * compute the negation of the roaring bitmap within a specified
 * interval: [range_start, range_end). The number of negated values is
 * range_end - range_start.
 * Areas outside the range are passed through unchanged.
 */

roaring_bitmap_t *roaring_bitmap_flip(const roaring_bitmap_t *x1,
                                      uint64_t range_start, uint64_t range_end);

/**
 * compute (in place) the negation of the roaring bitmap within a specified
 * interval: [range_start, range_end). The number of negated values is
 * range_end - range_start.
 * Areas outside the range are passed through unchanged.
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
* roaring_bitmap_rank returns the number of integers that are smaller or equal
* to x.
*/
uint64_t roaring_bitmap_rank(const roaring_bitmap_t *bm, uint32_t x);

/**
* roaring_bitmap_smallest returns the smallest value in the set.
* Returns UINT32_MAX if the set is empty.
*/
uint32_t roaring_bitmap_minimum(const roaring_bitmap_t *bm);

/**
* roaring_bitmap_smallest returns the greatest value in the set.
* Returns 0 if the set is empty.
*/
uint32_t roaring_bitmap_maximum(const roaring_bitmap_t *bm);

/**
*  (For advanced users.)
* Collect statistics about the bitmap, see roaring_types.h for
* a description of roaring_statistics_t
*/
void roaring_bitmap_statistics(const roaring_bitmap_t *ra,
                               roaring_statistics_t *stat);

/*********************
* What follows is code use to iterate through values in a roaring bitmap

roaring_bitmap_t *ra =...
roaring_uint32_iterator_t   i;
roaring_create_iterator(ra, &i);
while(i.has_value) {
  printf("value = %d\n", i.current_value);
  roaring_advance_uint32_iterator(&i);
}

Obviously, if you modify the underlying bitmap, the iterator
becomes invalid. So don't.
*/

typedef struct roaring_uint32_iterator_s {
    const roaring_bitmap_t *parent;  // owner
    int32_t container_index;         // point to the current container index
    int32_t in_container_index;  // for bitset and array container, this is out
                                 // index
    int32_t run_index;           // for run container, this points  at the run
    uint32_t in_run_index;  // within a run, this is our index (points at the
                            // end of the current run)

    uint32_t current_value;
    bool has_value;

    const void
        *container;  // should be:
                     // parent->high_low_container.containers[container_index];
    uint8_t typecode;  // should be:
                       // parent->high_low_container.typecodes[container_index];
    uint32_t highbits;  // should be:
                        // parent->high_low_container.keys[container_index]) <<
                        // 16;

} roaring_uint32_iterator_t;

/**
* Initialize an iterator object that can be used to iterate through the
* values.  If there is a  value, then it->has_value is true.
* The first value is in it->current_value. The iterator traverses the values
* in increasing order.
*/
void roaring_init_iterator(const roaring_bitmap_t *ra,
                           roaring_uint32_iterator_t *newit);

/**
* Create an iterator object that can be used to iterate through the
* values. Caller is responsible for calling roaring_free_iterator.
* The iterator is initialized. If there is a  value, then it->has_value is true.
* The first value is in it->current_value. The iterator traverses the values
* in increasing order.
*
* This function calls roaring_init_iterator.
*/
roaring_uint32_iterator_t *roaring_create_iterator(const roaring_bitmap_t *ra);

/**
* Advance the iterator. If there is a new value, then it->has_value is true.
* The new value is in it->current_value. Values are traversed in increasing
* orders. For convenience, returns it->has_value.
*/
bool roaring_advance_uint32_iterator(roaring_uint32_iterator_t *it);

/**
* Move the iterator to the first value >= val. If there is a such a value, then it->has_value is true.
* The new value is in it->current_value. For convenience, returns it->has_value.
*/
bool roaring_move_uint32_iterator_equalorlarger(roaring_uint32_iterator_t *it, uint32_t val) ;
/**
* Creates a copy of an iterator.
* Caller must free it.
*/
roaring_uint32_iterator_t *roaring_copy_uint32_iterator(
    const roaring_uint32_iterator_t *it);

/**
* Free memory following roaring_create_iterator
*/
void roaring_free_uint32_iterator(roaring_uint32_iterator_t *it);

/*
 * Reads next ${count} values from iterator into user-supplied ${buf}.
 * Returns the number of read elements.
 * This number can be smaller than ${count}, which means that iterator is drained.
 *
 * This function satisfies semantics of iteration and can be used together with
 * other iterator functions.
 *  - first value is copied from ${it}->current_value
 *  - after function returns, iterator is positioned at the next element
 */
uint32_t roaring_read_uint32_iterator(roaring_uint32_iterator_t *it, uint32_t* buf, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif
