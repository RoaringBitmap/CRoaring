#ifndef ROARING64_H
#define ROARING64_H

#include <roaring/memory.h>
#include <roaring/roaring_types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
namespace roaring {
namespace api {
#endif

typedef struct roaring64_bitmap_s roaring64_bitmap_t;
typedef struct roaring64_leaf_s roaring64_leaf_t;

/**
 * A bit of context usable with `roaring64_bitmap_*_bulk()` functions.
 *
 * Should be initialized with `{0}` (or `memset()` to all zeros).
 * Callers should treat it as an opaque type.
 *
 * A context may only be used with a single bitmap (unless re-initialized to
 * zero), and any modification to a bitmap (other than modifications performed
 * with `_bulk()` functions with the context passed) will invalidate any
 * contexts associated with that bitmap.
 */
typedef struct roaring64_bulk_context_s {
    uint8_t high_bytes[6];
    roaring64_leaf_t *leaf;
} roaring64_bulk_context_t;

/**
 * Dynamically allocates a new bitmap (initially empty).
 * Client is responsible for calling `roaring64_bitmap_free()`.
 */
roaring64_bitmap_t *roaring64_bitmap_create(void);
void roaring64_bitmap_free(roaring64_bitmap_t *r);

/**
 * Returns a copy of a bitmap.
 */
roaring64_bitmap_t *roaring64_bitmap_copy(const roaring64_bitmap_t *r);

/**
 * Creates a new bitmap of a pointer to N 64-bit integers.
 */
roaring64_bitmap_t *roaring64_bitmap_of_ptr(size_t n_args,
                                            const uint64_t *vals);

/**
 * Creates a new bitmap of a pointer to N 64-bit integers.
 */
roaring64_bitmap_t *roaring64_bitmap_of(size_t n_args, ...);

/**
 * Create a new bitmap containing all the values in [min, max) that are at a
 * distance k*step from min.
 */
roaring64_bitmap_t *roaring64_bitmap_from_range(uint64_t min, uint64_t max,
                                                uint64_t step);

/**
 * Adds the provided value to the bitmap.
 */
void roaring64_bitmap_add(roaring64_bitmap_t *r, uint64_t val);

/**
 * Adds the provided value to the bitmap.
 * Returns true if a new value was added, false if the value already existed.
 */
bool roaring64_bitmap_add_checked(roaring64_bitmap_t *r, uint64_t val);

/**
 * Add an item, using context from a previous insert for faster insertion.
 *
 * `context` will be used to store information between calls to make bulk
 * operations faster. `*context` should be zero-initialized before the first
 * call to this function.
 *
 * Modifying the bitmap in any way (other than `-bulk` suffixed functions)
 * will invalidate the stored context, calling this function with a non-zero
 * context after doing any modification invokes undefined behavior.
 *
 * In order to exploit this optimization, the caller should call this function
 * with values with the same high 48 bits of the value consecutively.
 */
void roaring64_bitmap_add_bulk(roaring64_bitmap_t *r,
                               roaring64_bulk_context_t *context, uint64_t val);

/**
 * Add `n_args` values from `vals`, faster than repeatedly calling
 * `roaring64_bitmap_add()`
 *
 * In order to exploit this optimization, the caller should attempt to keep
 * values with the same high 48 bits of the value as consecutive elements in
 * `vals`.
 */
void roaring64_bitmap_add_many(roaring64_bitmap_t *r, size_t n_args,
                               const uint64_t *vals);

/**
 * Add all values in range [min, max].
 */
void roaring64_bitmap_add_range_closed(roaring64_bitmap_t *r, uint64_t min,
                                       uint64_t max);

/**
 * Removes a value from the bitmap if present.
 */
void roaring64_bitmap_remove(roaring64_bitmap_t *r, uint64_t val);

/**
 * Removes a value from the bitmap if present, returns true if the value was
 * removed and false if the value was not present.
 */
bool roaring64_bitmap_remove_checked(roaring64_bitmap_t *r, uint64_t val);

/**
 * Remove an item, using context from a previous insert for faster removal.
 *
 * `context` will be used to store information between calls to make bulk
 * operations faster. `*context` should be zero-initialized before the first
 * call to this function.
 *
 * Modifying the bitmap in any way (other than `-bulk` suffixed functions)
 * will invalidate the stored context, calling this function with a non-zero
 * context after doing any modification invokes undefined behavior.
 *
 * In order to exploit this optimization, the caller should call this function
 * with values with the same high 48 bits of the value consecutively.
 */
void roaring64_bitmap_remove_bulk(roaring64_bitmap_t *r,
                                  roaring64_bulk_context_t *context,
                                  uint64_t val);

/**
 * Remove `n_args` values from `vals`, faster than repeatedly calling
 * `roaring64_bitmap_remove()`
 *
 * In order to exploit this optimization, the caller should attempt to keep
 * values with the same high 48 bits of the value as consecutive elements in
 * `vals`.
 */
void roaring64_bitmap_remove_many(roaring64_bitmap_t *r, size_t n_args,
                                  const uint64_t *vals);

/**
 * Remove all values in range [min, max].
 */
void roaring64_bitmap_remove_range_closed(roaring64_bitmap_t *r, uint64_t min,
                                          uint64_t max);

/**
 * Returns true if the provided value is present.
 */
bool roaring64_bitmap_contains(const roaring64_bitmap_t *r, uint64_t val);

/**
 * Check if an item is present using context from a previous insert or search
 * for faster search.
 *
 * `context` will be used to store information between calls to make bulk
 * operations faster. `*context` should be zero-initialized before the first
 * call to this function.
 *
 * Modifying the bitmap in any way (other than `-bulk` suffixed functions)
 * will invalidate the stored context, calling this function with a non-zero
 * context after doing any modification invokes undefined behavior.
 *
 * In order to exploit this optimization, the caller should call this function
 * with values with the same high 48 bits of the value consecutively.
 */
bool roaring64_bitmap_contains_bulk(const roaring64_bitmap_t *r,
                                    roaring64_bulk_context_t *context,
                                    uint64_t val);

/**
 * Selects the element at index 'rank' where the smallest element is at index 0.
 * If the size of the bitmap is strictly greater than rank, then this function
 * returns true and sets element to the element of given rank. Otherwise, it
 * returns false.
 */
bool roaring64_bitmap_select(const roaring64_bitmap_t *r, uint64_t rank,
                             uint64_t *element);

/**
 * Returns the number of integers that are smaller or equal to x. Thus if x is
 * the first element, this function will return 1. If x is smaller than the
 * smallest element, this function will return 0.
 *
 * The indexing convention differs between roaring64_bitmap_select and
 * roaring64_bitmap_rank: roaring_bitmap64_select refers to the smallest value
 * as having index 0, whereas roaring64_bitmap_rank returns 1 when ranking
 * the smallest value.
 */
uint64_t roaring64_bitmap_rank(const roaring64_bitmap_t *r, uint64_t val);

/**
 * Returns true if the given value is in the bitmap, and sets `out_index` to the
 * (0-based) index of the value in the bitmap. Returns false if the value is not
 * in the bitmap.
 */
bool roaring64_bitmap_get_index(const roaring64_bitmap_t *r, uint64_t val,
                                uint64_t *out_index);

/**
 * Returns the number of values in the bitmap.
 */
uint64_t roaring64_bitmap_get_cardinality(const roaring64_bitmap_t *r);

/**
 * Returns the number of elements in the range [min, max).
 */
uint64_t roaring64_bitmap_range_cardinality(const roaring64_bitmap_t *r,
                                            uint64_t min, uint64_t max);

/**
 * Returns true if the bitmap is empty (cardinality is zero).
 */
bool roaring64_bitmap_is_empty(const roaring64_bitmap_t *r);

/**
 * Returns the smallest value in the set, or UINT64_MAX if the set is empty.
 */
uint64_t roaring64_bitmap_minimum(const roaring64_bitmap_t *r);

/**
 * Returns the largest value in the set, or 0 if empty.
 */
uint64_t roaring64_bitmap_maximum(const roaring64_bitmap_t *r);

/**
 * Returns true if the result has at least one run container.
 */
bool roaring64_bitmap_run_optimize(roaring64_bitmap_t *r);

/**
 * Returns the in-memory size of the bitmap.
 * TODO: Return the serialized size.
 */
size_t roaring64_bitmap_size_in_bytes(const roaring64_bitmap_t *r);

/**
 * Return true if the two bitmaps contain the same elements.
 */
bool roaring64_bitmap_equals(const roaring64_bitmap_t *r1,
                             const roaring64_bitmap_t *r2);

/**
 * Return true if all the elements of r1 are also in r2.
 */
bool roaring64_bitmap_is_subset(const roaring64_bitmap_t *r1,
                                const roaring64_bitmap_t *r2);

/**
 * Return true if all the elements of r1 are also in r2, and r2 is strictly
 * greater than r1.
 */
bool roaring64_bitmap_is_strict_subset(const roaring64_bitmap_t *r1,
                                       const roaring64_bitmap_t *r2);

/**
 * Computes the intersection between two bitmaps and returns new bitmap. The
 * caller is responsible for free-ing the result.
 *
 * Performance hint: if you are computing the intersection between several
 * bitmaps, two-by-two, it is best to start with the smallest bitmaps. You may
 * also rely on roaring64_bitmap_and_inplace to avoid creating many temporary
 * bitmaps.
 */
roaring64_bitmap_t *roaring64_bitmap_and(const roaring64_bitmap_t *r1,
                                         const roaring64_bitmap_t *r2);

/**
 * Computes the size of the intersection between two bitmaps.
 */
uint64_t roaring64_bitmap_and_cardinality(const roaring64_bitmap_t *r1,
                                          const roaring64_bitmap_t *r2);

/**
 * In-place version of `roaring64_bitmap_and()`, modifies `r1`. `r1` and `r2`
 * are allowed to be equal.
 *
 * Performance hint: if you are computing the intersection between several
 * bitmaps, two-by-two, it is best to start with the smallest bitmaps.
 */
void roaring64_bitmap_and_inplace(roaring64_bitmap_t *r1,
                                  const roaring64_bitmap_t *r2);

/**
 * Check whether two bitmaps intersect.
 */
bool roaring64_bitmap_intersect(const roaring64_bitmap_t *r1,
                                const roaring64_bitmap_t *r2);

/**
 * Computes the Jaccard index between two bitmaps. (Also known as the Tanimoto
 * distance, or the Jaccard similarity coefficient)
 *
 * The Jaccard index is undefined if both bitmaps are empty.
 */
double roaring64_bitmap_jaccard_index(const roaring64_bitmap_t *r1,
                                      const roaring64_bitmap_t *r2);

/**
 * Computes the union between two bitmaps and returns new bitmap. The caller is
 * responsible for free-ing the result.
 */
roaring64_bitmap_t *roaring64_bitmap_or(const roaring64_bitmap_t *r1,
                                        const roaring64_bitmap_t *r2);

/**
 * Computes the size of the union between two bitmaps.
 */
uint64_t roaring64_bitmap_or_cardinality(const roaring64_bitmap_t *r1,
                                         const roaring64_bitmap_t *r2);

/**
 * In-place version of `roaring64_bitmap_or(), modifies `r1`.
 */
void roaring64_bitmap_or_inplace(roaring64_bitmap_t *r1,
                                 const roaring64_bitmap_t *r2);

/**
 * Computes the symmetric difference (xor) between two bitmaps and returns a new
 * bitmap. The caller is responsible for free-ing the result.
 */
roaring64_bitmap_t *roaring64_bitmap_xor(const roaring64_bitmap_t *r1,
                                         const roaring64_bitmap_t *r2);

/**
 * Computes the size of the symmetric difference (xor) between two bitmaps.
 */
uint64_t roaring64_bitmap_xor_cardinality(const roaring64_bitmap_t *r1,
                                          const roaring64_bitmap_t *r2);

/**
 * In-place version of `roaring64_bitmap_xor()`, modifies `r1`. `r1` and `r2`
 * are not allowed to be equal (that would result in an empty bitmap).
 */
void roaring64_bitmap_xor_inplace(roaring64_bitmap_t *r1,
                                  const roaring64_bitmap_t *r2);

/**
 * Computes the difference (andnot) between two bitmaps and returns a new
 * bitmap. The caller is responsible for free-ing the result.
 */
roaring64_bitmap_t *roaring64_bitmap_andnot(const roaring64_bitmap_t *r1,
                                            const roaring64_bitmap_t *r2);

/**
 * Computes the size of the difference (andnot) between two bitmaps.
 */
uint64_t roaring64_bitmap_andnot_cardinality(const roaring64_bitmap_t *r1,
                                             const roaring64_bitmap_t *r2);

/**
 * In-place version of `roaring64_bitmap_andnot()`, modifies `r1`. `r1` and `r2`
 * are not allowed to be equal (that would result in an empty bitmap).
 */
void roaring64_bitmap_andnot_inplace(roaring64_bitmap_t *r1,
                                     const roaring64_bitmap_t *r2);

/**
 * Iterate over the bitmap elements. The function `iterator` is called once for
 * all the values with `ptr` (can be NULL) as the second parameter of each call.
 *
 * `roaring_iterator64` is simply a pointer to a function that returns a bool
 * and takes `(uint64_t, void*)` as inputs. True means that the iteration should
 * continue, while false means that it should stop.
 *
 * Returns true if the `roaring64_iterator` returned true throughout (so that
 * all data points were necessarily visited).
 *
 * Iteration is ordered from the smallest to the largest elements.
 */
bool roaring64_bitmap_iterate(const roaring64_bitmap_t *r,
                              roaring_iterator64 iterator, void *ptr);

#ifdef __cplusplus
}  // extern "C"
}  // namespace roaring
}  // namespace api
#endif

#endif /* ROARING64_H */

