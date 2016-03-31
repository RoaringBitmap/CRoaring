/*
 * mixed_union.c
 *
 */

#include "mixed_union.h"
#include "bitset_util.h"
#include "convert.h"

/* Compute the union of src_1 and src_2 and write the result to
 * dst.  */
void array_bitset_container_union(const array_container_t *src_1,
                                  const bitset_container_t *src_2,
                                  bitset_container_t *dst) {
    if (src_2 != dst) bitset_container_copy(src_2, dst);
    dst->cardinality = bitset_set_list_withcard(
        dst->array, dst->cardinality, src_1->array, src_1->cardinality);
}

void run_bitset_container_union(const run_container_t *src_1,
                                const bitset_container_t *src_2,
                                bitset_container_t *dst) {
    if (run_container_is_full(src_1)) {
        if (src_2 != dst) bitset_container_copy(src_2, dst);
        return;
    }
    if (src_2 != dst) bitset_container_copy(src_2, dst);
    for (int32_t rlepos = 0; rlepos < src_1->n_runs; ++rlepos) {
        rle16_t rle = src_1->runs[rlepos];
        bitset_set_range(dst->array, rle.value,
                         rle.value + rle.length + UINT32_C(1));
    }
    dst->cardinality = bitset_container_compute_cardinality(dst);
}

void array_run_container_union(const array_container_t *src_1,
                               const run_container_t *src_2,
                               run_container_t *dst) {
    if (run_container_is_full(src_2)) {
        run_container_copy(src_2, dst);
        return;
    }
    run_container_grow(dst, 2 * (src_1->cardinality + src_2->n_runs), false);
    int32_t rlepos = 0;
    int32_t arraypos = 0;
    while ((rlepos < src_2->n_runs) && (arraypos < src_1->cardinality)) {
        if (src_2->runs[rlepos].value <= src_1->array[arraypos]) {
            run_container_append(dst, src_2->runs[rlepos]);
            rlepos++;
        } else {
            run_container_append_value(dst, src_1->array[arraypos]);
            arraypos++;
        }
    }
    if (arraypos < src_1->cardinality) {
        while (arraypos < src_1->cardinality) {
            run_container_append_value(dst, src_1->array[arraypos]);
            arraypos++;
        }
    } else {
        while (rlepos < src_2->n_runs) {
            run_container_append(dst, src_2->runs[rlepos]);
            rlepos++;
        }
    }
}

bool array_array_container_union(const array_container_t *src_1,
                                 const array_container_t *src_2, void **dst) {
    int totalCardinality = src_1->cardinality + src_2->cardinality;
    if (totalCardinality <= DEFAULT_MAX_SIZE) {
        *dst = array_container_create_given_capacity(totalCardinality);
        if (*dst != NULL) array_container_union(src_1, src_2, *dst);
        return false;  // not a bitset
    }
    *dst = bitset_container_create();
    bool returnval = true;  // expect a bitset
    if (*dst != NULL) {
        bitset_container_t *ourbitset = *dst;
        bitset_set_list(ourbitset->array, src_1->array, src_1->cardinality);
        ourbitset->cardinality =
            bitset_set_list_withcard(ourbitset->array, src_1->cardinality,
                                     src_2->array, src_2->cardinality);
        if (ourbitset->cardinality <= DEFAULT_MAX_SIZE) {
            // need to convert!
            *dst = array_container_from_bitset(ourbitset);
            bitset_container_free(ourbitset);
            returnval = false;  // not going to be a bitset
        }
    }
    return returnval;
}
