/*
An implementation of Roaring Bitmaps in C.
*/

#ifndef ROARING_H
#define ROARING_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>  // for `size_t`

#include <roaring/roaring_types.h>
#include <roaring/roaring_version.h>

#ifdef __cplusplus
extern "C" { namespace roaring { namespace api {
#endif

typedef struct roaring_bitmap_s {
    roaring_array_t high_low_container;
} roaring_bitmap_t;

/**
 * Dynamically allocates a new bitmap (initially empty).
 * Returns NULL if the allocation fails.
 * Capacity is a performance hint for how many "containers" the data will need.
 * Client is responsible for calling `roaring_bitmap_free()`.
 */
roaring_bitmap_t *roaring_bitmap_create_with_capacity(uint32_t cap);

/**
 * Dynamically allocates a new bitmap (initially empty).
 * Returns NULL if the allocation fails.
 * Client is responsible for calling `roaring_bitmap_free()`.
 */
static inline roaring_bitmap_t *roaring_bitmap_create(void)
  { return roaring_bitmap_create_with_capacity(0); }

/**
 * Initialize a roaring bitmap structure in memory controlled by client.
 * Capacity is a performance hint for how many "containers" the data will need.
 * Can return false if auxiliary allocations fail when capacity greater than 0.
 */
bool roaring_bitmap_init_with_capacity(roaring_bitmap_t *r, uint32_t cap);

/**
 * Initialize a roaring bitmap structure in memory controlled by client.
 * The bitmap will be in a "clear" state, with no auxiliary allocations.
 * Since this performs no allocations, the function will not fail.
 */
static inline void roaring_bitmap_init_cleared(roaring_bitmap_t *r)
  { roaring_bitmap_init_with_capacity(r, 0); }

/**
 * Add all the values between min (included) and max (excluded) that are at a
 * distance k*step from min.
*/
roaring_bitmap_t *roaring_bitmap_from_range(uint64_t min, uint64_t max,
                                            uint32_t step);

/**
 * Creates a new bitmap from a pointer of uint32_t integers
 */
roaring_bitmap_t *roaring_bitmap_of_ptr(size_t n_args, const uint32_t *vals);

/*
 * Whether you want to use copy-on-write.
 * Saves memory and avoids copies but needs more care in a threaded context.
 * Most users should ignore this flag.
 * Note: if you do turn this flag to 'true', enabling COW,
 * then ensure that you do so for all of your bitmaps since
 * interactions between bitmaps with and without COW is unsafe.
 */
static inline bool roaring_bitmap_get_copy_on_write(const roaring_bitmap_t* r) {
    return r->high_low_container.flags & ROARING_FLAG_COW;
}
static inline void roaring_bitmap_set_copy_on_write(roaring_bitmap_t* r, bool cow) {
    if (cow) {
        r->high_low_container.flags |= ROARING_FLAG_COW;
    } else {
        r->high_low_container.flags &= ~ROARING_FLAG_COW;
    }
}

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
void roaring_bitmap_free(const roaring_bitmap_t *r);

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
static inline void roaring_bitmap_add_range(roaring_bitmap_t *ra, uint64_t min, uint64_t max) {
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
static inline void roaring_bitmap_remove_range(roaring_bitmap_t *ra, uint64_t min, uint64_t max) {
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
 * Check if value is present
 */
bool roaring_bitmap_contains(const roaring_bitmap_t *r, uint32_t val);

/**
 * Check whether a range of values from range_start (included) to range_end (excluded) is present
 */
bool roaring_bitmap_contains_range(const roaring_bitmap_t *r, uint64_t range_start, uint64_t range_end);

/**
 * Get the cardinality of the bitmap (number of elements).
 */
uint64_t roaring_bitmap_get_cardinality(const roaring_bitmap_t *ra);

/**
 * Returns the number of elements in the range [range_start, range_end).
 */
uint64_t roaring_bitmap_range_cardinality(const roaring_bitmap_t *ra,
                                          uint64_t range_start, uint64_t range_end);

/**
* Returns true if the bitmap is empty (cardinality is zero).
*/
bool roaring_bitmap_is_empty(const roaring_bitmap_t *ra);


/**
 * Empties the bitmap.  It will have no auxiliary allocations (so if the bitmap
 * was initialized in client memory via roaring_bitmap_init(), then a call to
 * roaring_bitmap_clear() would be enough to "free" it) 
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

/*
 * "Frozen" serialization format imitates memory layout of roaring_bitmap_t.
 * Deserialized bitmap is a constant view of the underlying buffer.
 * This significantly reduces amount of allocations and copying required during
 * deserialization.
 * It can be used with memory mapped files.
 * Example can be found in benchmarks/frozen_benchmark.c
 *
 *         [#####] const roaring_bitmap_t *
 *          | | |
 *     +----+ | +-+
 *     |      |   |
 * [#####################################] underlying buffer
 *
 * Note that because frozen serialization format imitates C memory layout
 * of roaring_bitmap_t, it is not fixed. It is different on big/little endian
 * platforms and can be changed in future.
 */

/**
 * Returns number of bytes required to serialize bitmap using frozen format.
 */
size_t roaring_bitmap_frozen_size_in_bytes(const roaring_bitmap_t *ra);

/**
 * Serializes bitmap using frozen format.
 * Buffer size must be at least roaring_bitmap_frozen_size_in_bytes().
 */
void roaring_bitmap_frozen_serialize(const roaring_bitmap_t *ra, char *buf);

/**
 * Creates constant bitmap that is a view of a given buffer.
 * Buffer must contain data previously written by roaring_bitmap_frozen_serialize(),
 * and additionally its beginning must be aligned by 32 bytes.
 * Length must be equal exactly to roaring_bitmap_frozen_size_in_bytes().
 *
 * On error, NULL is returned.
 *
 * Bitmap returned by this function can be used in all readonly contexts.
 * Bitmap must be freed as usual, by calling roaring_bitmap_free().
 * Underlying buffer must not be freed or modified while it backs any bitmaps.
 */
const roaring_bitmap_t *roaring_bitmap_frozen_view(const char *buf, size_t length);


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
 * Selects the element at index 'rank' where the smallest element is at index 0.
 * If the size of the roaring bitmap is strictly greater than rank, then this
   function returns true and sets element to the element of given rank.
   Otherwise, it returns false.
 */
bool roaring_bitmap_select(const roaring_bitmap_t *ra, uint32_t rank,
                           uint32_t *element);
/**
* roaring_bitmap_rank returns the number of integers that are smaller or equal
* to x. Thus if x is the first element, this function will return 1. If
* x is smaller than the smallest element, this function will return 0.
*
* The indexing convention differs between roaring_bitmap_select and
* roaring_bitmap_rank: roaring_bitmap_select refers to the smallest value
* as having index 0, whereas roaring_bitmap_rank returns 1 when ranking
* the smallest value.
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

    uint32_t current_value;
    bool has_value;

    const ROARING_CONTAINER_T
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
* values. If there is a  value, then this iterator points to the first value
* and it->has_value is true. The value is in it->current_value.
*/
void roaring_init_iterator(const roaring_bitmap_t *ra,
                           roaring_uint32_iterator_t *newit);

/**
* Initialize an iterator object that can be used to iterate through the
* values. If there is a value, then this iterator points to the last value
* and it->has_value is true. The value is in it->current_value.
*/
void roaring_init_iterator_last(const roaring_bitmap_t *ra,
                                roaring_uint32_iterator_t *newit);

/**
* Create an iterator object that can be used to iterate through the
* values. Caller is responsible for calling roaring_free_iterator.
* The iterator is initialized. If there is a  value, then this iterator
* points to the first value and it->has_value is true.
* The value is in it->current_value.
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
* Decrement the iterator. If there is a new value, then it->has_value is true.
* The new value is in it->current_value. Values are traversed in decreasing
* orders. For convenience, returns it->has_value.
*/
bool roaring_previous_uint32_iterator(roaring_uint32_iterator_t *it);

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
} } }  // extern "C" { namespace roaring { namespace api {
#endif

#endif  /* ROARING_H */

#ifdef __cplusplus
    /**
     * Best practices for C++ headers is to avoid polluting global scope.
     * But for C compatibility when just `roaring.h` is included building as
     * C++, default to global access for the C public API.
     *
     * BUT when `roaring.hh` is included instead, it sets this flag.  That way
     * explicit namespacing must be used to get the C functions.
     *
     * This is outside the include guard so that if you include BOTH headers, 
     * the order won't matter; you still get the global definitions.
     */
    #if !defined(ROARING_API_NOT_IN_GLOBAL_NAMESPACE)
        using namespace ::roaring::api;
    #endif
#endif

