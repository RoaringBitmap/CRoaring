#include <assert.h>
#include <roaring/array_util.h>
#include <roaring/roaring.h>
#include <roaring/roaring_array.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern inline bool roaring_bitmap_contains(const roaring_bitmap_t *r,
                                           uint32_t val);
extern inline bool roaring_bitmap_is_strict_subset(const roaring_bitmap_t *ra1,
                                                   const roaring_bitmap_t *ra2);

// this is like roaring_bitmap_add, but it populates pointer arguments in such a
// way
// that we can recover the container touched, which, in turn can be used to
// accelerate some functions (when you repeatedly need to add to the same
// container)
static inline void *containerptr_roaring_bitmap_add(roaring_bitmap_t *r,
                                                    uint32_t val,
                                                    uint8_t *typecode,
                                                    int *index) {
    uint16_t hb = val >> 16;
    const int i = ra_get_index(&r->high_low_container, hb);
    if (i >= 0) {
        ra_unshare_container_at_index(&r->high_low_container, i);
        void *container =
            ra_get_container_at_index(&r->high_low_container, i, typecode);
        uint8_t newtypecode = *typecode;
        void *container2 =
            container_add(container, val & 0xFFFF, *typecode, &newtypecode);
        *index = i;
        if (container2 != container) {
            container_free(container, *typecode);
            ra_set_container_at_index(&r->high_low_container, i, container2,
                                      newtypecode);
            *typecode = newtypecode;
            return container2;
        } else {
            return container;
        }
    } else {
        array_container_t *newac = array_container_create();
        void *container = container_add(newac, val & 0xFFFF,
                                        ARRAY_CONTAINER_TYPE_CODE, typecode);
        // we could just assume that it stays an array container
        ra_insert_new_key_value_at(&r->high_low_container, -i - 1, hb,
                                   container, *typecode);
        *index = -i - 1;
        return container;
    }
}

roaring_bitmap_t *roaring_bitmap_create() {
    roaring_bitmap_t *ans =
        (roaring_bitmap_t *)malloc(sizeof(roaring_bitmap_t));
    if (!ans) {
        return NULL;
    }
    bool is_ok = ra_init(&ans->high_low_container);
    if (!is_ok) {
        free(ans);
        return NULL;
    }
    ans->copy_on_write = false;
    return ans;
}

roaring_bitmap_t *roaring_bitmap_create_with_capacity(uint32_t cap) {
    roaring_bitmap_t *ans =
        (roaring_bitmap_t *)malloc(sizeof(roaring_bitmap_t));
    if (!ans) {
        return NULL;
    }
    bool is_ok = ra_init_with_capacity(&ans->high_low_container, cap);
    if (!is_ok) {
        free(ans);
        return NULL;
    }
    ans->copy_on_write = false;
    return ans;
}

void roaring_bitmap_add_many(roaring_bitmap_t *r, size_t n_args,
                             const uint32_t *vals) {
    void *container = NULL;  // hold value of last container touched
    uint8_t typecode = 0;    // typecode of last container touched
    uint32_t prev = 0;       // previous valued inserted
    size_t i = 0;            // index of value
    int containerindex = 0;
    if (n_args == 0) return;
    uint32_t val;
    memcpy(&val, vals + i, sizeof(val));
    container =
        containerptr_roaring_bitmap_add(r, val, &typecode, &containerindex);
    prev = val;
    i++;
    for (; i < n_args; i++) {
        memcpy(&val, vals + i, sizeof(val));
        if (((prev ^ val) >> 16) ==
            0) {  // no need to seek the container, it is at hand
            // because we already have the container at hand, we can do the
            // insertion
            // automatically, bypassing the roaring_bitmap_add call
            uint8_t newtypecode = typecode;
            void *container2 =
                container_add(container, val & 0xFFFF, typecode, &newtypecode);
            if (container2 != container) {  // rare instance when we need to
                                            // change the container type
                container_free(container, typecode);
                ra_set_container_at_index(&r->high_low_container,
                                          containerindex, container2,
                                          newtypecode);
                typecode = newtypecode;
                container = container2;
            }
        } else {
            container = containerptr_roaring_bitmap_add(r, val, &typecode,
                                                        &containerindex);
        }
        prev = val;
    }
}

roaring_bitmap_t *roaring_bitmap_of_ptr(size_t n_args, const uint32_t *vals) {
    roaring_bitmap_t *answer = roaring_bitmap_create();
    roaring_bitmap_add_many(answer, n_args, vals);
    return answer;
}

roaring_bitmap_t *roaring_bitmap_of(size_t n_args, ...) {
    // todo: could be greatly optimized but we do not expect this call to ever
    // include long lists
    roaring_bitmap_t *answer = roaring_bitmap_create();
    va_list ap;
    va_start(ap, n_args);
    for (size_t i = 1; i <= n_args; i++) {
        uint32_t val = va_arg(ap, uint32_t);
        roaring_bitmap_add(answer, val);
    }
    va_end(ap);
    return answer;
}

static inline uint32_t minimum_uint32(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

roaring_bitmap_t *roaring_bitmap_from_range(uint32_t min, uint32_t max,
                                            uint32_t step) {
    if (step == 0) return NULL;
    if (max <= min) return NULL;
    roaring_bitmap_t *answer = roaring_bitmap_create();
    if (step >= (1 << 16)) {
        for (uint32_t value = min; value < max; value += step) {
            roaring_bitmap_add(answer, value);
        }
        return answer;
    }
    uint32_t min_tmp = min;
    do {
        uint32_t key = min_tmp >> 16;
        uint32_t container_min = min_tmp & 0xFFFF;
        uint32_t container_max = minimum_uint32(max - (key << 16), 1 << 16);
        uint8_t type;
        void *container = container_from_range(&type, container_min,
                                               container_max, (uint16_t)step);
        ra_append(&answer->high_low_container, key, container, type);
        uint32_t gap = container_max - container_min + step - 1;
        min_tmp += gap - (gap % step);
    } while (min_tmp < max);
    // cardinality of bitmap will be ((uint64_t) max - min + step - 1 ) / step
    return answer;
}

void roaring_bitmap_printf(const roaring_bitmap_t *ra) {
    printf("{");
    for (int i = 0; i < ra->high_low_container.size; ++i) {
        container_printf_as_uint32_array(
            ra->high_low_container.containers[i],
            ra->high_low_container.typecodes[i],
            ((uint32_t)ra->high_low_container.keys[i]) << 16);
        if (i + 1 < ra->high_low_container.size) printf(",");
    }
    printf("}");
}

void roaring_bitmap_printf_describe(const roaring_bitmap_t *ra) {
    printf("{");
    for (int i = 0; i < ra->high_low_container.size; ++i) {
        printf("%d: %s (%d)", ra->high_low_container.keys[i],
               get_full_container_name(ra->high_low_container.containers[i],
                                       ra->high_low_container.typecodes[i]),
               container_get_cardinality(ra->high_low_container.containers[i],
                                         ra->high_low_container.typecodes[i]));
        if (ra->high_low_container.typecodes[i] == SHARED_CONTAINER_TYPE_CODE) {
            printf(
                "(shared count = %u )",
                ((shared_container_t *)(ra->high_low_container.containers[i]))
                    ->counter);
        }

        if (i + 1 < ra->high_low_container.size) printf(", ");
    }
    printf("}");
}

typedef struct min_max_sum_s {
    uint32_t min;
    uint32_t max;
    uint64_t sum;
} min_max_sum_t;

static bool min_max_sum_fnc(uint32_t value, void *param) {
    min_max_sum_t *mms = (min_max_sum_t *)param;
    if (value > mms->max) mms->max = value;
    if (value < mms->min) mms->min = value;
    mms->sum += value;
    return true;  // we always process all data points
}

/**
*  (For advanced users.)
* Collect statistics about the bitmap
*/
void roaring_bitmap_statistics(const roaring_bitmap_t *ra,
                               roaring_statistics_t *stat) {
    memset(stat, 0, sizeof(*stat));
    stat->n_containers = ra->high_low_container.size;
    stat->cardinality = roaring_bitmap_get_cardinality(ra);
    min_max_sum_t mms;
    mms.min = UINT32_C(0xFFFFFFFF);
    mms.max = UINT32_C(0);
    mms.sum = 0;
    roaring_iterate(ra, &min_max_sum_fnc, &mms);
    stat->min_value = mms.min;
    stat->max_value = mms.max;
    stat->sum_value = mms.sum;

    for (int i = 0; i < ra->high_low_container.size; ++i) {
        uint8_t truetype =
            get_container_type(ra->high_low_container.containers[i],
                               ra->high_low_container.typecodes[i]);
        uint32_t card =
            container_get_cardinality(ra->high_low_container.containers[i],
                                      ra->high_low_container.typecodes[i]);
        uint32_t sbytes =
            container_size_in_bytes(ra->high_low_container.containers[i],
                                    ra->high_low_container.typecodes[i]);
        switch (truetype) {
            case BITSET_CONTAINER_TYPE_CODE:
                stat->n_bitset_containers++;
                stat->n_values_bitset_containers += card;
                stat->n_bytes_bitset_containers += sbytes;
                break;
            case ARRAY_CONTAINER_TYPE_CODE:
                stat->n_array_containers++;
                stat->n_values_array_containers += card;
                stat->n_bytes_array_containers += sbytes;
                break;
            case RUN_CONTAINER_TYPE_CODE:
                stat->n_run_containers++;
                stat->n_values_run_containers += card;
                stat->n_bytes_run_containers += sbytes;
                break;
            default:
                assert(false);
                __builtin_unreachable();
        }
    }
}

roaring_bitmap_t *roaring_bitmap_copy(const roaring_bitmap_t *r) {
    roaring_bitmap_t *ans =
        (roaring_bitmap_t *)malloc(sizeof(roaring_bitmap_t));
    if (!ans) {
        return NULL;
    }
    bool is_ok = ra_copy(&r->high_low_container, &ans->high_low_container,
                         r->copy_on_write);
    if (!is_ok) {
        free(ans);
        return NULL;
    }
    ans->copy_on_write = r->copy_on_write;
    return ans;
}

static bool roaring_bitmap_overwrite(roaring_bitmap_t *dest,
                                     const roaring_bitmap_t *src) {
    return ra_overwrite(&src->high_low_container, &dest->high_low_container,
                        src->copy_on_write);
}

void roaring_bitmap_free(roaring_bitmap_t *r) {
    ra_clear(&r->high_low_container);
    free(r);
}

void roaring_bitmap_clear(roaring_bitmap_t *r) {
  ra_reset(&r->high_low_container);
}

void roaring_bitmap_add(roaring_bitmap_t *r, uint32_t val) {
    const uint16_t hb = val >> 16;
    const int i = ra_get_index(&r->high_low_container, hb);
    uint8_t typecode;
    if (i >= 0) {
        ra_unshare_container_at_index(&r->high_low_container, i);
        void *container =
            ra_get_container_at_index(&r->high_low_container, i, &typecode);
        uint8_t newtypecode = typecode;
        void *container2 =
            container_add(container, val & 0xFFFF, typecode, &newtypecode);
        if (container2 != container) {
            container_free(container, typecode);
            ra_set_container_at_index(&r->high_low_container, i, container2,
                                      newtypecode);
        }
    } else {
        array_container_t *newac = array_container_create();
        void *container = container_add(newac, val & 0xFFFF,
                                        ARRAY_CONTAINER_TYPE_CODE, &typecode);
        // we could just assume that it stays an array container
        ra_insert_new_key_value_at(&r->high_low_container, -i - 1, hb,
                                   container, typecode);
    }
}

void roaring_bitmap_remove(roaring_bitmap_t *r, uint32_t val) {
    const uint16_t hb = val >> 16;
    const int i = ra_get_index(&r->high_low_container, hb);
    uint8_t typecode;
    if (i >= 0) {
        ra_unshare_container_at_index(&r->high_low_container, i);
        void *container =
            ra_get_container_at_index(&r->high_low_container, i, &typecode);
        uint8_t newtypecode = typecode;
        void *container2 =
            container_remove(container, val & 0xFFFF, typecode, &newtypecode);
        if (container2 != container) {
            container_free(container, typecode);
            ra_set_container_at_index(&r->high_low_container, i, container2,
                                      newtypecode);
        }
        if (container_get_cardinality(container2, newtypecode) != 0) {
            ra_set_container_at_index(&r->high_low_container, i, container2,
                                      newtypecode);
        } else {
            ra_remove_at_index_and_free(&r->high_low_container, i);
        }
    }
}

extern bool roaring_bitmap_contains(const roaring_bitmap_t *r, uint32_t val);

// there should be some SIMD optimizations possible here
roaring_bitmap_t *roaring_bitmap_and(const roaring_bitmap_t *x1,
                                     const roaring_bitmap_t *x2) {
    uint8_t container_result_type = 0;
    const int length1 = x1->high_low_container.size,
              length2 = x2->high_low_container.size;
    uint32_t neededcap = length1 > length2 ? length2 : length1;
    roaring_bitmap_t *answer = roaring_bitmap_create_with_capacity(neededcap);
    answer->copy_on_write = x1->copy_on_write && x2->copy_on_write;

    int pos1 = 0, pos2 = 0;

    while (pos1 < length1 && pos2 < length2) {
        const uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
        const uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        if (s1 == s2) {
            uint8_t container_type_1, container_type_2;
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            void *c = container_and(c1, container_type_1, c2, container_type_2,
                                    &container_result_type);
            if (container_nonzero_cardinality(c, container_result_type)) {
                ra_append(&answer->high_low_container, s1, c,
                          container_result_type);
            } else {
                container_free(
                    c, container_result_type);  // otherwise:memory leak!
            }
            ++pos1;
            ++pos2;
        } else if (s1 < s2) {  // s1 < s2
            pos1 = ra_advance_until(&x1->high_low_container, s2, pos1);
        } else {  // s1 > s2
            pos2 = ra_advance_until(&x2->high_low_container, s1, pos2);
        }
    }
    return answer;
}

/**
 * Compute the union of 'number' bitmaps.
 */
roaring_bitmap_t *roaring_bitmap_or_many(size_t number,
                                         const roaring_bitmap_t **x) {
    if (number == 0) {
        return roaring_bitmap_create();
    }
    if (number == 1) {
        return roaring_bitmap_copy(x[0]);
    }
    roaring_bitmap_t *answer =
        roaring_bitmap_lazy_or(x[0], x[1], LAZY_OR_BITSET_CONVERSION);
    for (size_t i = 2; i < number; i++) {
        roaring_bitmap_lazy_or_inplace(answer, x[i], LAZY_OR_BITSET_CONVERSION);
    }
    roaring_bitmap_repair_after_lazy(answer);
    return answer;
}

/**
 * Compute the xor of 'number' bitmaps.
 */
roaring_bitmap_t *roaring_bitmap_xor_many(size_t number,
                                          const roaring_bitmap_t **x) {
    if (number == 0) {
        return roaring_bitmap_create();
    }
    if (number == 1) {
        return roaring_bitmap_copy(x[0]);
    }
    roaring_bitmap_t *answer = roaring_bitmap_lazy_xor(x[0], x[1]);
    for (size_t i = 2; i < number; i++) {
        roaring_bitmap_lazy_xor_inplace(answer, x[i]);
    }
    roaring_bitmap_repair_after_lazy(answer);
    return answer;
}

// inplace and (modifies its first argument).
void roaring_bitmap_and_inplace(roaring_bitmap_t *x1,
                                const roaring_bitmap_t *x2) {
    if (x1 == x2) return;
    int pos1 = 0, pos2 = 0, intersection_size = 0;
    const int length1 = ra_get_size(&x1->high_low_container);
    const int length2 = ra_get_size(&x2->high_low_container);

    // any skipped-over or newly emptied containers in x1
    // have to be freed.
    while (pos1 < length1 && pos2 < length2) {
        const uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
        const uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        if (s1 == s2) {
            uint8_t typecode1, typecode2, typecode_result;
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &typecode1);
            c1 = get_writable_copy_if_shared(c1, &typecode1);
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &typecode2);
            void *c =
                container_iand(c1, typecode1, c2, typecode2, &typecode_result);
            if (c != c1) {  // in this instance a new container was created, and
                            // we need to free the old one
                container_free(c1, typecode1);
            }
            if (container_nonzero_cardinality(c, typecode_result)) {
                ra_replace_key_and_container_at_index(&x1->high_low_container,
                                                      intersection_size, s1, c,
                                                      typecode_result);
                intersection_size++;
            } else {
                container_free(c, typecode_result);
            }
            ++pos1;
            ++pos2;
        } else if (s1 < s2) {
            pos1 = ra_advance_until_freeing(&x1->high_low_container, s2, pos1);
        } else {  // s1 > s2
            pos2 = ra_advance_until(&x2->high_low_container, s1, pos2);
        }
    }

    // if we ended early because x2 ran out, then all remaining in x1 should be
    // freed
    while (pos1 < length1) {
        container_free(x1->high_low_container.containers[pos1],
                       x1->high_low_container.typecodes[pos1]);
        ++pos1;
    }

    // all containers after this have either been copied or freed
    ra_downsize(&x1->high_low_container, intersection_size);
}

roaring_bitmap_t *roaring_bitmap_or(const roaring_bitmap_t *x1,
                                    const roaring_bitmap_t *x2) {
    uint8_t container_result_type = 0;
    const int length1 = x1->high_low_container.size,
              length2 = x2->high_low_container.size;
    if (0 == length1) {
        return roaring_bitmap_copy(x2);
    }
    if (0 == length2) {
        return roaring_bitmap_copy(x1);
    }
    roaring_bitmap_t *answer =
        roaring_bitmap_create_with_capacity(length1 + length2);
    answer->copy_on_write = x1->copy_on_write && x2->copy_on_write;
    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
    uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
    while (true) {
        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            void *c = container_or(c1, container_type_1, c2, container_type_2,
                                   &container_result_type);
            // since we assume that the initial containers are non-empty, the
            // result here
            // can only be non-empty
            ra_append(&answer->high_low_container, s1, c,
                      container_result_type);
            ++pos1;
            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        } else if (s1 < s2) {  // s1 < s2
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            // c1 = container_clone(c1, container_type_1);
            c1 =
                get_copy_of_container(c1, &container_type_1, x1->copy_on_write);
            if (x1->copy_on_write) {
                ra_set_container_at_index(&x1->high_low_container, pos1, c1,
                                          container_type_1);
            }
            ra_append(&answer->high_low_container, s1, c1, container_type_1);
            pos1++;
            if (pos1 == length1) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);

        } else {  // s1 > s2
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            // c2 = container_clone(c2, container_type_2);
            c2 =
                get_copy_of_container(c2, &container_type_2, x2->copy_on_write);
            if (x2->copy_on_write) {
                ra_set_container_at_index(&x2->high_low_container, pos2, c2,
                                          container_type_2);
            }
            ra_append(&answer->high_low_container, s2, c2, container_type_2);
            pos2++;
            if (pos2 == length2) break;
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
        }
    }
    if (pos1 == length1) {
        ra_append_copy_range(&answer->high_low_container,
                             &x2->high_low_container, pos2, length2,
                             x2->copy_on_write);
    } else if (pos2 == length2) {
        ra_append_copy_range(&answer->high_low_container,
                             &x1->high_low_container, pos1, length1,
                             x1->copy_on_write);
    }
    return answer;
}

// inplace or (modifies its first argument).
void roaring_bitmap_or_inplace(roaring_bitmap_t *x1,
                               const roaring_bitmap_t *x2) {
    uint8_t container_result_type = 0;
    int length1 = x1->high_low_container.size;
    const int length2 = x2->high_low_container.size;

    if (0 == length2) return;

    if (0 == length1) {
        roaring_bitmap_overwrite(x1, x2);
        return;
    }
    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
    uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
    while (true) {
        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            if (!container_is_full(c1, container_type_1)) {
                c1 = get_writable_copy_if_shared(c1, &container_type_1);

                void *c2 = ra_get_container_at_index(&x2->high_low_container,
                                                     pos2, &container_type_2);
                void *c =
                    container_ior(c1, container_type_1, c2, container_type_2,
                                  &container_result_type);
                if (c !=
                    c1) {  // in this instance a new container was created, and
                           // we need to free the old one
                    container_free(c1, container_type_1);
                }

                ra_set_container_at_index(&x1->high_low_container, pos1, c,
                                          container_result_type);
            }
            ++pos1;
            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        } else if (s1 < s2) {  // s1 < s2
            pos1++;
            if (pos1 == length1) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);

        } else {  // s1 > s2
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            c2 =
                get_copy_of_container(c2, &container_type_2, x2->copy_on_write);
            if (x2->copy_on_write) {
                ra_set_container_at_index(&x2->high_low_container, pos2, c2,
                                          container_type_2);
            }

            // void *c2_clone = container_clone(c2, container_type_2);
            ra_insert_new_key_value_at(&x1->high_low_container, pos1, s2, c2,
                                       container_type_2);
            pos1++;
            length1++;
            pos2++;
            if (pos2 == length2) break;
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
        }
    }
    if (pos1 == length1) {
        ra_append_copy_range(&x1->high_low_container, &x2->high_low_container,
                             pos2, length2, x2->copy_on_write);
    }
}

roaring_bitmap_t *roaring_bitmap_xor(const roaring_bitmap_t *x1,
                                     const roaring_bitmap_t *x2) {
    uint8_t container_result_type = 0;
    const int length1 = x1->high_low_container.size,
              length2 = x2->high_low_container.size;
    if (0 == length1) {
        return roaring_bitmap_copy(x2);
    }
    if (0 == length2) {
        return roaring_bitmap_copy(x1);
    }
    roaring_bitmap_t *answer =
        roaring_bitmap_create_with_capacity(length1 + length2);
    answer->copy_on_write = x1->copy_on_write && x2->copy_on_write;
    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
    uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
    while (true) {
        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            void *c = container_xor(c1, container_type_1, c2, container_type_2,
                                    &container_result_type);

            if (container_nonzero_cardinality(c, container_result_type)) {
                ra_append(&answer->high_low_container, s1, c,
                          container_result_type);
            } else {
                container_free(c, container_result_type);
            }
            ++pos1;
            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        } else if (s1 < s2) {  // s1 < s2
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            c1 =
                get_copy_of_container(c1, &container_type_1, x1->copy_on_write);
            if (x1->copy_on_write) {
                ra_set_container_at_index(&x1->high_low_container, pos1, c1,
                                          container_type_1);
            }
            ra_append(&answer->high_low_container, s1, c1, container_type_1);
            pos1++;
            if (pos1 == length1) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);

        } else {  // s1 > s2
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            c2 =
                get_copy_of_container(c2, &container_type_2, x2->copy_on_write);
            if (x2->copy_on_write) {
                ra_set_container_at_index(&x2->high_low_container, pos2, c2,
                                          container_type_2);
            }
            ra_append(&answer->high_low_container, s2, c2, container_type_2);
            pos2++;
            if (pos2 == length2) break;
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
        }
    }
    if (pos1 == length1) {
        ra_append_copy_range(&answer->high_low_container,
                             &x2->high_low_container, pos2, length2,
                             x2->copy_on_write);
    } else if (pos2 == length2) {
        ra_append_copy_range(&answer->high_low_container,
                             &x1->high_low_container, pos1, length1,
                             x1->copy_on_write);
    }
    return answer;
}

// inplace xor (modifies its first argument).

void roaring_bitmap_xor_inplace(roaring_bitmap_t *x1,
                                const roaring_bitmap_t *x2) {
    assert(x1 != x2);
    uint8_t container_result_type = 0;
    int length1 = x1->high_low_container.size;
    const int length2 = x2->high_low_container.size;

    if (0 == length2) return;

    if (0 == length1) {
        roaring_bitmap_overwrite(x1, x2);
        return;
    }

    // XOR can have new containers inserted from x2, but can also
    // lose containers when x1 and x2 are nonempty and identical.

    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
    uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
    while (true) {
        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            c1 = get_writable_copy_if_shared(c1, &container_type_1);

            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            void *c = container_ixor(c1, container_type_1, c2, container_type_2,
                                     &container_result_type);

            if (container_nonzero_cardinality(c, container_result_type)) {
                ra_set_container_at_index(&x1->high_low_container, pos1, c,
                                          container_result_type);
                ++pos1;
            } else {
                container_free(c, container_result_type);
                ra_remove_at_index(&x1->high_low_container, pos1);
                --length1;
            }

            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        } else if (s1 < s2) {  // s1 < s2
            pos1++;
            if (pos1 == length1) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);

        } else {  // s1 > s2
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            c2 =
                get_copy_of_container(c2, &container_type_2, x2->copy_on_write);
            if (x2->copy_on_write) {
                ra_set_container_at_index(&x2->high_low_container, pos2, c2,
                                          container_type_2);
            }

            ra_insert_new_key_value_at(&x1->high_low_container, pos1, s2, c2,
                                       container_type_2);
            pos1++;
            length1++;
            pos2++;
            if (pos2 == length2) break;
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
        }
    }
    if (pos1 == length1) {
        ra_append_copy_range(&x1->high_low_container, &x2->high_low_container,
                             pos2, length2, x2->copy_on_write);
    }
}

roaring_bitmap_t *roaring_bitmap_andnot(const roaring_bitmap_t *x1,
                                        const roaring_bitmap_t *x2) {
    uint8_t container_result_type = 0;
    const int length1 = x1->high_low_container.size,
              length2 = x2->high_low_container.size;
    if (0 == length1) {
        roaring_bitmap_t *empty_bitmap = roaring_bitmap_create();
        empty_bitmap->copy_on_write = x1->copy_on_write && x2->copy_on_write;
        return empty_bitmap;
    }
    if (0 == length2) {
        return roaring_bitmap_copy(x1);
    }
    roaring_bitmap_t *answer = roaring_bitmap_create_with_capacity(length1);
    answer->copy_on_write = x1->copy_on_write && x2->copy_on_write;

    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = 0;
    uint16_t s2 = 0;
    while (true) {
        s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
        s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            void *c =
                container_andnot(c1, container_type_1, c2, container_type_2,
                                 &container_result_type);

            if (container_nonzero_cardinality(c, container_result_type)) {
                ra_append(&answer->high_low_container, s1, c,
                          container_result_type);
            } else {
                container_free(c, container_result_type);
            }
            ++pos1;
            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
        } else if (s1 < s2) {  // s1 < s2
            const int next_pos1 =
                ra_advance_until(&x1->high_low_container, s2, pos1);
            ra_append_copy_range(&answer->high_low_container,
                                 &x1->high_low_container, pos1, next_pos1,
                                 x1->copy_on_write);
            // TODO : perhaps some of the copy_on_write should be based on
            // answer rather than x1 (more stringent?).  Many similar cases
            pos1 = next_pos1;
            if (pos1 == length1) break;
        } else {  // s1 > s2
            pos2 = ra_advance_until(&x2->high_low_container, s1, pos2);
            if (pos2 == length2) break;
        }
    }
    if (pos2 == length2) {
        ra_append_copy_range(&answer->high_low_container,
                             &x1->high_low_container, pos1, length1,
                             x1->copy_on_write);
    }
    return answer;
}

// inplace andnot (modifies its first argument).

void roaring_bitmap_andnot_inplace(roaring_bitmap_t *x1,
                                   const roaring_bitmap_t *x2) {
    assert(x1 != x2);

    uint8_t container_result_type = 0;
    int length1 = x1->high_low_container.size;
    const int length2 = x2->high_low_container.size;
    int intersection_size = 0;

    if (0 == length2) return;

    if (0 == length1) {
        roaring_bitmap_clear(x1);
        return;
    }

    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
    uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
    while (true) {
        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            c1 = get_writable_copy_if_shared(c1, &container_type_1);

            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            void *c =
                container_iandnot(c1, container_type_1, c2, container_type_2,
                                  &container_result_type);

            if (container_nonzero_cardinality(c, container_result_type)) {
                ra_replace_key_and_container_at_index(&x1->high_low_container,
                                                      intersection_size++, s1,
                                                      c, container_result_type);
            } else {
                container_free(c, container_result_type);
            }

            ++pos1;
            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        } else if (s1 < s2) {  // s1 < s2
            if (pos1 != intersection_size) {
                void *c1 = ra_get_container_at_index(&x1->high_low_container,
                                                     pos1, &container_type_1);

                ra_replace_key_and_container_at_index(&x1->high_low_container,
                                                      intersection_size, s1, c1,
                                                      container_type_1);
            }
            intersection_size++;
            pos1++;
            if (pos1 == length1) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);

        } else {  // s1 > s2
            pos2 = ra_advance_until(&x2->high_low_container, s1, pos2);
            if (pos2 == length2) break;
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
        }
    }

    if (pos1 < length1) {
        // all containers between intersection_size and
        // pos1 are junk.  However, they have either been moved
        // (thus still referenced) or involved in an iandnot
        // that will clean up all containers that could not be reused.
        // Thus we should not free the junk containers between
        // intersection_size and pos1.
        if (pos1 > intersection_size) {
            // left slide of remaining items
            ra_copy_range(&x1->high_low_container, pos1, length1,
                          intersection_size);
        }
        // else current placement is fine
        intersection_size += (length1 - pos1);
    }
    ra_downsize(&x1->high_low_container, intersection_size);
}

uint64_t roaring_bitmap_get_cardinality(const roaring_bitmap_t *ra) {
    uint64_t card = 0;
    for (int i = 0; i < ra->high_low_container.size; ++i)
        card += container_get_cardinality(ra->high_low_container.containers[i],
                                          ra->high_low_container.typecodes[i]);
    return card;
}

bool roaring_bitmap_is_empty(const roaring_bitmap_t *ra) {
    return ra->high_low_container.size == 0;
}

void roaring_bitmap_to_uint32_array(const roaring_bitmap_t *ra, uint32_t *ans) {
    ra_to_uint32_array(&ra->high_low_container, ans);
}

/** convert array and bitmap containers to run containers when it is more
 * efficient;
 * also convert from run containers when more space efficient.  Returns
 * true if the result has at least one run container.
*/
bool roaring_bitmap_run_optimize(roaring_bitmap_t *r) {
    bool answer = false;
    for (int i = 0; i < r->high_low_container.size; i++) {
        uint8_t typecode_original, typecode_after;
        ra_unshare_container_at_index(
            &r->high_low_container, i);  // TODO: this introduces extra cloning!
        void *c = ra_get_container_at_index(&r->high_low_container, i,
                                            &typecode_original);
        void *c1 = convert_run_optimize(c, typecode_original, &typecode_after);
        if (typecode_after == RUN_CONTAINER_TYPE_CODE) answer = true;
        ra_set_container_at_index(&r->high_low_container, i, c1,
                                  typecode_after);
    }
    return answer;
}

size_t roaring_bitmap_shrink_to_fit(roaring_bitmap_t *r) {
    size_t answer = 0;
    for (int i = 0; i < r->high_low_container.size; i++) {
        uint8_t typecode_original;
        void *c = ra_get_container_at_index(&r->high_low_container, i,
                                            &typecode_original);
        answer += container_shrink_to_fit(c, typecode_original);
    }
    answer += ra_shrink_to_fit(&r->high_low_container);
    return answer;
}

/**
 *  Remove run-length encoding even when it is more space efficient
 *  return whether a change was applied
 */
bool roaring_bitmap_remove_run_compression(roaring_bitmap_t *r) {
    bool answer = false;
    for (int i = 0; i < r->high_low_container.size; i++) {
        uint8_t typecode_original, typecode_after;
        void *c = ra_get_container_at_index(&r->high_low_container, i,
                                            &typecode_original);
        if (get_container_type(c, typecode_original) ==
            RUN_CONTAINER_TYPE_CODE) {
            answer = true;
            if (typecode_original == SHARED_CONTAINER_TYPE_CODE) {
                run_container_t *truec =
                    (run_container_t *)((shared_container_t *)c)->container;
                int32_t card = run_container_cardinality(truec);
                void *c1 = convert_to_bitset_or_array_container(
                    truec, card, &typecode_after);
                shared_container_free((shared_container_t *)c);
                ra_set_container_at_index(&r->high_low_container, i, c1,
                                          typecode_after);

            } else {
                int32_t card = run_container_cardinality((run_container_t *)c);
                void *c1 = convert_to_bitset_or_array_container(
                    (run_container_t *)c, card, &typecode_after);
                ra_set_container_at_index(&r->high_low_container, i, c1,
                                          typecode_after);
            }
        }
    }
    return answer;
}

size_t roaring_bitmap_serialize(const roaring_bitmap_t *ra, char *buf) {
    size_t portablesize = roaring_bitmap_portable_size_in_bytes(ra);
    uint64_t cardinality = roaring_bitmap_get_cardinality(ra);
    size_t sizeasarray = cardinality * sizeof(uint32_t) + sizeof(uint32_t);
    if (portablesize < sizeasarray) {
        buf[0] = SERIALIZATION_CONTAINER;
        return roaring_bitmap_portable_serialize(ra, buf + 1) + 1;
    } else {
        buf[0] = SERIALIZATION_ARRAY_UINT32;
        memcpy(buf + 1, &cardinality, sizeof(uint32_t));
        roaring_bitmap_to_uint32_array(
            ra, (uint32_t *)(buf + 1 + sizeof(uint32_t)));
        return 1 + sizeasarray;
    }
}

size_t roaring_bitmap_size_in_bytes(const roaring_bitmap_t *ra) {
    size_t portablesize = roaring_bitmap_portable_size_in_bytes(ra);
    size_t sizeasarray = roaring_bitmap_get_cardinality(ra) * sizeof(uint32_t) +
                         sizeof(uint32_t);
    return portablesize < sizeasarray ? portablesize + 1 : sizeasarray + 1;
}

size_t roaring_bitmap_portable_size_in_bytes(const roaring_bitmap_t *ra) {
    return ra_portable_size_in_bytes(&ra->high_low_container);
}

roaring_bitmap_t *roaring_bitmap_portable_deserialize(const char *buf) {
    roaring_bitmap_t *ans =
        (roaring_bitmap_t *)malloc(sizeof(roaring_bitmap_t));
    if (ans == NULL) {
        return NULL;
    }
    bool is_ok = ra_portable_deserialize(&ans->high_low_container, buf);
    ans->copy_on_write = false;
    if (!is_ok) {
        roaring_bitmap_free(ans);
        return NULL;
    }
    return ans;
}

size_t roaring_bitmap_portable_serialize(const roaring_bitmap_t *ra,
                                         char *buf) {
    return ra_portable_serialize(&ra->high_low_container, buf);
}

roaring_bitmap_t *roaring_bitmap_deserialize(const void *buf) {
    const char *bufaschar = (const char *)buf;
    if (*(const unsigned char *)buf == SERIALIZATION_ARRAY_UINT32) {
        /* This looks like a compressed set of uint32_t elements */
        uint32_t card;
        memcpy(&card, bufaschar + 1, sizeof(uint32_t));
        const uint32_t *elems =
            (const uint32_t *)(bufaschar + 1 + sizeof(uint32_t));

        return roaring_bitmap_of_ptr(card, elems);
    } else if (bufaschar[0] == SERIALIZATION_CONTAINER) {
        return roaring_bitmap_portable_deserialize(bufaschar + 1);
    } else
        return (NULL);
}

bool roaring_iterate(const roaring_bitmap_t *ra, roaring_iterator iterator,
                     void *ptr) {
    for (int i = 0; i < ra->high_low_container.size; ++i)
        if (!container_iterate(ra->high_low_container.containers[i],
                               ra->high_low_container.typecodes[i],
                               ((uint32_t)ra->high_low_container.keys[i]) << 16,
                               iterator, ptr)) {
            return false;
        }
    return true;
}

bool roaring_iterate64(const roaring_bitmap_t *ra, roaring_iterator64 iterator,
                       uint64_t high_bits, void *ptr) {
    for (int i = 0; i < ra->high_low_container.size; ++i)
        if (!container_iterate64(
                ra->high_low_container.containers[i],
                ra->high_low_container.typecodes[i],
                ((uint32_t)ra->high_low_container.keys[i]) << 16, iterator,
                high_bits, ptr)) {
            return false;
        }
    return true;
}

/****
* begin roaring_uint32_iterator_t
*****/

static bool loadfirstvalue(roaring_uint32_iterator_t *newit) {
    newit->in_container_index = 0;
    newit->run_index = 0;
    newit->current_value = 0;
    if (newit->container_index >=
        newit->parent->high_low_container.size) {  // otherwise nothing
        newit->current_value = UINT32_MAX;
        return (newit->has_value = false);
    }
    // assume not empty
    newit->has_value = true;
    // we precompute container, typecode and highbits so that successive
    // iterators do not have to grab them from odd memory locations
    // and have to worry about the (easily predicted) container_unwrap_shared
    // call.
    newit->container =
        newit->parent->high_low_container.containers[newit->container_index];
    newit->typecode =
        newit->parent->high_low_container.typecodes[newit->container_index];
    newit->highbits =
        ((uint32_t)
             newit->parent->high_low_container.keys[newit->container_index])
        << 16;
    newit->container =
        container_unwrap_shared(newit->container, &(newit->typecode));
    uint32_t wordindex;
    uint64_t word;  // used for bitsets
    switch (newit->typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            wordindex = 0;
            while ((word = ((const bitset_container_t *)(newit->container))
                               ->array[wordindex]) == 0)
                wordindex++;  // advance
            // here "word" is non-zero
            newit->in_container_index = wordindex * 64 + __builtin_ctzll(word);
            newit->current_value = newit->highbits | newit->in_container_index;
            break;
        case ARRAY_CONTAINER_TYPE_CODE:
            newit->current_value =
                newit->highbits |
                ((const array_container_t *)(newit->container))->array[0];
            break;
        case RUN_CONTAINER_TYPE_CODE:
            newit->current_value =
                newit->highbits |
                (((const run_container_t *)(newit->container))->runs[0].value);
            newit->in_run_index =
                newit->current_value +
                (((const run_container_t *)(newit->container))->runs[0].length);
            break;
        default:
            // if this ever happens, bug!
            assert(false);
    }  // switch (typecode)
    return true;
}

void roaring_init_iterator(const roaring_bitmap_t *ra,
                           roaring_uint32_iterator_t *newit) {
    newit->parent = ra;
    newit->container_index = 0;
    newit->has_value = loadfirstvalue(newit);
}

roaring_uint32_iterator_t *roaring_create_iterator(const roaring_bitmap_t *ra) {
    roaring_uint32_iterator_t *newit =
        (roaring_uint32_iterator_t *)malloc(sizeof(roaring_uint32_iterator_t));
    if (newit == NULL) return NULL;
    roaring_init_iterator(ra, newit);
    return newit;
}

roaring_uint32_iterator_t *roaring_copy_uint32_iterator(
    const roaring_uint32_iterator_t *it) {
    roaring_uint32_iterator_t *newit =
        (roaring_uint32_iterator_t *)malloc(sizeof(roaring_uint32_iterator_t));
    memcpy(newit, it, sizeof(roaring_uint32_iterator_t));
    return newit;
}

bool roaring_advance_uint32_iterator(roaring_uint32_iterator_t *it) {
    if (it->container_index >= it->parent->high_low_container.size) {
        return false;
    }
    uint32_t wordindex;  // used for bitsets
    uint64_t word;       // used for bitsets
    switch (it->typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            it->in_container_index++;
            wordindex = it->in_container_index / 64;
            if (wordindex >= BITSET_CONTAINER_SIZE_IN_WORDS) break;
            word = ((const bitset_container_t *)(it->container))
                       ->array[wordindex] &
                   (UINT64_MAX << (it->in_container_index % 64));
            // next part could be optimized/simplified
            while ((word == 0) &&
                   (wordindex + 1 < BITSET_CONTAINER_SIZE_IN_WORDS)) {
                wordindex++;
                word = ((const bitset_container_t *)(it->container))
                           ->array[wordindex];
            }
            if (word != 0) {
                it->in_container_index = wordindex * 64 + __builtin_ctzll(word);
                it->current_value = it->highbits | it->in_container_index;
                return true;
            }
            break;
        case ARRAY_CONTAINER_TYPE_CODE:
            it->in_container_index++;
            if (it->in_container_index <
                ((const array_container_t *)(it->container))->cardinality) {
                it->current_value = it->highbits |
                                    ((const array_container_t *)(it->container))
                                        ->array[it->in_container_index];
                return true;
            }
            break;
        case RUN_CONTAINER_TYPE_CODE:
            it->current_value++;
            if (it->current_value <= it->in_run_index) {
                return true;
            }
            it->run_index++;
            if (it->run_index <
                ((const run_container_t *)(it->container))->n_runs) {
                it->current_value =
                    it->highbits | (((const run_container_t *)(it->container))
                                        ->runs[it->run_index]
                                        .value);
                it->in_run_index = it->current_value +
                                   ((const run_container_t *)(it->container))
                                       ->runs[it->run_index]
                                       .length;
                return true;
            }
            break;
        default:
            // if this ever happens, bug!
            assert(false);
    }  // switch (typecode)
    // moving to next container
    it->container_index++;
    it->has_value = loadfirstvalue(it);
    return it->has_value;
}

void roaring_free_uint32_iterator(roaring_uint32_iterator_t *it) { free(it); }

/****
* end of roaring_uint32_iterator_t
*****/

bool roaring_bitmap_equals(const roaring_bitmap_t *ra1,
                           const roaring_bitmap_t *ra2) {
    if (ra1->high_low_container.size != ra2->high_low_container.size) {
        return false;
    }
    for (int i = 0; i < ra1->high_low_container.size; ++i) {
        if (ra1->high_low_container.keys[i] !=
            ra2->high_low_container.keys[i]) {
            return false;
        }
    }
    for (int i = 0; i < ra1->high_low_container.size; ++i) {
        bool areequal = container_equals(ra1->high_low_container.containers[i],
                                         ra1->high_low_container.typecodes[i],
                                         ra2->high_low_container.containers[i],
                                         ra2->high_low_container.typecodes[i]);
        if (!areequal) {
            return false;
        }
    }
    return true;
}

bool roaring_bitmap_is_subset(const roaring_bitmap_t *ra1,
                              const roaring_bitmap_t *ra2) {
    const int length1 = ra1->high_low_container.size,
              length2 = ra2->high_low_container.size;

    int pos1 = 0, pos2 = 0;

    while (pos1 < length1 && pos2 < length2) {
        const uint16_t s1 = ra_get_key_at_index(&ra1->high_low_container, pos1);
        const uint16_t s2 = ra_get_key_at_index(&ra2->high_low_container, pos2);

        if (s1 == s2) {
            uint8_t container_type_1, container_type_2;
            void *c1 = ra_get_container_at_index(&ra1->high_low_container, pos1,
                                                 &container_type_1);
            void *c2 = ra_get_container_at_index(&ra2->high_low_container, pos2,
                                                 &container_type_2);
            bool subset =
                container_is_subset(c1, container_type_1, c2, container_type_2);
            if (!subset) return false;
            ++pos1;
            ++pos2;
        } else if (s1 < s2) {  // s1 < s2
            return false;
        } else {  // s1 > s2
            pos2 = ra_advance_until(&ra2->high_low_container, s1, pos2);
        }
    }
    if (pos1 == length1)
        return true;
    else
        return false;
}

static void insert_flipped_container(roaring_array_t *ans_arr,
                                     const roaring_array_t *x1_arr, uint16_t hb,
                                     uint16_t lb_start, uint16_t lb_end) {
    const int i = ra_get_index(x1_arr, hb);
    const int j = ra_get_index(ans_arr, hb);
    uint8_t ctype_in, ctype_out;
    void *flipped_container = NULL;
    if (i >= 0) {
        void *container_to_flip =
            ra_get_container_at_index(x1_arr, i, &ctype_in);
        flipped_container =
            container_not_range(container_to_flip, ctype_in, (uint32_t)lb_start,
                                (uint32_t)(lb_end + 1), &ctype_out);

        if (container_get_cardinality(flipped_container, ctype_out))
            ra_insert_new_key_value_at(ans_arr, -j - 1, hb, flipped_container,
                                       ctype_out);
        else {
            container_free(flipped_container, ctype_out);
        }
    } else {
        flipped_container = container_range_of_ones(
            (uint32_t)lb_start, (uint32_t)(lb_end + 1), &ctype_out);
        ra_insert_new_key_value_at(ans_arr, -j - 1, hb, flipped_container,
                                   ctype_out);
    }
}

static void inplace_flip_container(roaring_array_t *x1_arr, uint16_t hb,
                                   uint16_t lb_start, uint16_t lb_end) {
    const int i = ra_get_index(x1_arr, hb);
    uint8_t ctype_in, ctype_out;
    void *flipped_container = NULL;
    if (i >= 0) {
        void *container_to_flip =
            ra_get_container_at_index(x1_arr, i, &ctype_in);
        flipped_container = container_inot_range(
            container_to_flip, ctype_in, (uint32_t)lb_start,
            (uint32_t)(lb_end + 1), &ctype_out);
        // if a new container was created, the old one was already freed
        if (container_get_cardinality(flipped_container, ctype_out)) {
            ra_set_container_at_index(x1_arr, i, flipped_container, ctype_out);
        } else {
            container_free(flipped_container, ctype_out);
            ra_remove_at_index(x1_arr, i);
        }

    } else {
        flipped_container = container_range_of_ones(
            (uint32_t)lb_start, (uint32_t)(lb_end + 1), &ctype_out);
        ra_insert_new_key_value_at(x1_arr, -i - 1, hb, flipped_container,
                                   ctype_out);
    }
}

static void insert_fully_flipped_container(roaring_array_t *ans_arr,
                                           const roaring_array_t *x1_arr,
                                           uint16_t hb) {
    const int i = ra_get_index(x1_arr, hb);
    const int j = ra_get_index(ans_arr, hb);
    uint8_t ctype_in, ctype_out;
    void *flipped_container = NULL;
    if (i >= 0) {
        void *container_to_flip =
            ra_get_container_at_index(x1_arr, i, &ctype_in);
        flipped_container =
            container_not(container_to_flip, ctype_in, &ctype_out);
        if (container_get_cardinality(flipped_container, ctype_out))
            ra_insert_new_key_value_at(ans_arr, -j - 1, hb, flipped_container,
                                       ctype_out);
        else {
            container_free(flipped_container, ctype_out);
        }
    } else {
        flipped_container = container_range_of_ones(0U, 0x10000U, &ctype_out);
        ra_insert_new_key_value_at(ans_arr, -j - 1, hb, flipped_container,
                                   ctype_out);
    }
}

static void inplace_fully_flip_container(roaring_array_t *x1_arr, uint16_t hb) {
    const int i = ra_get_index(x1_arr, hb);
    uint8_t ctype_in, ctype_out;
    void *flipped_container = NULL;
    if (i >= 0) {
        void *container_to_flip =
            ra_get_container_at_index(x1_arr, i, &ctype_in);
        flipped_container =
            container_inot(container_to_flip, ctype_in, &ctype_out);

        if (container_get_cardinality(flipped_container, ctype_out)) {
            ra_set_container_at_index(x1_arr, i, flipped_container, ctype_out);
        } else {
            container_free(flipped_container, ctype_out);
            ra_remove_at_index(x1_arr, i);
        }

    } else {
        flipped_container = container_range_of_ones(0U, 0x10000U, &ctype_out);
        ra_insert_new_key_value_at(x1_arr, -i - 1, hb, flipped_container,
                                   ctype_out);
    }
}

roaring_bitmap_t *roaring_bitmap_flip(const roaring_bitmap_t *x1,
                                      uint64_t range_start,
                                      uint64_t range_end) {
    if (range_start >= range_end) {
        return roaring_bitmap_copy(x1);
    }

    roaring_bitmap_t *ans = roaring_bitmap_create();
    ans->copy_on_write = x1->copy_on_write;

    uint16_t hb_start = (uint16_t)(range_start >> 16);
    const uint16_t lb_start = (uint16_t)range_start;  // & 0xFFFF;
    uint16_t hb_end = (uint16_t)((range_end - 1) >> 16);
    const uint16_t lb_end = (uint16_t)(range_end - 1);  // & 0xFFFF;

    ra_append_copies_until(&ans->high_low_container, &x1->high_low_container,
                           hb_start, x1->copy_on_write);
    if (hb_start == hb_end) {
        insert_flipped_container(&ans->high_low_container,
                                 &x1->high_low_container, hb_start, lb_start,
                                 lb_end);
    } else {
        // start and end containers are distinct
        if (lb_start > 0) {
            // handle first (partial) container
            insert_flipped_container(&ans->high_low_container,
                                     &x1->high_low_container, hb_start,
                                     lb_start, 0xFFFF);
            ++hb_start;  // for the full containers.  Can't wrap.
        }

        if (lb_end != 0xFFFF) --hb_end;  // later we'll handle the partial block

        for (uint16_t hb = hb_start; hb <= hb_end; ++hb) {
            insert_fully_flipped_container(&ans->high_low_container,
                                           &x1->high_low_container, hb);
        }

        // handle a partial final container
        if (lb_end != 0xFFFF) {
            insert_flipped_container(&ans->high_low_container,
                                     &x1->high_low_container, hb_end + 1, 0,
                                     lb_end);
            ++hb_end;
        }
    }
    ra_append_copies_after(&ans->high_low_container, &x1->high_low_container,
                           hb_end, x1->copy_on_write);
    return ans;
}

void roaring_bitmap_flip_inplace(roaring_bitmap_t *x1, uint64_t range_start,
                                 uint64_t range_end) {
    if (range_start >= range_end) {
        return;  // empty range
    }

    uint16_t hb_start = (uint16_t)(range_start >> 16);
    const uint16_t lb_start = (uint16_t)range_start;
    uint16_t hb_end = (uint16_t)((range_end - 1) >> 16);
    const uint16_t lb_end = (uint16_t)(range_end - 1);

    if (hb_start == hb_end) {
        inplace_flip_container(&x1->high_low_container, hb_start, lb_start,
                               lb_end);
    } else {
        // start and end containers are distinct
        if (lb_start > 0) {
            // handle first (partial) container
            inplace_flip_container(&x1->high_low_container, hb_start, lb_start,
                                   0xFFFF);
            ++hb_start;  // for the full containers.  Can't wrap.
        }

        if (lb_end != 0xFFFF) --hb_end;

        for (uint16_t hb = hb_start; hb <= hb_end; ++hb) {
            inplace_fully_flip_container(&x1->high_low_container, hb);
        }

        // handle a partial final container
        if (lb_end != 0xFFFF) {
            inplace_flip_container(&x1->high_low_container, hb_end + 1, 0,
                                   lb_end);
            ++hb_end;
        }
    }
}

roaring_bitmap_t *roaring_bitmap_lazy_or(const roaring_bitmap_t *x1,
                                         const roaring_bitmap_t *x2,
                                         const bool bitsetconversion) {
    uint8_t container_result_type = 0;
    const int length1 = x1->high_low_container.size,
              length2 = x2->high_low_container.size;
    if (0 == length1) {
        return roaring_bitmap_copy(x2);
    }
    if (0 == length2) {
        return roaring_bitmap_copy(x1);
    }
    roaring_bitmap_t *answer =
        roaring_bitmap_create_with_capacity(length1 + length2);
    answer->copy_on_write = x1->copy_on_write && x2->copy_on_write;
    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
    uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
    while (true) {
        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            void *c;
            if (bitsetconversion && (get_container_type(c1, container_type_1) !=
                                     BITSET_CONTAINER_TYPE_CODE) &&
                (get_container_type(c2, container_type_2) !=
                 BITSET_CONTAINER_TYPE_CODE)) {
                void *newc1 =
                    container_mutable_unwrap_shared(c1, &container_type_1);
                newc1 = container_to_bitset(newc1, container_type_1);
                container_type_1 = BITSET_CONTAINER_TYPE_CODE;
                c = container_lazy_ior(newc1, container_type_1, c2,
                                       container_type_2,
                                       &container_result_type);
                if (c != newc1) {  // should not happen
                    container_free(newc1, container_type_1);
                }
            } else {
                c = container_lazy_or(c1, container_type_1, c2,
                                      container_type_2, &container_result_type);
            }
            // since we assume that the initial containers are non-empty,
            // the
            // result here
            // can only be non-empty
            ra_append(&answer->high_low_container, s1, c,
                      container_result_type);
            ++pos1;
            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        } else if (s1 < s2) {  // s1 < s2
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            c1 =
                get_copy_of_container(c1, &container_type_1, x1->copy_on_write);
            if (x1->copy_on_write) {
                ra_set_container_at_index(&x1->high_low_container, pos1, c1,
                                          container_type_1);
            }
            ra_append(&answer->high_low_container, s1, c1, container_type_1);
            pos1++;
            if (pos1 == length1) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);

        } else {  // s1 > s2
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            c2 =
                get_copy_of_container(c2, &container_type_2, x2->copy_on_write);
            if (x2->copy_on_write) {
                ra_set_container_at_index(&x2->high_low_container, pos2, c2,
                                          container_type_2);
            }
            ra_append(&answer->high_low_container, s2, c2, container_type_2);
            pos2++;
            if (pos2 == length2) break;
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
        }
    }
    if (pos1 == length1) {
        ra_append_copy_range(&answer->high_low_container,
                             &x2->high_low_container, pos2, length2,
                             x2->copy_on_write);
    } else if (pos2 == length2) {
        ra_append_copy_range(&answer->high_low_container,
                             &x1->high_low_container, pos1, length1,
                             x1->copy_on_write);
    }
    return answer;
}

void roaring_bitmap_lazy_or_inplace(roaring_bitmap_t *x1,
                                    const roaring_bitmap_t *x2,
                                    const bool bitsetconversion) {
    uint8_t container_result_type = 0;
    int length1 = x1->high_low_container.size;
    const int length2 = x2->high_low_container.size;

    if (0 == length2) return;

    if (0 == length1) {
        roaring_bitmap_overwrite(x1, x2);
        return;
    }
    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
    uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
    while (true) {
        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            if (!container_is_full(c1, container_type_1)) {
                if ((bitsetconversion == false) ||
                    (get_container_type(c1, container_type_1) ==
                     BITSET_CONTAINER_TYPE_CODE)) {
                    c1 = get_writable_copy_if_shared(c1, &container_type_1);
                } else {
                    // convert to bitset
                    void *oldc1 = c1;
                    uint8_t oldt1 = container_type_1;
                    c1 = container_mutable_unwrap_shared(c1, &container_type_1);
                    c1 = container_to_bitset(c1, container_type_1);
                    container_free(oldc1, oldt1);
                    container_type_1 = BITSET_CONTAINER_TYPE_CODE;
                }

                void *c2 = ra_get_container_at_index(&x2->high_low_container,
                                                     pos2, &container_type_2);
                void *c = container_lazy_ior(c1, container_type_1, c2,
                                             container_type_2,
                                             &container_result_type);
                if (c !=
                    c1) {  // in this instance a new container was created, and
                           // we need to free the old one
                    container_free(c1, container_type_1);
                }

                ra_set_container_at_index(&x1->high_low_container, pos1, c,
                                          container_result_type);
            }
            ++pos1;
            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        } else if (s1 < s2) {  // s1 < s2
            pos1++;
            if (pos1 == length1) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);

        } else {  // s1 > s2
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            // void *c2_clone = container_clone(c2, container_type_2);
            c2 =
                get_copy_of_container(c2, &container_type_2, x2->copy_on_write);
            if (x2->copy_on_write) {
                ra_set_container_at_index(&x2->high_low_container, pos2, c2,
                                          container_type_2);
            }
            ra_insert_new_key_value_at(&x1->high_low_container, pos1, s2, c2,
                                       container_type_2);
            pos1++;
            length1++;
            pos2++;
            if (pos2 == length2) break;
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
        }
    }
    if (pos1 == length1) {
        ra_append_copy_range(&x1->high_low_container, &x2->high_low_container,
                             pos2, length2, x2->copy_on_write);
    }
}

roaring_bitmap_t *roaring_bitmap_lazy_xor(const roaring_bitmap_t *x1,
                                          const roaring_bitmap_t *x2) {
    uint8_t container_result_type = 0;
    const int length1 = x1->high_low_container.size,
              length2 = x2->high_low_container.size;
    if (0 == length1) {
        return roaring_bitmap_copy(x2);
    }
    if (0 == length2) {
        return roaring_bitmap_copy(x1);
    }
    roaring_bitmap_t *answer =
        roaring_bitmap_create_with_capacity(length1 + length2);
    answer->copy_on_write = x1->copy_on_write && x2->copy_on_write;
    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
    uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
    while (true) {
        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            void *c =
                container_lazy_xor(c1, container_type_1, c2, container_type_2,
                                   &container_result_type);

            if (container_nonzero_cardinality(c, container_result_type)) {
                ra_append(&answer->high_low_container, s1, c,
                          container_result_type);
            } else {
                container_free(c, container_result_type);
            }

            ++pos1;
            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        } else if (s1 < s2) {  // s1 < s2
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            c1 =
                get_copy_of_container(c1, &container_type_1, x1->copy_on_write);
            if (x1->copy_on_write) {
                ra_set_container_at_index(&x1->high_low_container, pos1, c1,
                                          container_type_1);
            }
            ra_append(&answer->high_low_container, s1, c1, container_type_1);
            pos1++;
            if (pos1 == length1) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);

        } else {  // s1 > s2
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            c2 =
                get_copy_of_container(c2, &container_type_2, x2->copy_on_write);
            if (x2->copy_on_write) {
                ra_set_container_at_index(&x2->high_low_container, pos2, c2,
                                          container_type_2);
            }
            ra_append(&answer->high_low_container, s2, c2, container_type_2);
            pos2++;
            if (pos2 == length2) break;
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
        }
    }
    if (pos1 == length1) {
        ra_append_copy_range(&answer->high_low_container,
                             &x2->high_low_container, pos2, length2,
                             x2->copy_on_write);
    } else if (pos2 == length2) {
        ra_append_copy_range(&answer->high_low_container,
                             &x1->high_low_container, pos1, length1,
                             x1->copy_on_write);
    }
    return answer;
}

void roaring_bitmap_lazy_xor_inplace(roaring_bitmap_t *x1,
                                     const roaring_bitmap_t *x2) {
    assert(x1 != x2);
    uint8_t container_result_type = 0;
    int length1 = x1->high_low_container.size;
    const int length2 = x2->high_low_container.size;

    if (0 == length2) return;

    if (0 == length1) {
        roaring_bitmap_overwrite(x1, x2);
        return;
    }
    int pos1 = 0, pos2 = 0;
    uint8_t container_type_1, container_type_2;
    uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
    uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
    while (true) {
        if (s1 == s2) {
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            c1 = get_writable_copy_if_shared(c1, &container_type_1);
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            void *c =
                container_lazy_ixor(c1, container_type_1, c2, container_type_2,
                                    &container_result_type);
            if (container_nonzero_cardinality(c, container_result_type)) {
                ra_set_container_at_index(&x1->high_low_container, pos1, c,
                                          container_result_type);
                ++pos1;
            } else {
                container_free(c, container_result_type);
                ra_remove_at_index(&x1->high_low_container, pos1);
                --length1;
            }
            ++pos2;
            if (pos1 == length1) break;
            if (pos2 == length2) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        } else if (s1 < s2) {  // s1 < s2
            pos1++;
            if (pos1 == length1) break;
            s1 = ra_get_key_at_index(&x1->high_low_container, pos1);

        } else {  // s1 > s2
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            // void *c2_clone = container_clone(c2, container_type_2);
            c2 =
                get_copy_of_container(c2, &container_type_2, x2->copy_on_write);
            if (x2->copy_on_write) {
                ra_set_container_at_index(&x2->high_low_container, pos2, c2,
                                          container_type_2);
            }
            ra_insert_new_key_value_at(&x1->high_low_container, pos1, s2, c2,
                                       container_type_2);
            pos1++;
            length1++;
            pos2++;
            if (pos2 == length2) break;
            s2 = ra_get_key_at_index(&x2->high_low_container, pos2);
        }
    }
    if (pos1 == length1) {
        ra_append_copy_range(&x1->high_low_container, &x2->high_low_container,
                             pos2, length2, x2->copy_on_write);
    }
}

void roaring_bitmap_repair_after_lazy(roaring_bitmap_t *ra) {
    for (int i = 0; i < ra->high_low_container.size; ++i) {
        const uint8_t original_typecode = ra->high_low_container.typecodes[i];
        void *container = ra->high_low_container.containers[i];
        uint8_t new_typecode = original_typecode;
        void *newcontainer =
            container_repair_after_lazy(container, &new_typecode);
        ra->high_low_container.containers[i] = newcontainer;
        ra->high_low_container.typecodes[i] = new_typecode;
    }
}

/**
* roaring_bitmap_rank returns the number of integers that are smaller or equal
* to x.
*/
uint64_t roaring_bitmap_rank(const roaring_bitmap_t *bm, uint32_t x) {
    uint64_t size = 0;
    uint32_t xhigh = x >> 16;
    for (int i = 0; i < bm->high_low_container.size; i++) {
        uint32_t key = bm->high_low_container.keys[i];
        if (xhigh > key) {
            size +=
                container_get_cardinality(bm->high_low_container.containers[i],
                                          bm->high_low_container.typecodes[i]);
        } else if (xhigh == key) {
            return size + container_rank(bm->high_low_container.containers[i],
                                         bm->high_low_container.typecodes[i],
                                         x & 0xFFFF);
        } else {
            return size;
        }
    }
    return size;
}

/**
* roaring_bitmap_smallest returns the smallest value in the set.
* Returns UINT32_MAX if the set is empty.
*/
uint32_t roaring_bitmap_minimum(const roaring_bitmap_t *bm) {
    if (bm->high_low_container.size > 0) {
        void *container = bm->high_low_container.containers[0];
        uint8_t typecode = bm->high_low_container.typecodes[0];
        uint32_t key = bm->high_low_container.keys[0];
        uint32_t lowvalue = container_minimum(container, typecode);
        return lowvalue | (key << 16);
    }
    return UINT32_MAX;
}

/**
* roaring_bitmap_smallest returns the greatest value in the set.
* Returns 0 if the set is empty.
*/
uint32_t roaring_bitmap_maximum(const roaring_bitmap_t *bm) {
    if (bm->high_low_container.size > 0) {
        void *container =
            bm->high_low_container.containers[bm->high_low_container.size - 1];
        uint8_t typecode =
            bm->high_low_container.typecodes[bm->high_low_container.size - 1];
        uint32_t key =
            bm->high_low_container.keys[bm->high_low_container.size - 1];
        uint32_t lowvalue = container_maximum(container, typecode);
        return lowvalue | (key << 16);
    }
    return 0;
}

bool roaring_bitmap_select(const roaring_bitmap_t *bm, uint32_t rank,
                           uint32_t *element) {
    void *container;
    uint8_t typecode;
    uint16_t key;
    uint32_t start_rank = 0;
    int i = 0;
    bool valid = false;
    while (!valid && i < bm->high_low_container.size) {
        container = bm->high_low_container.containers[i];
        typecode = bm->high_low_container.typecodes[i];
        valid =
            container_select(container, typecode, &start_rank, rank, element);
        i++;
    }

    if (valid) {
        key = bm->high_low_container.keys[i - 1];
        *element |= (key << 16);
        return true;
    } else
        return false;
}

bool roaring_bitmap_intersect(const roaring_bitmap_t *x1,
                                     const roaring_bitmap_t *x2) {
    const int length1 = x1->high_low_container.size,
              length2 = x2->high_low_container.size;
    uint64_t answer = 0;
    int pos1 = 0, pos2 = 0;

    while (pos1 < length1 && pos2 < length2) {
        const uint16_t s1 = ra_get_key_at_index(& x1->high_low_container, pos1);
        const uint16_t s2 = ra_get_key_at_index(& x2->high_low_container, pos2);

        if (s1 == s2) {
            uint8_t container_type_1, container_type_2;
            void *c1 = ra_get_container_at_index(& x1->high_low_container, pos1,
                                                 &container_type_1);
            void *c2 = ra_get_container_at_index(& x2->high_low_container, pos2,
                                                 &container_type_2);
            if( container_intersect(c1, container_type_1, c2, container_type_2) ) return true;
            ++pos1;
            ++pos2;
        } else if (s1 < s2) {  // s1 < s2
            pos1 = ra_advance_until(& x1->high_low_container, s2, pos1);
        } else {  // s1 > s2
            pos2 = ra_advance_until(& x2->high_low_container, s1, pos2);
        }
    }
    return answer;
}


uint64_t roaring_bitmap_and_cardinality(const roaring_bitmap_t *x1,
                                        const roaring_bitmap_t *x2) {
    const int length1 = x1->high_low_container.size,
              length2 = x2->high_low_container.size;
    uint64_t answer = 0;
    int pos1 = 0, pos2 = 0;

    while (pos1 < length1 && pos2 < length2) {
        const uint16_t s1 = ra_get_key_at_index(&x1->high_low_container, pos1);
        const uint16_t s2 = ra_get_key_at_index(&x2->high_low_container, pos2);

        if (s1 == s2) {
            uint8_t container_type_1, container_type_2;
            void *c1 = ra_get_container_at_index(&x1->high_low_container, pos1,
                                                 &container_type_1);
            void *c2 = ra_get_container_at_index(&x2->high_low_container, pos2,
                                                 &container_type_2);
            answer += container_and_cardinality(c1, container_type_1, c2,
                                                container_type_2);
            ++pos1;
            ++pos2;
        } else if (s1 < s2) {  // s1 < s2
            pos1 = ra_advance_until(&x1->high_low_container, s2, pos1);
        } else {  // s1 > s2
            pos2 = ra_advance_until(&x2->high_low_container, s1, pos2);
        }
    }
    return answer;
}

double roaring_bitmap_jaccard_index(const roaring_bitmap_t *x1,
                                    const roaring_bitmap_t *x2) {
    const uint64_t c1 = roaring_bitmap_get_cardinality(x1);
    const uint64_t c2 = roaring_bitmap_get_cardinality(x2);
    const uint64_t inter = roaring_bitmap_and_cardinality(x1, x2);
    return (double)inter / (double)(c1 + c2 - inter);
}

uint64_t roaring_bitmap_or_cardinality(const roaring_bitmap_t *x1,
                                       const roaring_bitmap_t *x2) {
    const uint64_t c1 = roaring_bitmap_get_cardinality(x1);
    const uint64_t c2 = roaring_bitmap_get_cardinality(x2);
    const uint64_t inter = roaring_bitmap_and_cardinality(x1, x2);
    return c1 + c2 - inter;
}

uint64_t roaring_bitmap_andnot_cardinality(const roaring_bitmap_t *x1,
                                           const roaring_bitmap_t *x2) {
    const uint64_t c1 = roaring_bitmap_get_cardinality(x1);
    const uint64_t inter = roaring_bitmap_and_cardinality(x1, x2);
    return c1 - inter;
}

uint64_t roaring_bitmap_xor_cardinality(const roaring_bitmap_t *x1,
                                        const roaring_bitmap_t *x2) {
    const uint64_t c1 = roaring_bitmap_get_cardinality(x1);
    const uint64_t c2 = roaring_bitmap_get_cardinality(x2);
    const uint64_t inter = roaring_bitmap_and_cardinality(x1, x2);
    return c1 + c2 - 2 * inter;
}
