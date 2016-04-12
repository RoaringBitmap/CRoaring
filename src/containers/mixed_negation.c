/*
 * mixed_negation.c
 *
 */

<<<<<<< HEAD
#include "mixed_negation.h"
#include "bitset_util.h"
#include "array_util.h"
#include "convert.h"
=======
#include "containers/mixed_negation.h"
>>>>>>> master
#include <assert.h>
#include <string.h>

/* code makes the assumption that sizeof(int) > 2
 * for ranges. Could use uint32_t instead if this is undesirable.
 * But it seems silly to worry about 16-bit machines with this library.
 */

//#include "array_util.h"
//#include "bitset_util.h"
//#include "convert.h"

// TODO: make simplified and optimized negation code across
// the full range.

/* Negation across the entire range of the container.
 * Compute the  negation of src  and write the result
 * to *dst. The complement of a
 * sufficiently sparse set will always be dense and a hence a bitmap
' * We assume that dst is pre-allocated and a valid bitset container
 * There can be no in-place version.
 */
void array_container_negation(const array_container_t *src,
                              bitset_container_t *dst) {
    uint64_t card = UINT64_C(1 << 16);
    bitset_container_set_all(dst);

    dst->cardinality =
        bitset_clear_list(dst->array, card, src->array, src->cardinality);
}

/* Negation across the entire range of the container
 * Compute the  negation of src  and write the result
 * to *dst.  A true return value indicates a bitset result,
 * otherwise the result is an array container.
 *  We assume that dst is not pre-allocated. In
 * case of failure, *dst will be NULL.
 */
bool bitset_container_negation(const bitset_container_t *src, void **dst) {
    return bitset_container_negation_range(src, 0, (1 << 16), dst);
}

/* inplace version */
/*
 * Same as bitset_container_negation except that if the output is to
 * be a
 * bitset_container_t, then src is modified and no allocation is made.
 * If the output is to be an array_container_t, then caller is responsible
 * to free the container.
 * In all cases, the result is in *dst.
 */
bool bitset_container_negation_inplace(bitset_container_t *src, void **dst) {
    return bitset_container_negation_range_inplace(src, 0, (1 << 16), dst);
}

/* Negation across the entire range of container
 * Compute the  negation of src  and write the result
 * to *dst.  A return value of 0 indicates an array result,
 * while a 1 indicates an bitset result, and 2 indicates a
 * run result.
 *  We assume that dst is not pre-allocated. In
 * case of failure, *dst will be NULL.
 */
int run_container_negation(const run_container_t *src, void **dst) {
    return run_container_negation_range(src, 0, (1 << 16), dst);
}

/*
 * Same as run_container_negation except that if the output is to
 * be a
 * run_container_t, and has the capacity to hold the result,
 * then src is modified and no allocation is made.
 * In all cases, the result is in *dst.
 */
int run_container_negation_inplace(run_container_t *src, void **dst) {
    return run_container_negation_range(src, 0, (1 << 16), dst);
}

/* Negation across a range of the container.
 * Compute the  negation of src  and write the result
 * to *dst. Returns true if the result is a bitset container
 * and false for an array container.  *dst is not preallocated.
 */
bool array_container_negation_range(const array_container_t *src,
                                    const int range_start, const int range_end,
                                    void **dst) {
    /* close port of the Java implementation */
    if (range_start >= range_end) {
        *dst = array_container_clone(src);
        return false;
    }

    int32_t start_index =
        binarySearch(src->array, src->cardinality, (uint16_t)range_start);
    if (start_index < 0) start_index = -start_index - 1;

    int32_t last_index =
        binarySearch(src->array, src->cardinality, (uint16_t)(range_end - 1));
    if (last_index < 0) last_index = -last_index - 2;

    const int32_t current_values_in_range = last_index - start_index + 1;
    const int32_t span_to_be_flipped = range_end - range_start;
    const int32_t new_values_in_range =
        span_to_be_flipped - current_values_in_range;
    const int32_t cardinality_change =
        new_values_in_range - current_values_in_range;
    const int32_t new_cardinality = src->cardinality + cardinality_change;

    if (new_cardinality > DEFAULT_MAX_SIZE) {
        bitset_container_t *temp = bitset_container_from_array(src);
        bitset_flip_range(temp->array, (uint32_t)range_start,
                          (uint32_t)range_end);
        temp->cardinality = new_cardinality;
        *dst = temp;
        return true;
    }

    array_container_t *arr =
        array_container_create_given_capacity(new_cardinality);
    *dst = (void *)arr;
    // copy stuff before the active area
    memcpy(arr->array, src->array, start_index * sizeof(uint16_t));

    // work on the range
    int32_t out_pos = start_index, in_pos = start_index;
    int32_t val_in_range = range_start;
    for (; val_in_range < range_end && in_pos <= last_index; ++val_in_range) {
        if ((uint16_t)val_in_range != src->array[in_pos]) {
            arr->array[out_pos++] = (uint16_t)val_in_range;
        } else {
            ++in_pos;
        }
    }
    for (; val_in_range < range_end; ++val_in_range)
        arr->array[out_pos++] = (uint16_t)val_in_range;

    // content after the active range
    memcpy(arr->array + out_pos, src->array + (last_index + 1),
           (src->cardinality - (last_index + 1)) * sizeof(uint16_t));
    arr->cardinality = new_cardinality;
    return false;
}

/* Even when the result would fit, it is unclear how to make an
 * inplace version without inefficient copying.
 */

/* Negation across a range of the container
 * Compute the  negation of src  and write the result
 * to *dst.  A true return value indicates a bitset result,
 * otherwise the result is an array container.
 *  We assume that dst is not pre-allocated. In
 * case of failure, *dst will be NULL.
 */
bool bitset_container_negation_range(const bitset_container_t *src,
                                     const int range_start, const int range_end,
                                     void **dst) {
    return true;
    // TODO WRITE ME
}

/* inplace version */
/*
 * Same as bitset_container_negation except that if the output is to
 * be a
 * bitset_container_t, then src is modified and no allocation is made.
 * If the output is to be an array_container_t, then caller is responsible
 * to free the container.
 * In all cases, the result is in *dst.
 */
bool bitset_container_negation_range_inplace(bitset_container_t *src,
                                             const int range_start,
                                             const int range_end, void **dst) {
    bitset_flip_range(src->array, (uint32_t)range_start, (uint32_t)range_end);
    src->cardinality = bitset_container_compute_cardinality(src);
    if (src->cardinality > DEFAULT_MAX_SIZE) {
        *dst = src;
        return true;
    }
    // TODO: write the conversion to array and freeing.
    assert(false);
    return false;
}

/* Negation across a range of container
 * Compute the  negation of src  and write the result
 * to *dst.  A return value of 0 indicates an array result,
 * while a 1 indicates a bitset result, and 2 indicates a
 * run result.
 *  We assume that dst is not pre-allocated. In
 * case of failure, *dst will be NULL.
 */
int run_container_negation_range(const run_container_t *src,
                                 const int range_start, const int range_end,
                                 void **dst) {  // TODO WRITE ME
    return 0;
}

/*
 * Same as run_container_negation except that if the output is to
 * be a
 * run_container_t, and has the capacity to hold the result,
 * then src is modified and no allocation is made.
 * In all cases, the result is in *dst.
 */
int run_container_negation_range_inplace(run_container_t *src,
                                         const int range_start,
                                         const int range_end, void **dst) {
    // TODO WRITE
    return 0;
}
