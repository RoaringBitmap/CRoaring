#include <assert.h>
#include <roaring/art/art.h>
#include <roaring/containers/containers.h>
#include <roaring/portability.h>
#include <roaring/roaring64.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
using namespace ::roaring::internal;

extern "C" {
namespace roaring {
namespace api {
#endif

// TODO: Iteration-based functions like roaring64_bitmap_intersect_with_range.
// TODO: Copy on write.
// TODO: Serialization.
// TODO: Error on failed allocation.

typedef struct roaring64_bitmap_s {
    art_t art;
    uint8_t flags;
} roaring64_bitmap_t;

// Leaf type of the ART used to keep the high 48 bits of each entry.
typedef struct roaring64_leaf_s {
    art_val_t _pad;
    uint8_t typecode;
    container_t *container;
} roaring64_leaf_t;

// Alias to make it easier to work with, since it's an internal-only type
// anyway.
typedef struct roaring64_leaf_s leaf_t;

// Iterator struct to hold iteration state.
typedef struct roaring64_iterator_s {
    const roaring64_bitmap_t *parent;
    art_iterator_t art_it;
    roaring_container_iterator_t container_it;
    uint64_t high48;  // Key that art_it points to.

    uint64_t value;
    bool has_value;
} roaring64_iterator_t;

// Splits the given uint64 key into high 48 bit and low 16 bit components.
// Expects high48_out to be of length ART_KEY_BYTES.
static inline uint16_t split_key(uint64_t key, uint8_t high48_out[]) {
    uint64_t tmp = croaring_htobe64(key);
    memcpy(high48_out, (uint8_t *)(&tmp), ART_KEY_BYTES);
    return (uint16_t)key;
}

// Recombines the high 48 bit and low 16 bit components into a uint64 key.
// Expects high48_out to be of length ART_KEY_BYTES.
static inline uint64_t combine_key(const uint8_t high48[], uint16_t low16) {
    uint64_t result = 0;
    memcpy((uint8_t *)(&result), high48, ART_KEY_BYTES);
    return croaring_be64toh(result) | low16;
}

static inline uint64_t minimum(uint64_t a, uint64_t b) {
    return (a < b) ? a : b;
}

static inline leaf_t *create_leaf(container_t *container, uint8_t typecode) {
    leaf_t *leaf = (leaf_t *)roaring_malloc(sizeof(leaf_t));
    leaf->container = container;
    leaf->typecode = typecode;
    return leaf;
}

static inline leaf_t *copy_leaf_container(const leaf_t *leaf) {
    leaf_t *result_leaf = (leaf_t *)roaring_malloc(sizeof(leaf_t));
    result_leaf->typecode = leaf->typecode;
    // get_copy_of_container modifies the typecode passed in.
    result_leaf->container = get_copy_of_container(
        leaf->container, &result_leaf->typecode, /*copy_on_write=*/false);
    return result_leaf;
}

static inline void free_leaf(leaf_t *leaf) { roaring_free(leaf); }

static inline int compare_high48(art_key_chunk_t key1[],
                                 art_key_chunk_t key2[]) {
    return art_compare_keys(key1, key2);
}

roaring64_bitmap_t *roaring64_bitmap_create(void) {
    roaring64_bitmap_t *r =
        (roaring64_bitmap_t *)roaring_malloc(sizeof(roaring64_bitmap_t));
    r->art.root = NULL;
    r->flags = 0;
    return r;
}

void roaring64_bitmap_free(roaring64_bitmap_t *r) {
    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    while (it.value != NULL) {
        leaf_t *leaf = (leaf_t *)it.value;
        container_free(leaf->container, leaf->typecode);
        free_leaf(leaf);
        art_iterator_next(&it);
    }
    art_free(&r->art);
    roaring_free(r);
}

roaring64_bitmap_t *roaring64_bitmap_copy(const roaring64_bitmap_t *r) {
    roaring64_bitmap_t *result = roaring64_bitmap_create();

    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    while (it.value != NULL) {
        leaf_t *leaf = (leaf_t *)it.value;
        uint8_t result_typecode = leaf->typecode;
        container_t *result_container = get_copy_of_container(
            leaf->container, &result_typecode, /*copy_on_write=*/false);
        leaf_t *result_leaf = create_leaf(result_container, result_typecode);
        art_insert(&result->art, it.key, (art_val_t *)result_leaf);
        art_iterator_next(&it);
    }
    return result;
}

roaring64_bitmap_t *roaring64_bitmap_from_range(uint64_t min, uint64_t max,
                                                uint64_t step) {
    if (step == 0 || max <= min) {
        return NULL;
    }
    roaring64_bitmap_t *r = roaring64_bitmap_create();
    if (step >= (1 << 16)) {
        // Only one value per container.
        for (uint64_t value = min; value < max; value += step) {
            roaring64_bitmap_add(r, value);
            if (value > UINT64_MAX - step) {
                break;
            }
        }
        return r;
    }
    do {
        uint64_t high_bits = min & 0xFFFFFFFFFFFF0000;
        uint16_t container_min = min & 0xFFFF;
        uint32_t container_max = (uint32_t)minimum(max - high_bits, 1 << 16);

        uint8_t typecode;
        container_t *container = container_from_range(
            &typecode, container_min, container_max, (uint16_t)step);

        uint8_t high48[ART_KEY_BYTES];
        split_key(min, high48);
        leaf_t *leaf = create_leaf(container, typecode);
        art_insert(&r->art, high48, (art_val_t *)leaf);

        uint64_t gap = container_max - container_min + step - 1;
        uint64_t increment = gap - (gap % step);
        if (min > UINT64_MAX - increment) {
            break;
        }
        min += increment;
    } while (min < max);
    return r;
}

roaring64_bitmap_t *roaring64_bitmap_of_ptr(size_t n_args,
                                            const uint64_t *vals) {
    roaring64_bitmap_t *r = roaring64_bitmap_create();
    roaring64_bitmap_add_many(r, n_args, vals);
    return r;
}

roaring64_bitmap_t *roaring64_bitmap_of(size_t n_args, ...) {
    roaring64_bitmap_t *r = roaring64_bitmap_create();
    roaring64_bulk_context_t context = {0};
    va_list ap;
    va_start(ap, n_args);
    for (size_t i = 0; i < n_args; i++) {
        uint64_t val = va_arg(ap, uint64_t);
        roaring64_bitmap_add_bulk(r, &context, val);
    }
    va_end(ap);
    return r;
}

static inline leaf_t *containerptr_roaring64_bitmap_add(roaring64_bitmap_t *r,
                                                        uint8_t *high48,
                                                        uint16_t low16,
                                                        leaf_t *leaf) {
    if (leaf != NULL) {
        uint8_t typecode2;
        container_t *container2 =
            container_add(leaf->container, low16, leaf->typecode, &typecode2);
        if (container2 != leaf->container) {
            container_free(leaf->container, leaf->typecode);
            leaf->container = container2;
            leaf->typecode = typecode2;
        }
        return leaf;
    } else {
        array_container_t *ac = array_container_create();
        uint8_t typecode;
        container_t *container =
            container_add(ac, low16, ARRAY_CONTAINER_TYPE, &typecode);
        assert(ac == container);
        leaf = create_leaf(container, typecode);
        art_insert(&r->art, high48, (art_val_t *)leaf);
        return leaf;
    }
}

void roaring64_bitmap_add(roaring64_bitmap_t *r, uint64_t val) {
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);
    leaf_t *leaf = (leaf_t *)art_find(&r->art, high48);
    containerptr_roaring64_bitmap_add(r, high48, low16, leaf);
}

bool roaring64_bitmap_add_checked(roaring64_bitmap_t *r, uint64_t val) {
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);
    leaf_t *leaf = (leaf_t *)art_find(&r->art, high48);

    int old_cardinality = 0;
    if (leaf != NULL) {
        old_cardinality =
            container_get_cardinality(leaf->container, leaf->typecode);
    }
    leaf = containerptr_roaring64_bitmap_add(r, high48, low16, leaf);
    int new_cardinality =
        container_get_cardinality(leaf->container, leaf->typecode);
    return old_cardinality != new_cardinality;
}

void roaring64_bitmap_add_bulk(roaring64_bitmap_t *r,
                               roaring64_bulk_context_t *context,
                               uint64_t val) {
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);
    if (context->leaf != NULL &&
        compare_high48(context->high_bytes, high48) == 0) {
        // We're at a container with the correct high bits.
        uint8_t typecode2;
        container_t *container2 =
            container_add(context->leaf->container, low16,
                          context->leaf->typecode, &typecode2);
        if (container2 != context->leaf->container) {
            container_free(context->leaf->container, context->leaf->typecode);
            context->leaf->container = container2;
            context->leaf->typecode = typecode2;
        }
    } else {
        // We're not positioned anywhere yet or the high bits of the key
        // differ.
        leaf_t *leaf = (leaf_t *)art_find(&r->art, high48);
        context->leaf =
            containerptr_roaring64_bitmap_add(r, high48, low16, leaf);
        memcpy(context->high_bytes, high48, ART_KEY_BYTES);
    }
}

void roaring64_bitmap_add_many(roaring64_bitmap_t *r, size_t n_args,
                               const uint64_t *vals) {
    if (n_args == 0) {
        return;
    }
    const uint64_t *end = vals + n_args;
    roaring64_bulk_context_t context = {0};
    for (const uint64_t *current_val = vals; current_val != end;
         current_val++) {
        roaring64_bitmap_add_bulk(r, &context, *current_val);
    }
}

static inline void add_range_closed_at(art_t *art, uint8_t *high48,
                                       uint16_t min, uint16_t max) {
    leaf_t *leaf = (leaf_t *)art_find(art, high48);
    if (leaf != NULL) {
        uint8_t typecode2;
        container_t *container2 = container_add_range(
            leaf->container, leaf->typecode, min, max, &typecode2);
        if (container2 != leaf->container) {
            container_free(leaf->container, leaf->typecode);
            leaf->container = container2;
            leaf->typecode = typecode2;
        }
        return;
    }
    uint8_t typecode;
    // container_add_range is inclusive, but `container_range_of_ones` is
    // exclusive.
    container_t *container = container_range_of_ones(min, max + 1, &typecode);
    leaf = create_leaf(container, typecode);
    art_insert(art, high48, (art_val_t *)leaf);
}

void roaring64_bitmap_add_range_closed(roaring64_bitmap_t *r, uint64_t min,
                                       uint64_t max) {
    if (min > max) {
        return;
    }

    art_t *art = &r->art;
    uint8_t min_high48[ART_KEY_BYTES];
    uint16_t min_low16 = split_key(min, min_high48);
    uint8_t max_high48[ART_KEY_BYTES];
    uint16_t max_low16 = split_key(max, max_high48);
    if (compare_high48(min_high48, max_high48) == 0) {
        // Only populate range within one container.
        add_range_closed_at(art, min_high48, min_low16, max_low16);
        return;
    }

    // Populate a range across containers. Fill intermediate containers
    // entirely.
    add_range_closed_at(art, min_high48, min_low16, 0xffff);
    uint64_t min_high_bits = min >> 16;
    uint64_t max_high_bits = max >> 16;
    for (uint64_t current = min_high_bits + 1; current < max_high_bits;
         ++current) {
        uint8_t current_high48[ART_KEY_BYTES];
        split_key(current << 16, current_high48);
        add_range_closed_at(art, current_high48, 0, 0xffff);
    }
    add_range_closed_at(art, max_high48, 0, max_low16);
}

bool roaring64_bitmap_contains(const roaring64_bitmap_t *r, uint64_t val) {
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);
    leaf_t *leaf = (leaf_t *)art_find(&r->art, high48);
    if (leaf != NULL) {
        return container_contains(leaf->container, low16, leaf->typecode);
    }
    return false;
}

bool roaring64_bitmap_contains_bulk(const roaring64_bitmap_t *r,
                                    roaring64_bulk_context_t *context,
                                    uint64_t val) {
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);

    if (context->leaf == NULL || context->high_bytes != high48) {
        // We're not positioned anywhere yet or the high bits of the key
        // differ.
        leaf_t *leaf = (leaf_t *)art_find(&r->art, high48);
        if (leaf == NULL) {
            return false;
        }
        context->leaf = leaf;
        memcpy(context->high_bytes, high48, ART_KEY_BYTES);
    }
    return container_contains(context->leaf->container, low16,
                              context->leaf->typecode);
}

bool roaring64_bitmap_select(const roaring64_bitmap_t *r, uint64_t rank,
                             uint64_t *element) {
    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    uint64_t start_rank = 0;
    while (it.value != NULL) {
        leaf_t *leaf = (leaf_t *)it.value;
        uint64_t cardinality =
            container_get_cardinality(leaf->container, leaf->typecode);
        if (start_rank + cardinality > rank) {
            uint32_t uint32_start = 0;
            uint32_t uint32_rank = rank - start_rank;
            uint32_t uint32_element = 0;
            if (container_select(leaf->container, leaf->typecode, &uint32_start,
                                 uint32_rank, &uint32_element)) {
                *element = combine_key(it.key, (uint16_t)uint32_element);
                return true;
            }
            return false;
        }
        start_rank += cardinality;
        art_iterator_next(&it);
    }
    return false;
}

uint64_t roaring64_bitmap_rank(const roaring64_bitmap_t *r, uint64_t val) {
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);

    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    uint64_t rank = 0;
    while (it.value != NULL) {
        leaf_t *leaf = (leaf_t *)it.value;
        int compare_result = compare_high48(it.key, high48);
        if (compare_result < 0) {
            rank += container_get_cardinality(leaf->container, leaf->typecode);
        } else if (compare_result == 0) {
            return rank +
                   container_rank(leaf->container, leaf->typecode, low16);
        } else {
            return rank;
        }
        art_iterator_next(&it);
    }
    return rank;
}

bool roaring64_bitmap_get_index(const roaring64_bitmap_t *r, uint64_t val,
                                uint64_t *out_index) {
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);

    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    uint64_t index = 0;
    while (it.value != NULL) {
        leaf_t *leaf = (leaf_t *)it.value;
        int compare_result = compare_high48(it.key, high48);
        if (compare_result < 0) {
            index += container_get_cardinality(leaf->container, leaf->typecode);
        } else if (compare_result == 0) {
            int index16 =
                container_get_index(leaf->container, leaf->typecode, low16);
            if (index16 < 0) {
                return false;
            }
            *out_index = index + index16;
            return true;
        } else {
            return false;
        }
        art_iterator_next(&it);
    }
    return false;
}

static inline leaf_t *containerptr_roaring64_bitmap_remove(
    roaring64_bitmap_t *r, uint8_t *high48, uint16_t low16, leaf_t *leaf) {
    if (leaf == NULL) {
        return NULL;
    }

    container_t *container = leaf->container;
    uint8_t typecode = leaf->typecode;
    uint8_t typecode2;
    container_t *container2 =
        container_remove(container, low16, typecode, &typecode2);
    if (container2 != container) {
        container_free(container, typecode);
        leaf->container = container2;
        leaf->typecode = typecode2;
    }
    if (!container_nonzero_cardinality(container2, typecode2)) {
        container_free(container2, typecode2);
        leaf = (leaf_t *)art_erase(&r->art, high48);
        if (leaf != NULL) {
            free_leaf(leaf);
        }
        return NULL;
    }
    return leaf;
}

void roaring64_bitmap_remove(roaring64_bitmap_t *r, uint64_t val) {
    art_t *art = &r->art;
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);

    leaf_t *leaf = (leaf_t *)art_find(art, high48);
    containerptr_roaring64_bitmap_remove(r, high48, low16, leaf);
}

bool roaring64_bitmap_remove_checked(roaring64_bitmap_t *r, uint64_t val) {
    art_t *art = &r->art;
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);
    leaf_t *leaf = (leaf_t *)art_find(art, high48);

    if (leaf == NULL) {
        return false;
    }
    int old_cardinality =
        container_get_cardinality(leaf->container, leaf->typecode);
    leaf = containerptr_roaring64_bitmap_remove(r, high48, low16, leaf);
    if (leaf == NULL) {
        return true;
    }
    int new_cardinality =
        container_get_cardinality(leaf->container, leaf->typecode);
    return new_cardinality != old_cardinality;
}

void roaring64_bitmap_remove_bulk(roaring64_bitmap_t *r,
                                  roaring64_bulk_context_t *context,
                                  uint64_t val) {
    art_t *art = &r->art;
    uint8_t high48[ART_KEY_BYTES];
    uint16_t low16 = split_key(val, high48);
    if (context->leaf != NULL &&
        compare_high48(context->high_bytes, high48) == 0) {
        // We're at a container with the correct high bits.
        uint8_t typecode2;
        container_t *container2 =
            container_remove(context->leaf->container, low16,
                             context->leaf->typecode, &typecode2);
        if (container2 != context->leaf->container) {
            container_free(context->leaf->container, context->leaf->typecode);
            context->leaf->container = container2;
            context->leaf->typecode = typecode2;
        }
        if (!container_nonzero_cardinality(container2, typecode2)) {
            leaf_t *leaf = (leaf_t *)art_erase(art, high48);
            container_free(container2, typecode2);
            free_leaf(leaf);
        }
    } else {
        // We're not positioned anywhere yet or the high bits of the key
        // differ.
        leaf_t *leaf = (leaf_t *)art_find(art, high48);
        context->leaf =
            containerptr_roaring64_bitmap_remove(r, high48, low16, leaf);
        memcpy(context->high_bytes, high48, ART_KEY_BYTES);
    }
}

void roaring64_bitmap_remove_many(roaring64_bitmap_t *r, size_t n_args,
                                  const uint64_t *vals) {
    if (n_args == 0) {
        return;
    }
    const uint64_t *end = vals + n_args;
    roaring64_bulk_context_t context = {0};
    for (const uint64_t *current_val = vals; current_val != end;
         current_val++) {
        roaring64_bitmap_remove_bulk(r, &context, *current_val);
    }
}

static inline void remove_range_closed_at(art_t *art, uint8_t *high48,
                                          uint16_t min, uint16_t max) {
    leaf_t *leaf = (leaf_t *)art_find(art, high48);
    if (leaf == NULL) {
        return;
    }
    uint8_t typecode2;
    container_t *container2 = container_remove_range(
        leaf->container, leaf->typecode, min, max, &typecode2);
    if (container2 != leaf->container) {
        container_free(leaf->container, leaf->typecode);
        if (container2 != NULL) {
            leaf->container = container2;
            leaf->typecode = typecode2;
        } else {
            art_erase(art, high48);
            free_leaf(leaf);
        }
    }
}

void roaring64_bitmap_remove_range_closed(roaring64_bitmap_t *r, uint64_t min,
                                          uint64_t max) {
    if (min > max) {
        return;
    }

    art_t *art = &r->art;
    uint8_t min_high48[ART_KEY_BYTES];
    uint16_t min_low16 = split_key(min, min_high48);
    uint8_t max_high48[ART_KEY_BYTES];
    uint16_t max_low16 = split_key(max, max_high48);
    if (compare_high48(min_high48, max_high48) == 0) {
        // Only remove a range within one container.
        remove_range_closed_at(art, min_high48, min_low16, max_low16);
        return;
    }

    // Remove a range across containers. Remove intermediate containers
    // entirely.
    remove_range_closed_at(art, min_high48, min_low16, 0xffff);
    uint64_t min_high_bits = min >> 16;
    uint64_t max_high_bits = max >> 16;
    for (uint64_t current = min_high_bits + 1; current < max_high_bits;
         ++current) {
        uint8_t current_high48[ART_KEY_BYTES];
        split_key(current << 16, current_high48);
        leaf_t *leaf = (leaf_t *)art_erase(art, current_high48);
        if (leaf != NULL) {
            container_free(leaf->container, leaf->typecode);
            free_leaf(leaf);
        }
    }
    remove_range_closed_at(art, max_high48, 0, max_low16);
}

uint64_t roaring64_bitmap_get_cardinality(const roaring64_bitmap_t *r) {
    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    uint64_t cardinality = 0;
    while (it.value != NULL) {
        leaf_t *leaf = (leaf_t *)it.value;
        cardinality +=
            container_get_cardinality(leaf->container, leaf->typecode);
        art_iterator_next(&it);
    }
    return cardinality;
}

uint64_t roaring64_bitmap_range_cardinality(const roaring64_bitmap_t *r,
                                            uint64_t min, uint64_t max) {
    if (min >= max) {
        return 0;
    }
    max--;  // A closed range is easier to work with.

    uint64_t cardinality = 0;
    uint8_t min_high48[ART_KEY_BYTES];
    uint16_t min_low16 = split_key(min, min_high48);
    uint8_t max_high48[ART_KEY_BYTES];
    uint16_t max_low16 = split_key(max, max_high48);

    art_iterator_t it = art_lower_bound(&r->art, min_high48);
    while (it.value != NULL) {
        int max_compare_result = compare_high48(it.key, max_high48);
        if (max_compare_result > 0) {
            // We're outside the range.
            break;
        }

        leaf_t *leaf = (leaf_t *)it.value;
        if (max_compare_result == 0) {
            // We're at the max high key, add only the range up to the low
            // 16 bits of max.
            cardinality +=
                container_rank(leaf->container, leaf->typecode, max_low16);
        } else {
            // We're not yet at the max high key, add the full container
            // range.
            cardinality +=
                container_get_cardinality(leaf->container, leaf->typecode);
        }
        if (compare_high48(it.key, min_high48) == 0 && min_low16 > 0) {
            // We're at the min high key, remove the range up to the low 16
            // bits of min.
            cardinality -=
                container_rank(leaf->container, leaf->typecode, min_low16 - 1);
        }
        art_iterator_next(&it);
    }
    return cardinality;
}

bool roaring64_bitmap_is_empty(const roaring64_bitmap_t *r) {
    return art_is_empty(&r->art);
}

uint64_t roaring64_bitmap_minimum(const roaring64_bitmap_t *r) {
    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    if (it.value == NULL) {
        return UINT64_MAX;
    }
    leaf_t *leaf = (leaf_t *)it.value;
    return combine_key(it.key,
                       container_minimum(leaf->container, leaf->typecode));
}

uint64_t roaring64_bitmap_maximum(const roaring64_bitmap_t *r) {
    art_iterator_t it = art_init_iterator(&r->art, /*first=*/false);
    if (it.value == NULL) {
        return 0;
    }
    leaf_t *leaf = (leaf_t *)it.value;
    return combine_key(it.key,
                       container_maximum(leaf->container, leaf->typecode));
}

bool roaring64_bitmap_run_optimize(roaring64_bitmap_t *r) {
    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    bool has_run_container = false;
    while (it.value != NULL) {
        leaf_t *leaf = (leaf_t *)it.value;
        uint8_t new_typecode;
        // We don't need to free the existing container if a new one was
        // created, convert_run_optimize does that internally.
        leaf->container = convert_run_optimize(leaf->container, leaf->typecode,
                                               &new_typecode);
        leaf->typecode = new_typecode;
        has_run_container |= new_typecode == RUN_CONTAINER_TYPE;
        art_iterator_next(&it);
    }
    return has_run_container;
}

size_t roaring64_bitmap_size_in_bytes(const roaring64_bitmap_t *r) {
    size_t size = art_size_in_bytes(&r->art);
    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    while (it.value != NULL) {
        leaf_t *leaf = (leaf_t *)it.value;
        size += sizeof(leaf_t);
        size += container_size_in_bytes(leaf->container, leaf->typecode);
        art_iterator_next(&it);
    }
    return size;
}

bool roaring64_bitmap_equals(const roaring64_bitmap_t *r1,
                             const roaring64_bitmap_t *r2) {
    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL && it2.value != NULL) {
        if (compare_high48(it1.key, it2.key) != 0) {
            return false;
        }
        leaf_t *leaf1 = (leaf_t *)it1.value;
        leaf_t *leaf2 = (leaf_t *)it2.value;
        if (!container_equals(leaf1->container, leaf1->typecode,
                              leaf2->container, leaf2->typecode)) {
            return false;
        }
        art_iterator_next(&it1);
        art_iterator_next(&it2);
    }
    return it1.value == NULL && it2.value == NULL;
}

bool roaring64_bitmap_is_subset(const roaring64_bitmap_t *r1,
                                const roaring64_bitmap_t *r2) {
    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL) {
        bool it2_present = it2.value != NULL;

        int compare_result = 0;
        if (it2_present) {
            compare_result = compare_high48(it1.key, it2.key);
            if (compare_result == 0) {
                leaf_t *leaf1 = (leaf_t *)it1.value;
                leaf_t *leaf2 = (leaf_t *)it2.value;
                if (!container_is_subset(leaf1->container, leaf1->typecode,
                                         leaf2->container, leaf2->typecode)) {
                    return false;
                }
                art_iterator_next(&it1);
                art_iterator_next(&it2);
            }
        }
        if (!it2_present || compare_result < 0) {
            return false;
        } else if (compare_result > 0) {
            art_iterator_lower_bound(&it2, it1.key);
        }
    }
    return true;
}

bool roaring64_bitmap_is_strict_subset(const roaring64_bitmap_t *r1,
                                       const roaring64_bitmap_t *r2) {
    return roaring64_bitmap_get_cardinality(r1) <
               roaring64_bitmap_get_cardinality(r2) &&
           roaring64_bitmap_is_subset(r1, r2);
}

roaring64_bitmap_t *roaring64_bitmap_and(const roaring64_bitmap_t *r1,
                                         const roaring64_bitmap_t *r2) {
    roaring64_bitmap_t *result = roaring64_bitmap_create();

    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL && it2.value != NULL) {
        // Cases:
        // 1. it1 <  it2 -> it1++
        // 2. it1 == it1 -> output it1 & it2, it1++, it2++
        // 3. it1 >  it2 -> it2++
        int compare_result = compare_high48(it1.key, it2.key);
        if (compare_result == 0) {
            // Case 2: iterators at the same high key position.
            leaf_t *result_leaf = (leaf_t *)roaring_malloc(sizeof(leaf_t));
            leaf_t *leaf1 = (leaf_t *)it1.value;
            leaf_t *leaf2 = (leaf_t *)it2.value;
            result_leaf->container = container_and(
                leaf1->container, leaf1->typecode, leaf2->container,
                leaf2->typecode, &result_leaf->typecode);

            if (container_nonzero_cardinality(result_leaf->container,
                                              result_leaf->typecode)) {
                art_insert(&result->art, it1.key, (art_val_t *)result_leaf);
            } else {
                container_free(result_leaf->container, result_leaf->typecode);
                free_leaf(result_leaf);
            }
            art_iterator_next(&it1);
            art_iterator_next(&it2);
        } else if (compare_result < 0) {
            // Case 1: it1 is before it2.
            art_iterator_lower_bound(&it1, it2.key);
        } else {
            // Case 3: it2 is before it1.
            art_iterator_lower_bound(&it2, it1.key);
        }
    }
    return result;
}

uint64_t roaring64_bitmap_and_cardinality(const roaring64_bitmap_t *r1,
                                          const roaring64_bitmap_t *r2) {
    uint64_t result = 0;

    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL && it2.value != NULL) {
        // Cases:
        // 1. it1 <  it2 -> it1++
        // 2. it1 == it1 -> output cardinaltiy it1 & it2, it1++, it2++
        // 3. it1 >  it2 -> it2++
        int compare_result = compare_high48(it1.key, it2.key);
        if (compare_result == 0) {
            // Case 2: iterators at the same high key position.
            leaf_t *leaf1 = (leaf_t *)it1.value;
            leaf_t *leaf2 = (leaf_t *)it2.value;
            result +=
                container_and_cardinality(leaf1->container, leaf1->typecode,
                                          leaf2->container, leaf2->typecode);
            art_iterator_next(&it1);
            art_iterator_next(&it2);
        } else if (compare_result < 0) {
            // Case 1: it1 is before it2.
            art_iterator_lower_bound(&it1, it2.key);
        } else {
            // Case 3: it2 is before it1.
            art_iterator_lower_bound(&it2, it1.key);
        }
    }
    return result;
}

// Inplace and (modifies its first argument).
void roaring64_bitmap_and_inplace(roaring64_bitmap_t *r1,
                                  const roaring64_bitmap_t *r2) {
    if (r1 == r2) {
        return;
    }
    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL) {
        // Cases:
        // 1. !it2_present -> erase it1
        // 2. it2_present
        //    a. it1 <  it2 -> erase it1
        //    b. it1 == it2 -> output it1 & it2, it1++, it2++
        //    c. it1 >  it2 -> it2++
        bool it2_present = it2.value != NULL;
        int compare_result = 0;
        if (it2_present) {
            compare_result = compare_high48(it1.key, it2.key);
            if (compare_result == 0) {
                // Case 2a: iterators at the same high key position.
                leaf_t *leaf1 = (leaf_t *)it1.value;
                leaf_t *leaf2 = (leaf_t *)it2.value;

                // We do the computation "in place" only when c1 is not a
                // shared container. Rationale: using a shared container
                // safely with in place computation would require making a
                // copy and then doing the computation in place which is
                // likely less efficient than avoiding in place entirely and
                // always generating a new container.
                uint8_t typecode2;
                container_t *container2;
                if (leaf1->typecode == SHARED_CONTAINER_TYPE) {
                    container2 = container_and(
                        leaf1->container, leaf1->typecode, leaf2->container,
                        leaf2->typecode, &typecode2);
                } else {
                    container2 = container_iand(
                        leaf1->container, leaf1->typecode, leaf2->container,
                        leaf2->typecode, &typecode2);
                }

                if (container2 != leaf1->container) {
                    container_free(leaf1->container, leaf1->typecode);
                    leaf1->container = container2;
                    leaf1->typecode = typecode2;
                }
                if (!container_nonzero_cardinality(container2, typecode2)) {
                    container_free(container2, typecode2);
                    art_iterator_erase(&r1->art, &it1);
                    free_leaf(leaf1);
                } else {
                    // Only advance the iterator if we didn't delete the
                    // leaf, as erasing advances by itself.
                    art_iterator_next(&it1);
                }
                art_iterator_next(&it2);
            }
        }

        if (!it2_present || compare_result < 0) {
            // Cases 1 and 3a: it1 is the only iterator or is before it2.
            leaf_t *leaf = (leaf_t *)art_iterator_erase(&r1->art, &it1);
            assert(leaf != NULL);
            container_free(leaf->container, leaf->typecode);
            free_leaf(leaf);
        } else if (compare_result > 0) {
            // Case 2c: it1 is after it2.
            art_iterator_lower_bound(&it2, it1.key);
        }
    }
}

bool roaring64_bitmap_intersect(const roaring64_bitmap_t *r1,
                                const roaring64_bitmap_t *r2) {
    bool intersect = false;
    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL && it2.value != NULL) {
        // Cases:
        // 1. it1 <  it2 -> it1++
        // 2. it1 == it1 -> intersect |= it1 & it2, it1++, it2++
        // 3. it1 >  it2 -> it2++
        int compare_result = compare_high48(it1.key, it2.key);
        if (compare_result == 0) {
            // Case 2: iterators at the same high key position.
            leaf_t *leaf1 = (leaf_t *)it1.value;
            leaf_t *leaf2 = (leaf_t *)it2.value;
            intersect |= container_intersect(leaf1->container, leaf1->typecode,
                                             leaf2->container, leaf2->typecode);
            art_iterator_next(&it1);
            art_iterator_next(&it2);
        } else if (compare_result < 0) {
            // Case 1: it1 is before it2.
            art_iterator_lower_bound(&it1, it2.key);
        } else {
            // Case 3: it2 is before it1.
            art_iterator_lower_bound(&it2, it1.key);
        }
    }
    return intersect;
}

double roaring64_bitmap_jaccard_index(const roaring64_bitmap_t *r1,
                                      const roaring64_bitmap_t *r2) {
    uint64_t c1 = roaring64_bitmap_get_cardinality(r1);
    uint64_t c2 = roaring64_bitmap_get_cardinality(r2);
    uint64_t inter = roaring64_bitmap_and_cardinality(r1, r2);
    return (double)inter / (double)(c1 + c2 - inter);
}

roaring64_bitmap_t *roaring64_bitmap_or(const roaring64_bitmap_t *r1,
                                        const roaring64_bitmap_t *r2) {
    roaring64_bitmap_t *result = roaring64_bitmap_create();

    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL || it2.value != NULL) {
        bool it1_present = it1.value != NULL;
        bool it2_present = it2.value != NULL;

        // Cases:
        // 1. it1_present  && !it2_present -> output it1, it1++
        // 2. !it1_present && it2_present  -> output it2, it2++
        // 3. it1_present  && it2_present
        //    a. it1 <  it2 -> output it1, it1++
        //    b. it1 == it2 -> output it1 | it2, it1++, it2++
        //    c. it1 >  it2 -> output it2, it2++
        int compare_result = 0;
        if (it1_present && it2_present) {
            compare_result = compare_high48(it1.key, it2.key);
            if (compare_result == 0) {
                // Case 3b: iterators at the same high key position.
                leaf_t *leaf1 = (leaf_t *)it1.value;
                leaf_t *leaf2 = (leaf_t *)it2.value;
                leaf_t *result_leaf = (leaf_t *)roaring_malloc(sizeof(leaf_t));
                result_leaf->container = container_or(
                    leaf1->container, leaf1->typecode, leaf2->container,
                    leaf2->typecode, &result_leaf->typecode);
                art_insert(&result->art, it1.key, (art_val_t *)result_leaf);
                art_iterator_next(&it1);
                art_iterator_next(&it2);
            }
        }
        if ((it1_present && !it2_present) || compare_result < 0) {
            // Cases 1 and 3a: it1 is the only iterator or is before it2.
            leaf_t *result_leaf = copy_leaf_container((leaf_t *)it1.value);
            art_insert(&result->art, it1.key, (art_val_t *)result_leaf);
            art_iterator_next(&it1);
        } else if ((!it1_present && it2_present) || compare_result > 0) {
            // Cases 2 and 3c: it2 is the only iterator or is before it1.
            leaf_t *result_leaf = copy_leaf_container((leaf_t *)it2.value);
            art_insert(&result->art, it2.key, (art_val_t *)result_leaf);
            art_iterator_next(&it2);
        }
    }
    return result;
}

uint64_t roaring64_bitmap_or_cardinality(const roaring64_bitmap_t *r1,
                                         const roaring64_bitmap_t *r2) {
    uint64_t c1 = roaring64_bitmap_get_cardinality(r1);
    uint64_t c2 = roaring64_bitmap_get_cardinality(r2);
    uint64_t inter = roaring64_bitmap_and_cardinality(r1, r2);
    return c1 + c2 - inter;
}

void roaring64_bitmap_or_inplace(roaring64_bitmap_t *r1,
                                 const roaring64_bitmap_t *r2) {
    if (r1 == r2) {
        return;
    }
    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL || it2.value != NULL) {
        bool it1_present = it1.value != NULL;
        bool it2_present = it2.value != NULL;

        // Cases:
        // 1. it1_present  && !it2_present -> it1++
        // 2. !it1_present && it2_present  -> add it2, it2++
        // 3. it1_present  && it2_present
        //    a. it1 <  it2 -> it1++
        //    b. it1 == it2 -> it1 | it2, it1++, it2++
        //    c. it1 >  it2 -> add it2, it2++
        int compare_result = 0;
        if (it1_present && it2_present) {
            compare_result = compare_high48(it1.key, it2.key);
            if (compare_result == 0) {
                // Case 3b: iterators at the same high key position.
                leaf_t *leaf1 = (leaf_t *)it1.value;
                leaf_t *leaf2 = (leaf_t *)it2.value;
                uint8_t typecode2;
                container_t *container2;
                if (leaf1->typecode == SHARED_CONTAINER_TYPE) {
                    container2 = container_or(leaf1->container, leaf1->typecode,
                                              leaf2->container, leaf2->typecode,
                                              &typecode2);
                } else {
                    container2 = container_ior(
                        leaf1->container, leaf1->typecode, leaf2->container,
                        leaf2->typecode, &typecode2);
                }
                if (container2 != leaf1->container) {
                    container_free(leaf1->container, leaf1->typecode);
                    leaf1->container = container2;
                    leaf1->typecode = typecode2;
                }
                art_iterator_next(&it1);
                art_iterator_next(&it2);
            }
        }
        if ((it1_present && !it2_present) || compare_result < 0) {
            // Cases 1 and 3a: it1 is the only iterator or is before it2.
            art_iterator_next(&it1);
        } else if ((!it1_present && it2_present) || compare_result > 0) {
            // Cases 2 and 3c: it2 is the only iterator or is before it1.
            leaf_t *result_leaf = copy_leaf_container((leaf_t *)it2.value);
            art_iterator_insert(&r1->art, &it1, it2.key,
                                (art_val_t *)result_leaf);
            art_iterator_next(&it2);
        }
    }
}

roaring64_bitmap_t *roaring64_bitmap_xor(const roaring64_bitmap_t *r1,
                                         const roaring64_bitmap_t *r2) {
    roaring64_bitmap_t *result = roaring64_bitmap_create();

    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL || it2.value != NULL) {
        bool it1_present = it1.value != NULL;
        bool it2_present = it2.value != NULL;

        // Cases:
        // 1. it1_present  && !it2_present -> output it1, it1++
        // 2. !it1_present && it2_present  -> output it2, it2++
        // 3. it1_present  && it2_present
        //    a. it1 <  it2 -> output it1, it1++
        //    b. it1 == it2 -> output it1 ^ it2, it1++, it2++
        //    c. it1 >  it2 -> output it2, it2++
        int compare_result = 0;
        if (it1_present && it2_present) {
            compare_result = compare_high48(it1.key, it2.key);
            if (compare_result == 0) {
                // Case 3b: iterators at the same high key position.
                leaf_t *leaf1 = (leaf_t *)it1.value;
                leaf_t *leaf2 = (leaf_t *)it2.value;
                leaf_t *result_leaf = (leaf_t *)roaring_malloc(sizeof(leaf_t));
                result_leaf->container = container_xor(
                    leaf1->container, leaf1->typecode, leaf2->container,
                    leaf2->typecode, &result_leaf->typecode);
                if (container_nonzero_cardinality(result_leaf->container,
                                                  result_leaf->typecode)) {
                    art_insert(&result->art, it1.key, (art_val_t *)result_leaf);
                } else {
                    container_free(result_leaf->container,
                                   result_leaf->typecode);
                    free_leaf(result_leaf);
                }
                art_iterator_next(&it1);
                art_iterator_next(&it2);
            }
        }
        if ((it1_present && !it2_present) || compare_result < 0) {
            // Cases 1 and 3a: it1 is the only iterator or is before it2.
            leaf_t *result_leaf = copy_leaf_container((leaf_t *)it1.value);
            art_insert(&result->art, it1.key, (art_val_t *)result_leaf);
            art_iterator_next(&it1);
        } else if ((!it1_present && it2_present) || compare_result > 0) {
            // Cases 2 and 3c: it2 is the only iterator or is before it1.
            leaf_t *result_leaf = copy_leaf_container((leaf_t *)it2.value);
            art_insert(&result->art, it2.key, (art_val_t *)result_leaf);
            art_iterator_next(&it2);
        }
    }
    return result;
}

uint64_t roaring64_bitmap_xor_cardinality(const roaring64_bitmap_t *r1,
                                          const roaring64_bitmap_t *r2) {
    uint64_t c1 = roaring64_bitmap_get_cardinality(r1);
    uint64_t c2 = roaring64_bitmap_get_cardinality(r2);
    uint64_t inter = roaring64_bitmap_and_cardinality(r1, r2);
    return c1 + c2 - 2 * inter;
}

void roaring64_bitmap_xor_inplace(roaring64_bitmap_t *r1,
                                  const roaring64_bitmap_t *r2) {
    assert(r1 != r2);
    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL || it2.value != NULL) {
        bool it1_present = it1.value != NULL;
        bool it2_present = it2.value != NULL;

        // Cases:
        // 1.  it1_present && !it2_present -> it1++
        // 2. !it1_present &&  it2_present -> add it2, it2++
        // 3.  it1_present &&  it2_present
        //    a. it1 <  it2 -> it1++
        //    b. it1 == it2 -> it1 ^ it2, it1++, it2++
        //    c. it1 >  it2 -> add it2, it2++
        int compare_result = 0;
        if (it1_present && it2_present) {
            compare_result = compare_high48(it1.key, it2.key);
            if (compare_result == 0) {
                // Case 3b: iterators at the same high key position.
                leaf_t *leaf1 = (leaf_t *)it1.value;
                leaf_t *leaf2 = (leaf_t *)it2.value;
                container_t *container1 = leaf1->container;
                uint8_t typecode1 = leaf1->typecode;
                uint8_t typecode2;
                container_t *container2;
                if (leaf1->typecode == SHARED_CONTAINER_TYPE) {
                    container2 = container_xor(
                        leaf1->container, leaf1->typecode, leaf2->container,
                        leaf2->typecode, &typecode2);
                    if (container2 != container1) {
                        // We only free when doing container_xor, not
                        // container_ixor, as ixor frees the original
                        // internally.
                        container_free(container1, typecode1);
                    }
                } else {
                    container2 = container_ixor(
                        leaf1->container, leaf1->typecode, leaf2->container,
                        leaf2->typecode, &typecode2);
                }
                leaf1->container = container2;
                leaf1->typecode = typecode2;

                if (!container_nonzero_cardinality(container2, typecode2)) {
                    container_free(container2, typecode2);
                    art_iterator_erase(&r1->art, &it1);
                    free_leaf(leaf1);
                } else {
                    // Only advance the iterator if we didn't delete the
                    // leaf, as erasing advances by itself.
                    art_iterator_next(&it1);
                }
                art_iterator_next(&it2);
            }
        }
        if ((it1_present && !it2_present) || compare_result < 0) {
            // Cases 1 and 3a: it1 is the only iterator or is before it2.
            art_iterator_next(&it1);
        } else if ((!it1_present && it2_present) || compare_result > 0) {
            // Cases 2 and 3c: it2 is the only iterator or is before it1.
            leaf_t *result_leaf = copy_leaf_container((leaf_t *)it2.value);
            if (it1_present) {
                art_iterator_insert(&r1->art, &it1, it2.key,
                                    (art_val_t *)result_leaf);
                art_iterator_next(&it1);
            } else {
                art_insert(&r1->art, it2.key, (art_val_t *)result_leaf);
            }
            art_iterator_next(&it2);
        }
    }
}

roaring64_bitmap_t *roaring64_bitmap_andnot(const roaring64_bitmap_t *r1,
                                            const roaring64_bitmap_t *r2) {
    roaring64_bitmap_t *result = roaring64_bitmap_create();

    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL) {
        // Cases:
        // 1. it1_present && !it2_present -> output it1, it1++
        // 2. it1_present && it2_present
        //    a. it1 <  it2 -> output it1, it1++
        //    b. it1 == it2 -> output it1 - it2, it1++, it2++
        //    c. it1 >  it2 -> it2++
        bool it2_present = it2.value != NULL;
        int compare_result = 0;
        if (it2_present) {
            compare_result = compare_high48(it1.key, it2.key);
            if (compare_result == 0) {
                // Case 2b: iterators at the same high key position.
                leaf_t *result_leaf = (leaf_t *)roaring_malloc(sizeof(leaf_t));
                leaf_t *leaf1 = (leaf_t *)it1.value;
                leaf_t *leaf2 = (leaf_t *)it2.value;
                result_leaf->container = container_andnot(
                    leaf1->container, leaf1->typecode, leaf2->container,
                    leaf2->typecode, &result_leaf->typecode);

                if (container_nonzero_cardinality(result_leaf->container,
                                                  result_leaf->typecode)) {
                    art_insert(&result->art, it1.key, (art_val_t *)result_leaf);
                } else {
                    container_free(result_leaf->container,
                                   result_leaf->typecode);
                    free_leaf(result_leaf);
                }
                art_iterator_next(&it1);
                art_iterator_next(&it2);
            }
        }
        if (!it2_present || compare_result < 0) {
            // Cases 1 and 2a: it1 is the only iterator or is before it2.
            leaf_t *result_leaf = copy_leaf_container((leaf_t *)it1.value);
            art_insert(&result->art, it1.key, (art_val_t *)result_leaf);
            art_iterator_next(&it1);
        } else if (compare_result > 0) {
            // Case 2c: it1 is after it2.
            art_iterator_next(&it2);
        }
    }
    return result;
}

uint64_t roaring64_bitmap_andnot_cardinality(const roaring64_bitmap_t *r1,
                                             const roaring64_bitmap_t *r2) {
    uint64_t c1 = roaring64_bitmap_get_cardinality(r1);
    uint64_t inter = roaring64_bitmap_and_cardinality(r1, r2);
    return c1 - inter;
}

void roaring64_bitmap_andnot_inplace(roaring64_bitmap_t *r1,
                                     const roaring64_bitmap_t *r2) {
    art_iterator_t it1 = art_init_iterator(&r1->art, /*first=*/true);
    art_iterator_t it2 = art_init_iterator(&r2->art, /*first=*/true);

    while (it1.value != NULL) {
        // Cases:
        // 1. it1_present && !it2_present -> it1++
        // 2. it1_present &&  it2_present
        //    a. it1 <  it2 -> it1++
        //    b. it1 == it2 -> it1 - it2, it1++, it2++
        //    c. it1 >  it2 -> it2++
        bool it2_present = it2.value != NULL;
        int compare_result = 0;
        if (it2_present) {
            compare_result = compare_high48(it1.key, it2.key);
            if (compare_result == 0) {
                // Case 2b: iterators at the same high key position.
                leaf_t *leaf1 = (leaf_t *)it1.value;
                leaf_t *leaf2 = (leaf_t *)it2.value;
                container_t *container1 = leaf1->container;
                uint8_t typecode1 = leaf1->typecode;
                uint8_t typecode2;
                container_t *container2;
                if (leaf1->typecode == SHARED_CONTAINER_TYPE) {
                    container2 = container_andnot(
                        leaf1->container, leaf1->typecode, leaf2->container,
                        leaf2->typecode, &typecode2);
                    if (container2 != container1) {
                        // We only free when doing container_andnot, not
                        // container_iandnot, as iandnot frees the original
                        // internally.
                        container_free(container1, typecode1);
                    }
                } else {
                    container2 = container_iandnot(
                        leaf1->container, leaf1->typecode, leaf2->container,
                        leaf2->typecode, &typecode2);
                }
                if (container2 != container1) {
                    leaf1->container = container2;
                    leaf1->typecode = typecode2;
                }

                if (!container_nonzero_cardinality(container2, typecode2)) {
                    container_free(container2, typecode2);
                    art_iterator_erase(&r1->art, &it1);
                    free_leaf(leaf1);
                } else {
                    // Only advance the iterator if we didn't delete the
                    // leaf, as erasing advances by itself.
                    art_iterator_next(&it1);
                }
                art_iterator_next(&it2);
            }
        }
        if (!it2_present || compare_result < 0) {
            // Cases 1 and 2a: it1 is the only iterator or is before it2.
            art_iterator_next(&it1);
        } else if (compare_result > 0) {
            // Case 2c: it1 is after it2.
            art_iterator_next(&it2);
        }
    }
}

bool roaring64_bitmap_iterate(const roaring64_bitmap_t *r,
                              roaring_iterator64 iterator, void *ptr) {
    art_iterator_t it = art_init_iterator(&r->art, /*first=*/true);
    while (it.value != NULL) {
        uint64_t high48 = combine_key(it.key, 0);
        uint64_t high32 = high48 & 0xFFFFFFFF00000000;
        uint32_t low32 = high48;
        leaf_t *leaf = (leaf_t *)it.value;
        if (!container_iterate64(leaf->container, leaf->typecode, low32,
                                 iterator, high32, ptr)) {
            return false;
        }
        art_iterator_next(&it);
    }
    return true;
}

static inline bool roaring64_iterator_init_at_leaf_first(
    roaring64_iterator_t *it) {
    it->high48 = combine_key(it->art_it.key, 0);
    leaf_t *leaf = (leaf_t *)it->art_it.value;
    uint16_t low16 = 0;
    it->container_it =
        container_init_iterator(leaf->container, leaf->typecode, &low16);
    it->value = it->high48 | low16;
    return (it->has_value = true);
}

static inline bool roaring64_iterator_init_at_leaf_last(
    roaring64_iterator_t *it) {
    it->high48 = combine_key(it->art_it.key, 0);
    leaf_t *leaf = (leaf_t *)it->art_it.value;
    uint16_t low16 = 0;
    it->container_it =
        container_init_iterator_last(leaf->container, leaf->typecode, &low16);
    it->value = it->high48 | low16;
    return (it->has_value = true);
}

static inline roaring64_iterator_t *roaring64_iterator_init_at(
    const roaring64_bitmap_t *r, roaring64_iterator_t *it, bool first) {
    it->parent = r;
    it->art_it = art_init_iterator(&r->art, first);
    it->has_value = it->art_it.value != NULL;
    if (it->has_value) {
        if (first) {
            roaring64_iterator_init_at_leaf_first(it);
        } else {
            roaring64_iterator_init_at_leaf_last(it);
        }
    }
    return it;
}

roaring64_iterator_t *roaring64_iterator_create(const roaring64_bitmap_t *r) {
    roaring64_iterator_t *it =
        (roaring64_iterator_t *)roaring_malloc(sizeof(roaring64_iterator_t));
    return roaring64_iterator_init_at(r, it, /*first=*/true);
}

roaring64_iterator_t *roaring64_iterator_create_last(
    const roaring64_bitmap_t *r) {
    roaring64_iterator_t *it =
        (roaring64_iterator_t *)roaring_malloc(sizeof(roaring64_iterator_t));
    return roaring64_iterator_init_at(r, it, /*first=*/false);
}

void roaring64_iterator_reinit(const roaring64_bitmap_t *r,
                               roaring64_iterator_t *it) {
    roaring64_iterator_init_at(r, it, /*first=*/true);
}

void roaring64_iterator_reinit_last(const roaring64_bitmap_t *r,
                                    roaring64_iterator_t *it) {
    roaring64_iterator_init_at(r, it, /*first=*/false);
}

roaring64_iterator_t *roaring64_iterator_copy(const roaring64_iterator_t *it) {
    roaring64_iterator_t *new_it =
        (roaring64_iterator_t *)roaring_malloc(sizeof(roaring64_iterator_t));
    memcpy(new_it, it, sizeof(*it));
    return new_it;
}

void roaring64_iterator_free(roaring64_iterator_t *it) { roaring_free(it); }

bool roaring64_iterator_has_value(const roaring64_iterator_t *it) {
    return it->has_value;
}

uint64_t roaring64_iterator_value(const roaring64_iterator_t *it) {
    return it->value;
}

bool roaring64_iterator_advance(roaring64_iterator_t *it) {
    if (it->art_it.value == NULL) {
        return (it->has_value = false);
    }
    leaf_t *leaf = (leaf_t *)it->art_it.value;
    uint16_t low16 = (uint16_t)it->value;
    if (container_iterator_next(leaf->container, leaf->typecode,
                                &it->container_it, &low16)) {
        it->value = it->high48 | low16;
        return (it->has_value = true);
    }
    if (!art_iterator_next(&it->art_it)) {
        return (it->has_value = false);
    }
    return roaring64_iterator_init_at_leaf_first(it);
}

bool roaring64_iterator_previous(roaring64_iterator_t *it) {
    if (it->art_it.value == NULL) {
        return (it->has_value = false);
    }
    leaf_t *leaf = (leaf_t *)it->art_it.value;
    uint16_t low16 = (uint16_t)it->value;
    if (container_iterator_prev(leaf->container, leaf->typecode,
                                &it->container_it, &low16)) {
        it->value = it->high48 | low16;
        return (it->has_value = true);
    }
    if (!art_iterator_prev(&it->art_it)) {
        return (it->has_value = false);
    }
    return roaring64_iterator_init_at_leaf_last(it);
}

bool roaring64_iterator_move_equalorlarger(roaring64_iterator_t *it,
                                           uint64_t val) {
    if (it->art_it.value == NULL) {
        return (it->has_value = false);
    }

    uint8_t val_high48[ART_KEY_BYTES];
    uint16_t val_low16 = split_key(val, val_high48);
    if (it->high48 < (val & 0xFFFFFFFFFFFF0000)) {
        // The ART iterator is before the high48 bits of `val`, so we need to
        // move to a leaf with a key equal or greater.
        if (!art_iterator_lower_bound(&it->art_it, val_high48)) {
            // Only smaller keys found.
            return (it->has_value = false);
        }
    }

    if (it->high48 == (val & 0xFFFFFFFFFFFF0000)) {
        // We're at equal high bits, check if a suitable value can be found in
        // this container.
        leaf_t *leaf = (leaf_t *)it->art_it.value;
        uint16_t low16 = (uint16_t)it->value;
        if (container_iterator_lower_bound(leaf->container, leaf->typecode,
                                           &it->container_it, &low16,
                                           val_low16)) {
            it->value = it->high48 | low16;
            return (it->has_value = true);
        }
        // Only smaller entries in this container, move to the next.
        if (!art_iterator_next(&it->art_it)) {
            return (it->has_value = false);
        }
    }

    // We're at a leaf with high bits greater than `val`, so the first entry in
    // this container is our result.
    return roaring64_iterator_init_at_leaf_first(it);
}

uint64_t roaring64_iterator_read(roaring64_iterator_t *it, uint64_t *buf,
                                 uint64_t count) {
    uint64_t consumed = 0;
    while (it->has_value && consumed < count) {
        uint32_t container_consumed;
        leaf_t *leaf = (leaf_t *)it->art_it.value;
        uint16_t low16 = (uint16_t)it->value;
        bool has_value = container_iterator_read_into_uint64(
            leaf->container, leaf->typecode, &it->container_it, it->high48, buf,
            count - consumed, &container_consumed, &low16);
        consumed += container_consumed;
        buf += container_consumed;
        if (has_value) {
            it->has_value = true;
            it->value = it->high48 | low16;
            assert(consumed == count);
            return consumed;
        }
        it->has_value = art_iterator_next(&it->art_it);
        if (it->has_value) {
            roaring64_iterator_init_at_leaf_first(it);
        }
    }
    return consumed;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace roaring
}  // namespace api
#endif
