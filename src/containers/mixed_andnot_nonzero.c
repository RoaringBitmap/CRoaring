/*
 * mixed_andnot_nonzeroion.c
 *
 */

#include <roaring/array_util.h>
#include <roaring/bitset_util.h>
#include <roaring/containers/convert.h>
#include <roaring/containers/mixed_andnot_nonzero.h>

#include "containers/array.h"
#include "containers/bitset.h"
#include "containers/run.h"

#ifdef __cplusplus
extern "C" {
namespace roaring {
namespace internal {
#endif

bool bitset_run_container_andnot_nonzero(const bitset_container_t *src_1,
                                         const run_container_t *src_2) {
    if (bitset_container_empty(src_1) || run_container_is_full(src_2)) {
        return false;
    }
    if (src_2->n_runs == 0) {
        return true;
    }

    int32_t rlepos = 0;
    rle16_t blank_rle;
    rle16_t rle = src_2->runs[rlepos];
    if (rle.value != 0) {
        blank_rle = MAKE_RLE16(0, rle.value - 1);
        if (!bitset_lenrange_empty(src_1->words, blank_rle.value,
                                   blank_rle.length)) {
            return true;
        }
    }

    while (++rlepos < src_2->n_runs) {
        rle = src_2->runs[rlepos];
        rle16_t pre_rle = src_2->runs[rlepos-1];
        blank_rle = MAKE_RLE16(pre_rle.value + pre_rle.length + 1,
                                   rle.value - pre_rle.value - pre_rle.length - 2);
        if (!bitset_lenrange_empty(src_1->words, blank_rle.value,
                                   blank_rle.length)) {
            return true;
        }
    }
    if (rle.value + rle.length < 65535) {
        blank_rle = MAKE_RLE16(rle.value + rle.length + 1,
                                   65535 - rle.value - rle.length - 2);
        return !bitset_lenrange_empty(src_1->words, blank_rle.value,
                                   blank_rle.length);
    }
    return false;
}

bool bitset_array_container_andnot_nonzero(const bitset_container_t *src_1,
                                           const array_container_t *src_2) {
    if (bitset_container_empty(src_1)) {
        return false;
    }
    if (array_container_empty(src_2)) {
        return true;
    }

    rle16_t blank_rle;

    const int32_t origcard = src_2->cardinality;
    uint16_t key = src_2->array[0];
    if (key != 0) {
        blank_rle = MAKE_RLE16(0, key - 1);
        if (!bitset_lenrange_empty(src_1->words, blank_rle.value,
                                   blank_rle.length)) {
            return true;
        }
    }

    for (int i = 1; i < origcard; ++i) {
        uint16_t pre_key = key;
        key = src_2->array[i];
        if (pre_key + 1 < key) {
            blank_rle = MAKE_RLE16(pre_key + 1, key - pre_key - 2);
            if (!bitset_lenrange_empty(src_1->words, blank_rle.value,
                                       blank_rle.length)) {
                return true;
            }
        }
    }

    if (key < 65535) {
        blank_rle = MAKE_RLE16(key + 1, 65535 - key - 2);
        return !bitset_lenrange_empty(src_1->words, blank_rle.value,
                                      blank_rle.length);
    }
    return false;
}

bool array_bitset_container_andnot_nonzero(const array_container_t *src_1,
                                           const bitset_container_t *src_2) {
    const int32_t origcard = src_1->cardinality;
    for (int i = 0; i < origcard; ++i) {
        uint16_t key = src_1->array[i];
        if (!bitset_container_contains(src_2, key)) return true;
    }
    return false;
}

bool array_run_container_andnot_nonzero(const array_container_t *src_1,
                                        const run_container_t *src_2) {
    if (array_container_empty(src_1) || run_container_is_full(src_2)) {
        return false;
    }
    if (src_2->n_runs == 0) {
        return true;
    }

    int32_t rlepos = 0;
    int32_t arraypos = 0;
    rle16_t rle = src_2->runs[rlepos];
    while (arraypos < src_1->cardinality) {
        const uint16_t arrayval = src_1->array[arraypos];
        while (arrayval > rle.value + rle.length) {
            ++rlepos;
            if (rlepos == src_2->n_runs) {
                return true;  // we are done
            }
            rle = src_2->runs[rlepos];
        }

        if (arrayval < rle.value) {
            return true;
        }

        arraypos = advanceUntil(src_1->array, arraypos, src_1->cardinality,
                                rle.value + rle.length);
    }
    return false;
}

bool run_bitset_container_andnot_nonzero(const run_container_t *src_1,
                                         const bitset_container_t *src_2) {
    for (int32_t rlepos = 0; rlepos < src_1->n_runs; ++rlepos) {
        rle16_t rle = src_1->runs[rlepos];
        if (bitset_lenrange_cardinality(src_2->words, rle.value, rle.length) <
            rle.length) {
            return true;
        }
    }
    return false;
}

bool run_array_container_andnot_nonzero(const run_container_t *src_1,
                                        const array_container_t *src_2) {
    if (src_1->n_runs == 0) {
        return false;
    }
    if (array_container_empty(src_2) || run_container_is_full(src_1)) {
        return true;
    }

    int32_t arraypos = 0;
    for (int32_t rlepos = 0; rlepos < src_1->n_runs; ++rlepos) {
        rle16_t rle = src_1->runs[rlepos];
        if (src_2->array[arraypos] < rle.value) {
            arraypos = advanceUntil(src_2->array, arraypos, src_2->cardinality,
                                    rle.value);
            if (arraypos >= src_2->cardinality) {
                return true;
            }
        }
        if (src_2->array[arraypos] > rle.value) {
            return true;
        }

        // src_2->array[arraypos] == rle.value
        if (rle.length == 0) {
            ++arraypos;
            continue;
        }
        arraypos += rle.length;
        if (arraypos >= src_2->cardinality) {
            return true;
        }
        if (src_2->array[arraypos] > rle.value + rle.length) {
            return true;
        }
    }
    return false;
}

#ifdef __cplusplus
}
}
}  // extern "C" { namespace roaring { namespace internal {
#endif
