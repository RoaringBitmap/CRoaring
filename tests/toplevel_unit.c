#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <roaring/misc/configreport.h>
#include <roaring/roaring.h>

// include internal headers for invasive testing
#include <roaring/containers/containers.h>
#include <roaring/roaring_array.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
using namespace roaring::internal;
#endif

#include "test.h"

static unsigned int seed = 123456789;
static const int OUR_RAND_MAX = (1 << 30) - 1;
inline static unsigned int
our_rand() {  // we do not want to depend on a system-specific
              // random number generator
    seed = (1103515245 * seed + 12345);
    return seed & OUR_RAND_MAX;
}

static inline uint32_t minimum_uint32(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

// arrays expected to both be sorted.
static int array_equals(const uint32_t *a1, int32_t size1, const uint32_t *a2,
                        int32_t size2) {
    if (size1 != size2) return 0;
    for (int i = 0; i < size1; ++i) {
        if (a1[i] != a2[i]) {
            return 0;
        }
    }
    return 1;
}

bool roaring_iterator_sumall(uint32_t value, void *param) {
    *(uint32_t *)param += value;
    return true;  // continue till the end
}
DEFINE_TEST(issue457) {
    roaring_bitmap_t *r1 = roaring_bitmap_from_range(65539, 65541, 1);
    roaring_bitmap_printf_describe(r1);
    assert_true(roaring_bitmap_get_cardinality(r1) == 2);
    roaring_bitmap_t *r2 = roaring_bitmap_add_offset(r1, -3);
    roaring_bitmap_printf_describe(r2);
    assert_true(roaring_bitmap_get_cardinality(r2) == 2);
    roaring_bitmap_printf(r2);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
}

DEFINE_TEST(issue429) {
    // This is a memory leak test, so we don't need to check the results.
    roaring_bitmap_t *b1 = roaring_bitmap_create();
    roaring_bitmap_add_range(b1, 0, 100);
    roaring_bitmap_remove_range(b1, 0, 99);
    roaring_bitmap_t *b2 = roaring_bitmap_copy(b1);
    const roaring_bitmap_t *bitmaps[] = {b1, b2};
    roaring_bitmap_t *result = roaring_bitmap_or_many_heap(2, bitmaps);
    roaring_bitmap_free(result);
    roaring_bitmap_free(b2);
    roaring_bitmap_free(b1);
}

DEFINE_TEST(issue431) {
    // This is a memory access test, so we don't need to check the results.
    roaring_bitmap_t *b1 = roaring_bitmap_create();
    roaring_bitmap_add(b1, 100);
    roaring_bitmap_flip_inplace(b1, 0, 100 + 1);
    roaring_bitmap_t *b2 = roaring_bitmap_create();
    roaring_bitmap_add_range(b2, 50, 100 + 1);
    roaring_bitmap_is_subset(b2, b1);
    roaring_bitmap_free(b2);
    roaring_bitmap_free(b1);
}

DEFINE_TEST(issue433) {
    roaring_bitmap_t *b1 = roaring_bitmap_create();
    roaring_bitmap_add(b1, 262143);
    roaring_bitmap_add_range_closed(b1, 258047, 262143);
    roaring_bitmap_remove_range_closed(b1, 262143, 262143);
    size_t len = roaring_bitmap_portable_size_in_bytes(b1);
    char *data = roaring_malloc(len);
    roaring_bitmap_portable_serialize(b1, data);
    roaring_bitmap_t *b2 = roaring_bitmap_portable_deserialize_safe(data, len);
    assert_true(roaring_bitmap_equals(b1, b2));
    roaring_bitmap_free(b2);
    roaring_bitmap_free(b1);
    roaring_free(data);
}

DEFINE_TEST(issue436) {
    roaring_bitmap_t *b1 = roaring_bitmap_create();
    roaring_bitmap_add_range_closed(b1, 19711, 262068);
    for (int i = 0; i < 0x10000; i += 2) {
        roaring_bitmap_add(b1, i);
    }
    roaring_bitmap_printf_describe(b1);
    roaring_bitmap_remove_range_closed(b1, 6143, 65505);
    size_t len = roaring_bitmap_portable_size_in_bytes(b1);
    char *data = roaring_malloc(len);
    roaring_bitmap_portable_serialize(b1, data);
    roaring_bitmap_t *b2 = roaring_bitmap_portable_deserialize_safe(data, len);
    assert_true(roaring_bitmap_equals(b1, b2));
    roaring_bitmap_free(b2);
    roaring_bitmap_free(b1);
    roaring_free(data);
}

DEFINE_TEST(issue440) {
    roaring_bitmap_t *b1 = roaring_bitmap_create();
    roaring_bitmap_add_range_closed(b1, 0x20000, 0x2FFFF);
    roaring_bitmap_add_range_closed(b1, 0, 0xFFFF);
    uint32_t largest_item = 0x11000;
    assert_false(roaring_bitmap_contains_range(b1, 0, largest_item + 1));
    assert_false(roaring_bitmap_contains(b1, largest_item));
    roaring_bitmap_free(b1);
}

DEFINE_TEST(range_contains) {
    uint32_t end = 2073952257;
    uint32_t start = end - 2;
    roaring_bitmap_t *bm = roaring_bitmap_from_range(start, end - 1, 1);
    roaring_bitmap_printf_describe(bm);
    printf("\n");
    assert_true(roaring_bitmap_contains_range(bm, start, end - 1));
    assert_false(roaring_bitmap_contains_range(bm, start, end));
    roaring_bitmap_free(bm);
}

DEFINE_TEST(contains_bulk) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    roaring_bulk_context_t context = {0};

    // Ensure checking an empty bitmap is okay
    assert_true(!roaring_bitmap_contains_bulk(bm, &context, 0));
    assert_true(!roaring_bitmap_contains_bulk(bm, &context, 0xFFFFFFFF));

    // create RLE container from [0, 1000]
    roaring_bitmap_add_range_closed(bm, 0, 1000);

    // add array container from 77000
    for (uint32_t i = 77000; i < 87000; i += 2) {
        roaring_bitmap_add(bm, i);
    }
    // add bitset container from 132000
    for (uint32_t i = 132000; i < 140000; i += 2) {
        roaring_bitmap_add(bm, i);
    }

    roaring_bitmap_add(bm, UINT32_MAX);

    uint32_t values[] = {
        1000,            // 1
        1001,            // 0
        77000,           // 1
        77001,           // 0
        77002,           // 1
        1002,            // 0
        132000,          // 1
        132001,          // 0
        132002,          // 1
        77003,           // 0
        UINT32_MAX,      // 1
        UINT32_MAX - 1,  // 0
    };
    size_t test_count = sizeof(values) / sizeof(values[0]);

    for (size_t i = 0; i < test_count; i++) {
        roaring_bulk_context_t empty_context = {0};
        bool expected_contains = roaring_bitmap_contains(bm, values[i]);
        assert_true(expected_contains == roaring_bitmap_contains_bulk(
                                             bm, &empty_context, values[i]));
        assert_true(expected_contains ==
                    roaring_bitmap_contains_bulk(bm, &context, values[i]));

        if (expected_contains) {
            assert_int_equal(context.key, values[i] >> 16);
        }
        if (context.container != NULL) {
            assert_in_range(context.idx, 0, bm->high_low_container.size - 1);
            assert_ptr_equal(context.container,
                             bm->high_low_container.containers[context.idx]);
            assert_int_equal(context.key,
                             bm->high_low_container.keys[context.idx]);
            assert_int_equal(context.typecode,
                             bm->high_low_container.typecodes[context.idx]);
        }
    }
    roaring_bitmap_free(bm);
}

DEFINE_TEST(is_really_empty) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    assert_true(roaring_bitmap_is_empty(bm));
    assert_false(roaring_bitmap_contains(bm, 0));
    roaring_bitmap_free(bm);
}

DEFINE_TEST(inplaceorwide) {
    uint64_t end = 4294901761;
    roaring_bitmap_t *r1 = roaring_bitmap_from_range(0, 1, 1);
    roaring_bitmap_t *r2 = roaring_bitmap_from_range(0, end, 1);
    roaring_bitmap_or_inplace(r1, r2);
    assert_true(roaring_bitmap_get_cardinality(r1) == end);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
}

void can_copy_empty(bool copy_on_write) {
    roaring_bitmap_t *bm1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(bm1, copy_on_write);
    roaring_bitmap_t *bm2 = roaring_bitmap_copy(bm1);
    assert_true(roaring_bitmap_get_cardinality(bm1) == 0);
    assert_true(roaring_bitmap_get_cardinality(bm2) == 0);
    assert_true(roaring_bitmap_is_empty(bm1));
    assert_true(roaring_bitmap_is_empty(bm2));
    roaring_bitmap_add(bm1, 3);
    roaring_bitmap_add(bm2, 5);
    assert_true(roaring_bitmap_get_cardinality(bm1) == 1);
    assert_true(roaring_bitmap_get_cardinality(bm2) == 1);
    assert_true(roaring_bitmap_contains(bm1, 3));
    assert_true(roaring_bitmap_contains(bm2, 5));
    assert_true(!roaring_bitmap_contains(bm2, 3));
    assert_true(!roaring_bitmap_contains(bm1, 5));
    roaring_bitmap_free(bm1);
    roaring_bitmap_free(bm2);
}

bool check_serialization(roaring_bitmap_t *bitmap) {
    const size_t size = roaring_bitmap_portable_size_in_bytes(bitmap);
    char *data = (char *)malloc(size);
    roaring_bitmap_portable_serialize(bitmap, data);
    roaring_bitmap_t *deserializedBitmap =
        roaring_bitmap_portable_deserialize(data);
    bool ret = roaring_bitmap_equals(bitmap, deserializedBitmap);
    roaring_bitmap_free(deserializedBitmap);
    free(data);
    return ret;
}

#if !CROARING_IS_BIG_ENDIAN
DEFINE_TEST(issue245) {
    roaring_bitmap_t *bitmap = roaring_bitmap_create();
    const uint32_t targetEntries = 2048;
    const int32_t runLength = 8;
    int32_t offset = 0;
    // Add a single run more than 2 extents longs.
    roaring_bitmap_add_range_closed(bitmap, offset, offset + runLength);
    offset += runLength + 2;
    // Add 2047 non-contiguous bits.
    for (uint32_t count = 1; count < targetEntries; count++, offset += 2) {
        roaring_bitmap_add_range_closed(bitmap, offset, offset);
    }

    if (!check_serialization(bitmap)) {
        printf("Bitmaps do not match at 2048 entries\n");
        abort();
    }

    // Add one more, forcing it to become a bitset
    offset += 2;
    roaring_bitmap_add_range_closed(bitmap, offset, offset);

    if (!check_serialization(bitmap)) {
        printf("Bitmaps do not match at 2049 entries\n");
        abort();
    }
    roaring_bitmap_free(bitmap);
}
#endif

DEFINE_TEST(issue208) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    for (uint32_t i = 1; i < 8194; i += 2) {
        roaring_bitmap_add(r, i);
    }
    uint32_t rank = roaring_bitmap_rank(r, 63);
    assert_true(rank == 32);
    roaring_bitmap_free(r);
}

DEFINE_TEST(issue208b) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    for (uint32_t i = 65536 - 64; i < 65536; i++) {
        roaring_bitmap_add(r, i);
    }
    for (uint32_t i = 0; i < 8196; i += 2) {
        roaring_bitmap_add(r, i);
    }
    for (uint32_t i = 65536 - 64; i < 65536; i++) {
        uint32_t expected = i - (65536 - 64) + 8196 / 2 + 1;
        uint32_t rank = roaring_bitmap_rank(r, i);
        assert_true(rank == expected);
    }
    roaring_bitmap_free(r);
}

DEFINE_TEST(issue288) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert_true(roaring_bitmap_get_cardinality(r1) == 0);
    assert_true(roaring_bitmap_get_cardinality(r2) == 0);

    roaring_bitmap_add(r1, 42);
    assert_true(roaring_bitmap_get_cardinality(r1) == 1);
    roaring_bitmap_overwrite(r1, r2);
    assert_true(roaring_bitmap_get_cardinality(r1) == 0);
    assert_true(roaring_bitmap_get_cardinality(r2) == 0);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
}

DEFINE_TEST(can_copy_empty_true) { can_copy_empty(true); }

DEFINE_TEST(can_copy_empty_false) { can_copy_empty(false); }

void can_add_to_copies(bool copy_on_write) {
    roaring_bitmap_t *bm1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(bm1, copy_on_write);
    roaring_bitmap_add(bm1, 3);
    roaring_bitmap_t *bm2 = roaring_bitmap_copy(bm1);
    assert_true(roaring_bitmap_get_cardinality(bm1) == 1);
    assert_true(roaring_bitmap_get_cardinality(bm2) == 1);
    roaring_bitmap_add(bm2, 4);
    roaring_bitmap_add(bm1, 5);
    assert_true(roaring_bitmap_get_cardinality(bm1) == 2);
    assert_true(roaring_bitmap_get_cardinality(bm2) == 2);
    roaring_bitmap_free(bm1);
    roaring_bitmap_free(bm2);
}

void convert_all_containers(roaring_bitmap_t *r, uint8_t dst_type) {
    for (int32_t i = 0; i < r->high_low_container.size; i++) {
        // first step: convert src_type to ARRAY
        if (r->high_low_container.typecodes[i] == BITSET_CONTAINER_TYPE) {
            array_container_t *dst_container = array_container_from_bitset(
                CAST_bitset(r->high_low_container.containers[i]));
            bitset_container_free(
                CAST_bitset(r->high_low_container.containers[i]));
            r->high_low_container.containers[i] = dst_container;
            r->high_low_container.typecodes[i] = ARRAY_CONTAINER_TYPE;
        } else if (r->high_low_container.typecodes[i] == RUN_CONTAINER_TYPE) {
            array_container_t *dst_container = array_container_from_run(
                CAST_run(r->high_low_container.containers[i]));
            run_container_free(CAST_run(r->high_low_container.containers[i]));
            r->high_low_container.containers[i] = dst_container;
            r->high_low_container.typecodes[i] = ARRAY_CONTAINER_TYPE;
        }
        assert_true(r->high_low_container.typecodes[i] == ARRAY_CONTAINER_TYPE);

        // second step: convert ARRAY to dst_type
        if (dst_type == BITSET_CONTAINER_TYPE) {
            bitset_container_t *dst_container = bitset_container_from_array(
                CAST_array(r->high_low_container.containers[i]));
            array_container_free(
                CAST_array(r->high_low_container.containers[i]));
            r->high_low_container.containers[i] = dst_container;
            r->high_low_container.typecodes[i] = BITSET_CONTAINER_TYPE;
        } else if (dst_type == RUN_CONTAINER_TYPE) {
            run_container_t *dst_container = run_container_from_array(
                CAST_array(r->high_low_container.containers[i]));
            array_container_free(
                CAST_array(r->high_low_container.containers[i]));
            r->high_low_container.containers[i] = dst_container;
            r->high_low_container.typecodes[i] = RUN_CONTAINER_TYPE;
        }
        assert_true(r->high_low_container.typecodes[i] == dst_type);
    }
}

/*
 * Tiny framework to compare roaring bitmap vs reference implementation
 * side by side
 */
struct sbs_s {
    roaring_bitmap_t *roaring;

    // reference implementation
    uint64_t *words;
    uint32_t size;  // number of words
};
typedef struct sbs_s sbs_t;

sbs_t *sbs_create(void) {
    sbs_t *sbs = (sbs_t *)malloc(sizeof(sbs_t));
    sbs->roaring = roaring_bitmap_create();
    sbs->size = 1;
    sbs->words = (uint64_t *)malloc(sbs->size * sizeof(uint64_t));
    for (uint32_t i = 0; i < sbs->size; i++) {
        sbs->words[i] = 0;
    }
    return sbs;
}

void sbs_free(sbs_t *sbs) {
    roaring_bitmap_free(sbs->roaring);
    free(sbs->words);
    free(sbs);
}

void sbs_convert(sbs_t *sbs, uint8_t code) {
    convert_all_containers(sbs->roaring, code);
}

void sbs_ensure_room(sbs_t *sbs, uint32_t v) {
    uint32_t i = v / 64;
    if (i >= sbs->size) {
        uint32_t new_size = (i + 1) * 3 / 2;
        sbs->words =
            (uint64_t *)realloc(sbs->words, new_size * sizeof(uint64_t));
        for (uint32_t j = sbs->size; j < new_size; j++) {
            sbs->words[j] = 0;
        }
        sbs->size = new_size;
    }
}

void sbs_add_value(sbs_t *sbs, uint32_t v) {
    roaring_bitmap_add(sbs->roaring, v);

    sbs_ensure_room(sbs, v);
    sbs->words[v / 64] |= UINT64_C(1) << (v % 64);
}

void sbs_add_range(sbs_t *sbs, uint64_t min, uint64_t max) {
    sbs_ensure_room(sbs, max);
    for (uint64_t v = min; v <= max; v++) {
        sbs->words[v / 64] |= UINT64_C(1) << (v % 64);
    }

    roaring_bitmap_add_range(sbs->roaring, min, max + 1);
}

void sbs_remove_range(sbs_t *sbs, uint64_t min, uint64_t max) {
    sbs_ensure_room(sbs, max);
    for (uint64_t v = min; v <= max; v++) {
        sbs->words[v / 64] &= ~(UINT64_C(1) << (v % 64));
    }

    roaring_bitmap_remove_range(sbs->roaring, min, max + 1);
}

void sbs_remove_many(sbs_t *sbs, size_t n_args, uint32_t *vals) {
    for (size_t i = 0; i < n_args; i++) {
        uint32_t v = vals[i];
        sbs_ensure_room(sbs, v);
        sbs->words[v / 64] &= ~(UINT64_C(1) << (v % 64));
    }
    roaring_bitmap_remove_many(sbs->roaring, n_args, vals);
}

bool sbs_check_type(sbs_t *sbs, uint8_t type) {
    bool answer = true;
    for (int32_t i = 0; i < sbs->roaring->high_low_container.size; i++) {
        answer =
            answer && (sbs->roaring->high_low_container.typecodes[i] == type);
    }
    return answer;
}

bool sbs_is_empty(sbs_t *sbs) {
    return sbs->roaring->high_low_container.size == 0;
}

void sbs_compare(sbs_t *sbs) {
    uint32_t expected_cardinality = 0;
    for (uint32_t i = 0; i < sbs->size; i++) {
        uint64_t word = sbs->words[i];
        while (word != 0) {
            expected_cardinality += 1;
            word = word & (word - 1);
        }
    }
    uint32_t *expected_values =
        (uint32_t *)malloc(expected_cardinality * sizeof(uint32_t));
    memset(expected_values, 0, expected_cardinality * sizeof(uint32_t));
    for (uint32_t i = 0, dst = 0; i < sbs->size; i++) {
        for (uint32_t j = 0; j < 64; j++) {
            if ((sbs->words[i] & (UINT64_C(1) << j)) != 0) {
                expected_values[dst++] = i * 64 + j;
            }
        }
    }

    uint32_t actual_cardinality = roaring_bitmap_get_cardinality(sbs->roaring);
    uint32_t *actual_values =
        (uint32_t *)malloc(actual_cardinality * sizeof(uint32_t));
    memset(actual_values, 0, actual_cardinality * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(sbs->roaring, actual_values);

    bool ok = array_equals(actual_values, actual_cardinality, expected_values,
                           expected_cardinality);
    if (!ok) {
        printf("Expected: ");
        for (uint32_t i = 0; i < expected_cardinality; i++) {
            printf("%u ", expected_values[i]);
        }
        printf("\n");

        printf("Actual: ");
        roaring_bitmap_printf(sbs->roaring);
        printf("\n");
    }
    free(actual_values);
    free(expected_values);
    assert_true(ok);
}

DEFINE_TEST(test_stats) {
    // create a new empty bitmap
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    // then we can add values
    for (uint32_t i = 100; i < 1000; i++) {
        roaring_bitmap_add(r1, i);
    }
    for (uint32_t i = 1000; i < 100000; i += 10) {
        roaring_bitmap_add(r1, i);
    }
    roaring_bitmap_add(r1, 100000);

    roaring_statistics_t stats;
    roaring_bitmap_statistics(r1, &stats);
    assert_true(stats.cardinality == roaring_bitmap_get_cardinality(r1));
    assert_true(stats.min_value == 100);
    assert_true(stats.max_value == 100000);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(with_huge_capacity) {
    roaring_bitmap_t *r = roaring_bitmap_create_with_capacity(UINT32_MAX);
    assert_non_null(r);
    assert_int_equal(r->high_low_container.allocation_size, (1 << 16));
    roaring_bitmap_free(r);
}

// this should expose memory leaks
// (https://github.com/RoaringBitmap/CRoaring/pull/70)
void leaks_with_empty(bool copy_on_write) {
    roaring_bitmap_t *empty = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(empty, copy_on_write);
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    for (uint32_t i = 100; i < 70000; i += 3) {
        roaring_bitmap_add(r1, i);
    }
    roaring_bitmap_t *ror = roaring_bitmap_or(r1, empty);
    roaring_bitmap_t *rxor = roaring_bitmap_xor(r1, empty);
    roaring_bitmap_t *rand = roaring_bitmap_and(r1, empty);
    roaring_bitmap_t *randnot = roaring_bitmap_andnot(r1, empty);
    roaring_bitmap_free(empty);
    assert_true(roaring_bitmap_equals(ror, r1));
    roaring_bitmap_free(ror);
    assert_true(roaring_bitmap_equals(rxor, r1));
    roaring_bitmap_free(rxor);
    assert_true(roaring_bitmap_equals(randnot, r1));
    roaring_bitmap_free(randnot);
    roaring_bitmap_free(r1);
    assert_true(roaring_bitmap_is_empty(rand));
    roaring_bitmap_free(rand);
}

DEFINE_TEST(leaks_with_empty_true) { leaks_with_empty(true); }

DEFINE_TEST(leaks_with_empty_false) { leaks_with_empty(false); }

DEFINE_TEST(check_interval) {
    // create a new bitmap with varargs
    roaring_bitmap_t *r = roaring_bitmap_from(1, 2, 3, 1000);
    assert_non_null(r);

    roaring_bitmap_printf(r);

    roaring_bitmap_t *range = roaring_bitmap_from_range(10, 1000 + 1, 1);
    assert_non_null(range);
    assert_true(roaring_bitmap_intersect(r, range));
    roaring_bitmap_t *range2 = roaring_bitmap_from_range(10, 1000, 1);
    assert_non_null(range2);
    assert_false(roaring_bitmap_intersect(r, range2));

    assert_true(roaring_bitmap_intersect_with_range(r, 10, 1000 + 1));
    assert_false(roaring_bitmap_intersect_with_range(r, 10, 1000));

    roaring_bitmap_free(r);
    roaring_bitmap_free(range);
    roaring_bitmap_free(range2);
}

DEFINE_TEST(check_full_inplace_flip) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    uint64_t bignumber = UINT64_C(0x100000000);
    roaring_bitmap_flip_inplace(r1, 0, bignumber);
    assert_true(roaring_bitmap_get_cardinality(r1) == bignumber);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(check_iterate_to_end) {
    uint64_t bignumber = UINT64_C(0x100000000);
    for (uint64_t s = 0; s < 1024; s++) {
        roaring_bitmap_t *r1 = roaring_bitmap_create();
        roaring_bitmap_flip_inplace(r1, bignumber - s, bignumber);
        roaring_uint32_iterator_t iterator;
        roaring_iterator_init(r1, &iterator);
        uint64_t count = 0;
        while (iterator.has_value) {
            assert_true(iterator.current_value + (s - count) == bignumber);
            count++;
            roaring_uint32_iterator_advance(&iterator);
        }
        assert_true(count == s);
        assert_true(roaring_bitmap_get_cardinality(r1) == s);
        roaring_bitmap_free(r1);
    }
}

DEFINE_TEST(check_iterate_to_beginning) {
    uint64_t bignumber = UINT64_C(0x100000000);
    for (uint64_t s = 0; s < 1024; s++) {
        roaring_bitmap_t *r1 = roaring_bitmap_create();
        roaring_bitmap_flip_inplace(r1, bignumber - s, bignumber);
        roaring_uint32_iterator_t iterator;
        roaring_iterator_init_last(r1, &iterator);
        uint64_t count = 0;
        while (iterator.has_value) {
            count++;
            assert_true(iterator.current_value + count == bignumber);
            roaring_uint32_iterator_previous(&iterator);
        }
        assert_true(count == s);
        assert_true(roaring_bitmap_get_cardinality(r1) == s);
        roaring_bitmap_free(r1);
    }
}

DEFINE_TEST(check_range_contains_from_end) {
    uint64_t bignumber = UINT64_C(0x100000000);
    for (uint64_t s = 0; s < 1024 * 1024; s++) {
        roaring_bitmap_t *r1 = roaring_bitmap_create();
        roaring_bitmap_add_range(r1, bignumber - s, bignumber);
        assert_true(roaring_bitmap_get_cardinality(r1) == s);
        if (s > 0) {
            assert_true(roaring_bitmap_contains_range(r1, bignumber - s,
                                                      bignumber - 1));
        }
        assert_true(
            roaring_bitmap_contains_range(r1, bignumber - s, bignumber));
        assert_false(
            roaring_bitmap_contains_range(r1, bignumber - s - 1, bignumber));
        assert_true(roaring_bitmap_get_cardinality(r1) == s);
        roaring_bitmap_free(r1);
    }
}

DEFINE_TEST(check_full_flip) {
    roaring_bitmap_t *rorg = roaring_bitmap_create();
    uint64_t bignumber = UINT64_C(0x100000000);
    roaring_bitmap_t *r1 = roaring_bitmap_flip(rorg, 0, bignumber);
    assert_true(roaring_bitmap_get_cardinality(r1) == bignumber);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(rorg);
}

void test_stress_memory(bool copy_on_write) {
    for (size_t i = 0; i < 5; i++) {
        roaring_bitmap_t *r1 = roaring_bitmap_create();
        roaring_bitmap_set_copy_on_write(r1, copy_on_write);
        assert_non_null(r1);
        for (size_t k = 0; k < 1000000; k++) {
            uint32_t j = rand() % (100000000);
            roaring_bitmap_add(r1, j);
        }
        roaring_bitmap_run_optimize(r1);
        uint32_t compact_size = roaring_bitmap_portable_size_in_bytes(r1);
        char *serializedbytes = (char *)malloc(compact_size);
        size_t actualsize =
            roaring_bitmap_portable_serialize(r1, serializedbytes);
        assert_int_equal(actualsize, compact_size);
        roaring_bitmap_t *t =
            roaring_bitmap_portable_deserialize(serializedbytes);
        assert_true(roaring_bitmap_equals(r1, t));
        roaring_bitmap_free(t);
        free(serializedbytes);
        roaring_bitmap_free(r1);
    }
}

DEFINE_TEST(test_stress_memory_true) { test_stress_memory(true); }

DEFINE_TEST(test_stress_memory_false) { test_stress_memory(false); }

void test_example(bool copy_on_write) {
    // create a new empty bitmap
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    assert_bitmap_validate(r1);
    assert_non_null(r1);

    // then we can add values
    for (uint32_t i = 100; i < 1000; i++) {
        roaring_bitmap_add(r1, i);
        assert_bitmap_validate(r1);
    }

    // check whether a value is contained
    assert_true(roaring_bitmap_contains(r1, 500));

    // compute how many bits there are:
    uint32_t cardinality = roaring_bitmap_get_cardinality(r1);
    printf("Cardinality = %d \n", cardinality);

    // if your bitmaps have long runs, you can compress them by calling
    // run_optimize
    uint32_t size = roaring_bitmap_portable_size_in_bytes(r1);
    roaring_bitmap_run_optimize(r1);
    assert_bitmap_validate(r1);
    uint32_t compact_size = roaring_bitmap_portable_size_in_bytes(r1);

    printf("size before run optimize %d bytes, and after %d bytes\n", size,
           compact_size);

    // create a new bitmap with varargs
    roaring_bitmap_t *r2 = roaring_bitmap_from(1, 2, 3, 5, 6);
    assert_bitmap_validate(r2);
    assert_non_null(r2);

    roaring_bitmap_printf(r2);

    // we can also create a bitmap from a pointer to 32-bit integers
    const uint32_t values[] = {2, 3, 4};
    roaring_bitmap_t *r3 = roaring_bitmap_of_ptr(3, values);
    roaring_bitmap_set_copy_on_write(r3, copy_on_write);
    assert_bitmap_validate(r3);

    // we can also go in reverse and go from arrays to bitmaps
    uint64_t card1 = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    assert_true(arr1 != NULL);
    roaring_bitmap_to_uint32_array(r1, arr1);

    // we can go from arrays to bitmaps from "offset" by "limit"
    size_t offset = 100;
    size_t limit = 1000;
    uint32_t *arr3 = (uint32_t *)malloc(limit * sizeof(uint32_t));
    assert_true(arr3 != NULL);
    roaring_bitmap_range_uint32_array(r1, offset, limit, arr3);
    free(arr3);

    roaring_bitmap_t *r1f = roaring_bitmap_of_ptr(card1, arr1);
    assert_bitmap_validate(r1f);
    free(arr1);
    assert_non_null(r1f);

    // bitmaps shall be equal
    assert_true(roaring_bitmap_equals(r1, r1f));
    roaring_bitmap_free(r1f);

    // we can copy and compare bitmaps
    roaring_bitmap_t *z = roaring_bitmap_copy(r3);
    roaring_bitmap_set_copy_on_write(z, copy_on_write);
    assert_bitmap_validate(z);
    assert_true(roaring_bitmap_equals(r3, z));

    roaring_bitmap_free(z);

    // we can compute union two-by-two
    roaring_bitmap_t *r1_2_3 = roaring_bitmap_or(r1, r2);
    assert_bitmap_validate(r1_2_3);
    assert_true(roaring_bitmap_get_cardinality(r1_2_3) ==
                roaring_bitmap_or_cardinality(r1, r2));

    roaring_bitmap_set_copy_on_write(r1_2_3, copy_on_write);
    roaring_bitmap_or_inplace(r1_2_3, r3);

    // we can compute a big union
    const roaring_bitmap_t *allmybitmaps[] = {r1, r2, r3};
    roaring_bitmap_t *bigunion = roaring_bitmap_or_many(3, allmybitmaps);
    assert_bitmap_validate(bigunion);
    assert_true(roaring_bitmap_equals(r1_2_3, bigunion));
    roaring_bitmap_t *bigunionheap =
        roaring_bitmap_or_many_heap(3, allmybitmaps);
    assert_bitmap_validate(bigunionheap);
    assert_true(roaring_bitmap_equals(r1_2_3, bigunionheap));
    roaring_bitmap_free(r1_2_3);
    roaring_bitmap_free(bigunion);
    roaring_bitmap_free(bigunionheap);

    // we can compute xor two-by-two
    roaring_bitmap_t *rx1_2_3 = roaring_bitmap_xor(r1, r2);
    roaring_bitmap_set_copy_on_write(rx1_2_3, copy_on_write);
    assert_bitmap_validate(rx1_2_3);
    roaring_bitmap_xor_inplace(rx1_2_3, r3);

    // we can compute a big xor
    const roaring_bitmap_t *allmybitmaps_x[] = {r1, r2, r3};
    roaring_bitmap_t *bigxor = roaring_bitmap_xor_many(3, allmybitmaps_x);
    assert_bitmap_validate(bigxor);
    assert_true(roaring_bitmap_equals(rx1_2_3, bigxor));

    roaring_bitmap_free(rx1_2_3);
    roaring_bitmap_free(bigxor);

    // we can compute intersection two-by-two
    roaring_bitmap_t *i1_2 = roaring_bitmap_and(r1, r2);
    assert_bitmap_validate(i1_2);
    assert_true(roaring_bitmap_get_cardinality(i1_2) ==
                roaring_bitmap_and_cardinality(r1, r2));

    roaring_bitmap_free(i1_2);

    // we can write a bitmap to a pointer and recover it later
    uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    char *serializedbytes = (char *)malloc(expectedsize);
    size_t actualsize = roaring_bitmap_portable_serialize(r1, serializedbytes);
    assert_int_equal(actualsize, expectedsize);
    roaring_bitmap_t *t = roaring_bitmap_portable_deserialize(serializedbytes);
    assert_bitmap_validate(t);
    assert_true(roaring_bitmap_equals(r1, t));
    roaring_bitmap_free(t);
    // we can also check whether there is a bitmap at a memory location without
    // reading it
    size_t sizeofbitmap =
        roaring_bitmap_portable_deserialize_size(serializedbytes, expectedsize);
    assert_true(
        sizeofbitmap ==
        expectedsize);  // sizeofbitmap would be zero if no bitmap were found
    // we can also read the bitmap "safely" by specifying a byte size limit:
    t = roaring_bitmap_portable_deserialize_safe(serializedbytes, expectedsize);
    assert_bitmap_validate(t);
    assert_true(roaring_bitmap_equals(r1, t));  // what we recover is equal
    roaring_bitmap_free(t);
    free(serializedbytes);

    // we can iterate over all values using custom functions
    uint32_t counter = 0;
    roaring_iterate(r1, roaring_iterator_sumall, &counter);

    /**
     * bool roaring_iterator_sumall(uint32_t value, void *param) {
     *        *(uint32_t *) param += value;
     *        return true; // continue till the end
     *  }
     *
     */

    // we can also create iterator structs
    counter = 0;
    roaring_uint32_iterator_t *i = roaring_iterator_create(r1);
    while (i->has_value) {
        counter++;
        roaring_uint32_iterator_advance(i);
    }
    roaring_uint32_iterator_free(i);
    assert_true(roaring_bitmap_get_cardinality(r1) == counter);

    // for greater speed, you can iterate over the data in bulk
    i = roaring_iterator_create(r1);
    uint32_t buffer[256];
    while (1) {
        uint32_t ret = roaring_uint32_iterator_read(i, buffer, 256);
        for (uint32_t j = 0; j < ret; j++) {
            counter += buffer[j];
        }
        if (ret < 256) {
            break;
        }
    }
    roaring_uint32_iterator_free(i);

    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r3);
}

void test_uint32_iterator(bool run) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    for (uint32_t i = 0; i < 66000; i += 3) {
        roaring_bitmap_add(r1, i);
    }
    for (uint32_t i = 100000; i < 200000; i++) {
        roaring_bitmap_add(r1, i);
    }
    for (uint32_t i = 300000; i < 500000; i += 100) {
        roaring_bitmap_add(r1, i);
    }
    for (uint32_t i = 600000; i < 700000; i += 1) {
        roaring_bitmap_add(r1, i);
    }
    for (uint32_t i = 800000; i < 900000; i += 7) {
        roaring_bitmap_add(r1, i);
    }
    if (run) roaring_bitmap_run_optimize(r1);
    assert_bitmap_validate(r1);
    roaring_uint32_iterator_t *iter = roaring_iterator_create(r1);
    for (uint32_t i = 0; i < 66000; i += 3) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i);
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_advance(iter);
    }
    for (uint32_t i = 100000; i < 200000; i++) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i);
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_advance(iter);
    }
    for (uint32_t i = 300000; i < 500000; i += 100) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i);
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_advance(iter);
    }
    for (uint32_t i = 600000; i < 700000; i += 1) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i);
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_advance(iter);
    }
    for (uint32_t i = 800000; i < 900000; i += 7) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i);
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_advance(iter);
    }
    assert_false(iter->has_value);
    roaring_uint32_iterator_move_equalorlarger(iter, 0);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 0);
    roaring_uint32_iterator_move_equalorlarger(iter, 66000);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 100000);
    roaring_uint32_iterator_move_equalorlarger(iter, 100000);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 100000);
    roaring_uint32_iterator_move_equalorlarger(iter, 200000);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 300000);
    roaring_uint32_iterator_move_equalorlarger(iter, 300000);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 300000);
    roaring_uint32_iterator_move_equalorlarger(iter, 500000);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 600000);
    roaring_uint32_iterator_move_equalorlarger(iter, 600000);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 600000);
    roaring_uint32_iterator_move_equalorlarger(iter, 700000);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 800000);
    roaring_uint32_iterator_move_equalorlarger(iter, 800000);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 800000);
    roaring_uint32_iterator_move_equalorlarger(iter, 900000);
    assert_false(iter->has_value);
    roaring_uint32_iterator_move_equalorlarger(iter, 0);
    for (uint32_t i = 0; i < 66000; i += 3) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i + 1);
    }
    for (uint32_t i = 100000; i < 200000; i++) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i + 1);
    }
    for (uint32_t i = 300000; i < 500000; i += 100) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i + 1);
    }
    for (uint32_t i = 600000; i < 700000; i += 1) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i + 1);
    }
    for (uint32_t i = 800000; i < 900000; i += 7) {
        assert_true(iter->has_value);
        assert_true(iter->current_value == i);
        roaring_uint32_iterator_move_equalorlarger(iter, i + 1);
    }
    assert_false(iter->has_value);

    roaring_uint32_iterator_free(iter);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_uint32_iterator_true) { test_uint32_iterator(true); }

DEFINE_TEST(test_uint32_iterator_false) { test_uint32_iterator(false); }

DEFINE_TEST(test_example_true) { test_example(true); }

DEFINE_TEST(test_example_false) { test_example(false); }

void can_remove_from_copies(bool copy_on_write) {
    roaring_bitmap_t *bm1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(bm1, copy_on_write);
    roaring_bitmap_add(bm1, 3);
    roaring_bitmap_t *bm2 = roaring_bitmap_copy(bm1);
    assert_true(roaring_bitmap_get_cardinality(bm1) == 1);
    assert_true(roaring_bitmap_get_cardinality(bm2) == 1);
    roaring_bitmap_add(bm2, 4);
    roaring_bitmap_add(bm1, 5);
    assert_true(roaring_bitmap_get_cardinality(bm1) == 2);
    assert_true(roaring_bitmap_get_cardinality(bm2) == 2);
    roaring_bitmap_remove(bm1, 5);
    assert_true(roaring_bitmap_get_cardinality(bm1) == 1);
    roaring_bitmap_remove(bm1, 4);
    assert_true(roaring_bitmap_get_cardinality(bm1) == 1);
    assert_true(roaring_bitmap_get_cardinality(bm2) == 2);
    roaring_bitmap_remove(bm2, 4);
    assert_true(roaring_bitmap_get_cardinality(bm2) == 1);
    roaring_bitmap_free(bm1);
    roaring_bitmap_free(bm2);
}

DEFINE_TEST(test_basic_add) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    roaring_bitmap_add(bm, 0);
    roaring_bitmap_remove(bm, 0);
    roaring_bitmap_free(bm);
}

DEFINE_TEST(test_addremove) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    for (uint32_t value = 33057; value < 147849; value += 8) {
        roaring_bitmap_add(bm, value);
    }
    for (uint32_t value = 33057; value < 147849; value += 8) {
        roaring_bitmap_remove(bm, value);
    }
    assert_bitmap_validate(bm);
    assert_true(roaring_bitmap_is_empty(bm));
    roaring_bitmap_free(bm);
}

DEFINE_TEST(test_addremove_bulk) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    roaring_bulk_context_t context = {0};
    for (uint32_t value = 33057; value < 147849; value += 8) {
        roaring_bitmap_add_bulk(bm, &context, value);
    }
    for (uint32_t value = 33057; value < 147849; value += 8) {
        assert_true(roaring_bitmap_remove_checked(bm, value));
    }
    assert_bitmap_validate(bm);
    assert_true(roaring_bitmap_is_empty(bm));
    roaring_bitmap_free(bm);
}

DEFINE_TEST(test_addremoverun) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    for (uint32_t value = 33057; value < 147849; value += 8) {
        roaring_bitmap_add(bm, value);
    }
    roaring_bitmap_run_optimize(bm);
    for (uint32_t value = 33057; value < 147849; value += 8) {
        roaring_bitmap_remove(bm, value);
    }
    assert_bitmap_validate(bm);
    assert_true(roaring_bitmap_is_empty(bm));
    roaring_bitmap_free(bm);
}

DEFINE_TEST(test_clear) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    for (uint32_t value = 33057; value < 147849; value += 8) {
        roaring_bitmap_add(bm, value);
    }
    roaring_bitmap_clear(bm);
    assert_true(roaring_bitmap_is_empty(bm));
    size_t expected_card = 0;
    for (uint32_t value = 33057; value < 147849; value += 8) {
        roaring_bitmap_add(bm, value);
        expected_card++;
    }
    assert_true(roaring_bitmap_get_cardinality(bm) == expected_card);
    roaring_bitmap_clear(bm);
    assert_true(roaring_bitmap_is_empty(bm));
    roaring_bitmap_free(bm);
}

DEFINE_TEST(test_remove_from_copies_true) { can_remove_from_copies(true); }

DEFINE_TEST(test_remove_from_copies_false) { can_remove_from_copies(false); }

bool check_bitmap_from_range(uint32_t min, uint64_t max, uint32_t step) {
    roaring_bitmap_t *result = roaring_bitmap_from_range(min, max, step);
    assert_non_null(result);
    roaring_bitmap_t *expected = roaring_bitmap_create();
    assert_non_null(expected);
    for (uint32_t value = min; value < max; value += step) {
        roaring_bitmap_add(expected, value);
    }
    assert_bitmap_validate(result);
    assert_bitmap_validate(expected);
    bool is_equal = roaring_bitmap_equals(expected, result);
    if (!is_equal) {
        fprintf(stderr, "[ERROR] check_bitmap_from_range(%u, %u, %u)\n",
                (unsigned)min, (unsigned)max, (unsigned)step);
    }
    roaring_bitmap_free(expected);
    roaring_bitmap_free(result);
    return is_equal;
}

DEFINE_TEST(test_silly_range) {
    check_bitmap_from_range(0, 1, 1);
    check_bitmap_from_range(0, 2, 1);
    roaring_bitmap_t *bm1 = roaring_bitmap_from_range(0, 1, 1);
    roaring_bitmap_t *bm2 = roaring_bitmap_from_range(0, 2, 1);
    assert_bitmap_validate(bm1);
    assert_bitmap_validate(bm2);
    assert_false(roaring_bitmap_equals(bm1, bm2));
    roaring_bitmap_free(bm1);
    roaring_bitmap_free(bm2);
}

DEFINE_TEST(test_adversarial_range) {
    roaring_bitmap_t *bm1 =
        roaring_bitmap_from_range(0, UINT64_C(0x100000000), 1);
    assert_bitmap_validate(bm1);
    assert_true(roaring_bitmap_get_cardinality(bm1) == UINT64_C(0x100000000));
    roaring_bitmap_free(bm1);
}

DEFINE_TEST(test_range_and_serialize) {
    roaring_bitmap_t *old_bm = roaring_bitmap_from_range(65520, 131057, 16);
    size_t size = roaring_bitmap_portable_size_in_bytes(old_bm);
    char *buff = (char *)malloc(size);
    size_t actualsize = roaring_bitmap_portable_serialize(old_bm, buff);
    assert_int_equal(actualsize, size);
    roaring_bitmap_t *new_bm = roaring_bitmap_portable_deserialize(buff);
    assert_true(roaring_bitmap_equals(old_bm, new_bm));
    roaring_bitmap_free(old_bm);
    roaring_bitmap_free(new_bm);
    free(buff);
}

DEFINE_TEST(test_bitmap_from_range) {
    assert_true(roaring_bitmap_from_range(1, 10, 0) ==
                NULL);                                        // undefined range
    assert_true(roaring_bitmap_from_range(5, 1, 3) == NULL);  // empty range
    for (uint32_t i = 16; i < 1 << 18; i *= 2) {
        uint32_t min = i - 10;
        for (uint32_t delta = 16; delta < 1 << 18; delta *= 2) {
            uint32_t max = i + delta;
            for (uint32_t step = 1; step <= 64;
                 step *= 2) {  // check powers of 2
                assert_true(check_bitmap_from_range(min, max, step));
            }
            for (uint32_t step = 1; step <= 81;
                 step *= 3) {  // check powers of 3
                assert_true(check_bitmap_from_range(min, max, step));
            }
            for (uint32_t step = 1; step <= 125;
                 step *= 5) {  // check powers of 5
                assert_true(check_bitmap_from_range(min, max, step));
            }
        }
    }

    // max range
    roaring_bitmap_t *r = roaring_bitmap_from_range(0, UINT64_MAX, 1);
    assert_true(roaring_bitmap_get_cardinality(r) == UINT64_C(0x100000000));
    roaring_bitmap_free(r);
}

DEFINE_TEST(test_printf) {
    roaring_bitmap_t *r1 =
        roaring_bitmap_from(1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    assert_bitmap_validate(r1);
    assert_non_null(r1);
    roaring_bitmap_printf(r1);
    roaring_bitmap_free(r1);
    printf("\n");
}

DEFINE_TEST(test_printf_withbitmap) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    roaring_bitmap_printf(r1);
    /* Add some values to the bitmap */
    for (int i = 0, top_val = 4097; i < top_val; i++)
        roaring_bitmap_add(r1, 2 * i);
    roaring_bitmap_printf(r1);
    roaring_bitmap_free(r1);
    printf("\n");
}

DEFINE_TEST(test_printf_withrun) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    roaring_bitmap_printf(r1);
    /* Add some values to the bitmap */
    for (int i = 100, top_val = 200; i < top_val; i++)
        roaring_bitmap_add(r1, i);
    roaring_bitmap_run_optimize(r1);
    assert_bitmap_validate(r1);
    roaring_bitmap_printf(r1);  // does it crash?
    roaring_bitmap_free(r1);
    printf("\n");
}

bool dummy_iterator(uint32_t value, void *param) {
    (void)value;

    uint32_t *num = (uint32_t *)param;
    (*num)++;
    return true;
}

DEFINE_TEST(test_iterate) {
    roaring_bitmap_t *r1 =
        roaring_bitmap_from(1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    assert_non_null(r1);

    uint32_t num = 0;
    /* Add some values to the bitmap */
    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);

    roaring_iterate(r1, dummy_iterator, (void *)&num);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), num);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_iterate_empty) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    uint32_t num = 0;

    roaring_iterate(r1, dummy_iterator, (void *)&num);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), 0);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), num);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_iterate_withbitmap) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    /* Add some values to the bitmap */
    for (int i = 0, top_val = 4097; i < top_val; i++)
        roaring_bitmap_add(r1, 2 * i);
    uint32_t num = 0;

    roaring_iterate(r1, dummy_iterator, (void *)&num);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), num);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_iterate_withrun) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    /* Add some values to the bitmap */
    for (int i = 100, top_val = 200; i < top_val; i++)
        roaring_bitmap_add(r1, i);
    roaring_bitmap_run_optimize(r1);
    uint32_t num = 0;
    roaring_iterate(r1, dummy_iterator, (void *)&num);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), num);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_remove_withrun) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    /* Add some values to the bitmap */
    for (int i = 100, top_val = 20000; i < top_val; i++)
        roaring_bitmap_add(r1, i);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), 20000 - 100);
    roaring_bitmap_remove(r1, 1000);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), 20000 - 100 - 1);
    roaring_bitmap_run_optimize(r1);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), 20000 - 100 - 1);
    roaring_bitmap_remove(r1, 2000);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), 20000 - 100 - 1 - 1);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_portable_serialize) {
    roaring_bitmap_t *r1 =
        roaring_bitmap_from(1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    assert_non_null(r1);

    uint32_t serialize_len;
    roaring_bitmap_t *r2;

    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);

    uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    char *serialized = (char *)malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert_int_equal(serialize_len, expectedsize);
    assert_int_equal(serialize_len, expectedsize);
    r2 = roaring_bitmap_portable_deserialize(serialized);
    assert_non_null(r2);

    uint64_t card1 = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr1);

    uint64_t card2 = roaring_bitmap_get_cardinality(r2);
    uint32_t *arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r2, arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    r1 = roaring_bitmap_from(2946000, 2997491, 10478289, 10490227, 10502444,
                             19866827);
    expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    serialized = (char *)malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert_int_equal(serialize_len, expectedsize);
    assert_int_equal(serialize_len, expectedsize);

    r2 = roaring_bitmap_portable_deserialize(serialized);
    assert_non_null(r2);

    card1 = roaring_bitmap_get_cardinality(r1);
    arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr1);

    card2 = roaring_bitmap_get_cardinality(r2);
    arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r2, arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t k = 100; k < 100000; ++k) {
        roaring_bitmap_add(r1, k);
    }

    roaring_bitmap_run_optimize(r1);
    expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    serialized = (char *)malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert_int_equal(serialize_len, expectedsize);

    r2 = roaring_bitmap_portable_deserialize(serialized);
    assert_non_null(r2);

    card1 = roaring_bitmap_get_cardinality(r1);
    arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr1);

    card2 = roaring_bitmap_get_cardinality(r2);
    arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r2, arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
}

DEFINE_TEST(test_serialize) {
    roaring_bitmap_t *r1 =
        roaring_bitmap_from(1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    assert_non_null(r1);

    uint32_t serialize_len;
    char *serialized;
    roaring_bitmap_t *r2;

    /* Add some values to the bitmap */
    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);
    serialized = (char *)malloc(roaring_bitmap_size_in_bytes(r1));
    serialize_len = roaring_bitmap_serialize(r1, serialized);
    assert_int_equal(serialize_len, roaring_bitmap_size_in_bytes(r1));
    r2 = roaring_bitmap_deserialize(serialized);
    assert_non_null(r2);

    uint64_t card1 = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr1);

    uint64_t card2 = roaring_bitmap_get_cardinality(r2);
    uint32_t *arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r2, arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    /* manually create a run container, and inject ito a roaring bitmap */
    run_container_t *run = run_container_create_given_capacity(1024);
    assert_non_null(run);
    for (int i = 0; i < 768; i++) run_container_add(run, 3 * i);

    r1 = roaring_bitmap_create_with_capacity(1);
    ra_append(&r1->high_low_container, 0, run, RUN_CONTAINER_TYPE);

    serialize_len = roaring_bitmap_size_in_bytes(r1);
    serialized = (char *)malloc(serialize_len);
    assert_int_equal((int32_t)serialize_len,
                     roaring_bitmap_serialize(r1, serialized));
    r2 = roaring_bitmap_deserialize(serialized);
    assert_true(roaring_bitmap_equals(r1, r2));

    // Check that roaring_bitmap_deserialize_safe fails on invalid length

    assert_null(roaring_bitmap_deserialize_safe(serialized, 0));
    assert_null(roaring_bitmap_deserialize_safe(serialized, serialize_len - 1));

    // Check that roaring_bitmap_deserialize_safe succeed with valid length

    roaring_bitmap_t *t_safe =
        roaring_bitmap_deserialize_safe(serialized, serialize_len);
    assert_true(roaring_bitmap_equals(r1, t_safe));
    roaring_bitmap_free(t_safe);

    // Check that roaring_bitmap_deserialize_safe succeed with larger length

    t_safe = roaring_bitmap_deserialize_safe(serialized, serialize_len + 10);
    assert_true(roaring_bitmap_equals(r1, t_safe));
    roaring_bitmap_free(t_safe);

    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    r1 = roaring_bitmap_from(2946000, 2997491, 10478289, 10490227, 10502444,
                             19866827);

    serialized = (char *)malloc(roaring_bitmap_size_in_bytes(r1));
    serialize_len = roaring_bitmap_serialize(r1, serialized);
    assert_int_equal(serialize_len, roaring_bitmap_size_in_bytes(r1));
    r2 = roaring_bitmap_deserialize(serialized);
    assert_non_null(r2);

    card1 = roaring_bitmap_get_cardinality(r1);
    arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    assert_non_null(arr1);
    roaring_bitmap_to_uint32_array(r1, arr1);

    card2 = roaring_bitmap_get_cardinality(r2);
    arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    assert_non_null(arr2);
    roaring_bitmap_to_uint32_array(r2, arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    r1 = roaring_bitmap_create();
    for (uint32_t k = 100; k < 100000; ++k) {
        roaring_bitmap_add(r1, k);
    }
    roaring_bitmap_run_optimize(r1);
    serialized = (char *)malloc(roaring_bitmap_size_in_bytes(r1));
    serialize_len = roaring_bitmap_serialize(r1, serialized);
    assert_int_equal(serialize_len, roaring_bitmap_size_in_bytes(r1));
    r2 = roaring_bitmap_deserialize(serialized);

    card1 = roaring_bitmap_get_cardinality(r1);
    arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    assert_non_null(arr1);
    roaring_bitmap_to_uint32_array(r1, arr1);

    card2 = roaring_bitmap_get_cardinality(r2);
    arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    assert_non_null(arr2);
    roaring_bitmap_to_uint32_array(r2, arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));

    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    /* ******* */
    roaring_bitmap_t *old_bm = roaring_bitmap_create();
    for (unsigned i = 0; i < 102; i++) roaring_bitmap_add(old_bm, i);
    char *buff = (char *)malloc(roaring_bitmap_size_in_bytes(old_bm));
    uint32_t size = roaring_bitmap_serialize(old_bm, buff);
    assert_int_equal(size, roaring_bitmap_size_in_bytes(old_bm));
    roaring_bitmap_t *new_bm = roaring_bitmap_deserialize(buff);

    // Check that roaring_bitmap_deserialize_safe fails on invalid length
    assert_null(roaring_bitmap_deserialize_safe(buff, size - 1));
    // Check that roaring_bitmap_deserialize_safe succeed with valid length
    t_safe = roaring_bitmap_deserialize_safe(buff, size);
    assert_true(roaring_bitmap_equals(new_bm, t_safe));
    roaring_bitmap_free(t_safe);

    free(buff);
    assert_true((unsigned int)roaring_bitmap_get_cardinality(old_bm) ==
                (unsigned int)roaring_bitmap_get_cardinality(new_bm));
    assert_true(roaring_bitmap_equals(old_bm, new_bm));
    roaring_bitmap_free(old_bm);
    roaring_bitmap_free(new_bm);
}

DEFINE_TEST(test_add) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 10000; ++i) {
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i);
        roaring_bitmap_add(r1, 200 * i);
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i + 1);
    }

    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_add_checked) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    assert_true(roaring_bitmap_add_checked(r1, 999));
    for (uint32_t i = 0; i < 125; ++i) {
        assert_true(roaring_bitmap_add_checked(r1, 3823 * i));
        assert_false(roaring_bitmap_add_checked(r1, 3823 * i));
    }
    assert_false(roaring_bitmap_add_checked(r1, 999));

    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_remove_checked) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    for (uint32_t i = 0; i < 125; ++i) {
        roaring_bitmap_add(bm, i * 3533);
    }
    for (uint32_t i = 0; i < 125; ++i) {
        assert_true(roaring_bitmap_remove_checked(bm, i * 3533));
        assert_false(roaring_bitmap_remove_checked(bm, i * 3533));
    }
    assert_false(roaring_bitmap_remove_checked(bm, 999));
    roaring_bitmap_add(bm, 999);
    assert_true(roaring_bitmap_remove_checked(bm, 999));
    assert_true(roaring_bitmap_is_empty(bm));
    roaring_bitmap_free(bm);
}

DEFINE_TEST(test_contains) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 10000; ++i) {
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i);
        roaring_bitmap_add(r1, 200 * i);
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i + 1);
    }

    for (uint32_t i = 0; i < 200 * 10000; ++i) {
        assert_int_equal(roaring_bitmap_contains(r1, i), (i % 200 == 0));
    }

    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_contains_range) {
    uint32_t *values = (uint32_t *)malloc(100000 * sizeof(uint32_t));
    assert_non_null(values);
    for (uint32_t length_range = 1; length_range <= 64; ++length_range) {
        roaring_bitmap_t *r1 = roaring_bitmap_create();
        assert_non_null(r1);
        for (uint32_t i = 0; i < 100000; ++i) {
            const uint32_t val = rand() % 200000;
            roaring_bitmap_add(r1, val);
            values[i] = val;
        }
        for (uint64_t i = 0; i < 100000; ++i) {
            if (roaring_bitmap_contains_range(r1, values[i],
                                              values[i] + length_range)) {
                for (uint32_t j = values[i]; j < values[i] + length_range; ++j)
                    assert_true(roaring_bitmap_contains(r1, j));
            } else {
                uint32_t count = 0;
                for (uint32_t j = values[i]; j < values[i] + length_range;
                     ++j) {
                    if (roaring_bitmap_contains(r1, j))
                        ++count;
                    else
                        break;
                }
                assert_true(count != length_range);
            }
        }
        roaring_bitmap_free(r1);
    }
    free(values);
    for (uint32_t length_range = 1; length_range <= 64; ++length_range) {
        roaring_bitmap_t *r1 = roaring_bitmap_create();
        assert_non_null(r1);
        const uint32_t length_range_twice = length_range * 2;
        for (uint32_t i = 0; i < 130000; i += length_range) {
            if (i % length_range_twice == 0) {
                for (uint32_t j = i; j < i + length_range; ++j)
                    roaring_bitmap_add(r1, j);
            }
        }
        for (uint32_t i = 0; i < 130000; i += length_range) {
            bool pres = roaring_bitmap_contains_range(r1, i, i + length_range);
            assert_true(((i % length_range_twice == 0) ? pres : !pres));
        }
        roaring_bitmap_free(r1);
    }
}

DEFINE_TEST(test_contains_range_PyRoaringBitMap_issue81) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    roaring_bitmap_add_range(r, 1, 1900544);
    assert_true(roaring_bitmap_contains_range(r, 1, 1900544));
    assert_false(roaring_bitmap_contains_range(r, 1900543, 1900545));
    roaring_bitmap_free(r);
}

DEFINE_TEST(test_intersection_array_x_array) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert_non_null(r2);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);

        assert_int_equal(roaring_bitmap_get_cardinality(r2), 2 * (i + 1));
        assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * (i + 1));
    }

    roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
    assert_true(roaring_bitmap_get_cardinality(r1_and_r2) ==
                roaring_bitmap_and_cardinality(r1, r2));

    assert_non_null(r1_and_r2);
    assert_int_equal(roaring_bitmap_get_cardinality(r1_and_r2), 2 * 34);

    roaring_bitmap_free(r1_and_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_intersection_array_x_array_inplace) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_true(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert_true(r2);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);

        assert_int_equal(roaring_bitmap_get_cardinality(r2), 2 * (i + 1));
        assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * (i + 1));
    }

    roaring_bitmap_and_inplace(r1, r2);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * 34);

    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_intersection_bitset_x_bitset) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_true(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert_true(r2);

    for (uint32_t i = 0; i < 20000; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r2, 3 * i + 1);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i + 1);

        assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * (i + 1));
        assert_int_equal(roaring_bitmap_get_cardinality(r2), 4 * (i + 1));
    }

    roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
    assert_true(roaring_bitmap_get_cardinality(r1_and_r2) ==
                roaring_bitmap_and_cardinality(r1, r2));

    assert_non_null(r1_and_r2);

    // NOT analytically determined but seems reasonable
    assert_int_equal(roaring_bitmap_get_cardinality(r1_and_r2), 26666);

    roaring_bitmap_free(r1_and_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_intersection_bitset_x_bitset_inplace) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_true(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    assert_true(r2);

    for (uint32_t i = 0; i < 20000; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        roaring_bitmap_add(r2, 3 * i + 1);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i);
        roaring_bitmap_add(r2, 5 * 65536 + 3 * i + 1);

        assert_int_equal(roaring_bitmap_get_cardinality(r1), 2 * (i + 1));
        assert_int_equal(roaring_bitmap_get_cardinality(r2), 4 * (i + 1));
    }

    roaring_bitmap_and_inplace(r1, r2);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), 26666);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

void test_union(bool copy_on_write) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    assert_true(r1);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r2, copy_on_write);
    assert_true(r2);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r2, 3 * i);
        assert_int_equal(roaring_bitmap_get_cardinality(r2), i + 1);
        assert_int_equal(roaring_bitmap_get_cardinality(r1), i + 1);
    }

    roaring_bitmap_t *r1_or_r2 = roaring_bitmap_or(r1, r2);
    assert_true(roaring_bitmap_get_cardinality(r1_or_r2) ==
                roaring_bitmap_or_cardinality(r1, r2));

    roaring_bitmap_set_copy_on_write(r1_or_r2, copy_on_write);
    assert_int_equal(roaring_bitmap_get_cardinality(r1_or_r2), 166);

    roaring_bitmap_free(r1_or_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);
}

DEFINE_TEST(test_union_true) { test_union(true); }

DEFINE_TEST(test_union_false) { test_union(false); }

// density factor changes as one gets further into bitmap
static roaring_bitmap_t *gen_bitmap(double start_density,
                                    double density_gradient, int run_length,
                                    int blank_range_start, int blank_range_end,
                                    int universe_size) {
    srand(2345);
    roaring_bitmap_t *ans = roaring_bitmap_create();
    double d = start_density;

    for (int i = 0; i < universe_size; i += run_length) {
        d = start_density + i * density_gradient;
        double r = our_rand() / (double)OUR_RAND_MAX;
        assert_true(r <= 1.0);
        assert_true(r >= 0);
        if (r < d && !(i >= blank_range_start && i < blank_range_end))
            for (int j = 0; j < run_length; ++j) roaring_bitmap_add(ans, i + j);
    }
    roaring_bitmap_run_optimize(ans);
    return ans;
}

static roaring_bitmap_t *synthesized_xor(roaring_bitmap_t *r1,
                                         roaring_bitmap_t *r2) {
    unsigned universe_size = 0;
    roaring_statistics_t stats;
    roaring_bitmap_statistics(r1, &stats);
    universe_size = stats.max_value;
    roaring_bitmap_statistics(r2, &stats);
    if (stats.max_value > universe_size) universe_size = stats.max_value;

    roaring_bitmap_t *r1_or_r2 = roaring_bitmap_or(r1, r2);
    assert_true(roaring_bitmap_get_cardinality(r1_or_r2) ==
                roaring_bitmap_or_cardinality(r1, r2));

    roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
    assert_true(roaring_bitmap_get_cardinality(r1_and_r2) ==
                roaring_bitmap_and_cardinality(r1, r2));

    roaring_bitmap_t *r1_nand_r2 =
        roaring_bitmap_flip(r1_and_r2, 0U, universe_size + 1U);
    roaring_bitmap_t *r1_xor_r2 = roaring_bitmap_and(r1_or_r2, r1_nand_r2);
    roaring_bitmap_free(r1_or_r2);
    roaring_bitmap_free(r1_and_r2);
    roaring_bitmap_free(r1_nand_r2);
    return r1_xor_r2;
}

static roaring_bitmap_t *synthesized_andnot(roaring_bitmap_t *r1,
                                            roaring_bitmap_t *r2) {
    unsigned universe_size = 0;
    roaring_statistics_t stats;
    roaring_bitmap_statistics(r1, &stats);
    universe_size = stats.max_value;
    roaring_bitmap_statistics(r2, &stats);
    if (stats.max_value > universe_size) universe_size = stats.max_value;

    roaring_bitmap_t *not_r2 = roaring_bitmap_flip(r2, 0U, universe_size + 1U);
    roaring_bitmap_t *r1_andnot_r2 = roaring_bitmap_and(r1, not_r2);
    roaring_bitmap_free(not_r2);
    return r1_andnot_r2;
}

// only for valid for universe < 10M, could adapt with roaring_bitmap_statistics
static void show_difference(roaring_bitmap_t *result,
                            roaring_bitmap_t *hopedfor) {
    int out_ctr = 0;
    for (int i = 0; i < 10000000; ++i) {
        if (roaring_bitmap_contains(result, i) &&
            !roaring_bitmap_contains(hopedfor, i)) {
            printf("result incorrectly has %d\n", i);
            ++out_ctr;
        }
        if (!roaring_bitmap_contains(result, i) &&
            roaring_bitmap_contains(hopedfor, i)) {
            printf("result incorrectly omits %d\n", i);
            ++out_ctr;
        }
        if (out_ctr > 20) {
            printf("20 errors seen, stopping comparison\n");
            break;
        }
    }
}

void test_xor(bool copy_on_write) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r2, copy_on_write);

    for (uint32_t i = 0; i < 300; ++i) {
        if (i % 2 == 0) roaring_bitmap_add(r1, i);
        if (i % 3 == 0) roaring_bitmap_add(r2, i);
    }

    roaring_bitmap_t *r1_xor_r2 = roaring_bitmap_xor(r1, r2);
    roaring_bitmap_set_copy_on_write(r1_xor_r2, copy_on_write);

    int ansctr = 0;
    for (int i = 0; i < 300; ++i) {
        if (((i % 2 == 0) || (i % 3 == 0)) && (i % 6 != 0)) {
            ansctr++;
            if (!roaring_bitmap_contains(r1_xor_r2, i))
                printf("missing %d\n", i);
        } else if (roaring_bitmap_contains(r1_xor_r2, i))
            printf("surplus %d\n", i);
    }

    assert_int_equal(roaring_bitmap_get_cardinality(r1_xor_r2), ansctr);

    roaring_bitmap_free(r1_xor_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);

    // some tougher tests on synthetic data

    roaring_bitmap_t *r[] = {
        // ascending density, last containers might be runs
        gen_bitmap(0.0, 1e-6, 1, 0, 0, 1000000),
        // descending density, first containers might be runs
        gen_bitmap(1.0, -1e-6, 1, 0, 0, 1000000),
        // uniformly rather sparse
        gen_bitmap(1e-5, 0.0, 1, 0, 0, 2000000),
        // uniformly rather sparse with runs
        gen_bitmap(1e-5, 0.0, 3, 0, 0, 2000000),
        // uniformly rather dense
        gen_bitmap(1e-1, 0.0, 1, 0, 0, 2000000),
        // ascending density but never too dense
        gen_bitmap(0.001, 1e-7, 1, 0, 0, 1000000),
        // ascending density but very sparse
        gen_bitmap(0.0, 1e-10, 1, 0, 0, 1000000),
        // descending with a gap
        gen_bitmap(0.5, -1e-6, 1, 600000, 800000, 1000000),
        //  gap elsewhere
        gen_bitmap(1, -1e-6, 1, 300000, 500000, 1000000),
        0  // sentinel
    };

    for (int i = 0; r[i]; ++i) {
        for (int j = i; r[j]; ++j) {
            roaring_bitmap_t *expected = synthesized_xor(r[i], r[j]);
            roaring_bitmap_t *result = roaring_bitmap_xor(r[i], r[j]);
            assert_true(roaring_bitmap_get_cardinality(result) ==
                        roaring_bitmap_xor_cardinality(r[i], r[j]));

            bool is_equal = roaring_bitmap_equals(expected, result);

            assert_true(is_equal);
            roaring_bitmap_free(expected);
            roaring_bitmap_free(result);
        }
    }
    for (int i = 0; r[i]; ++i) roaring_bitmap_free(r[i]);
}

DEFINE_TEST(test_xor_true) { test_xor(true); }

DEFINE_TEST(test_xor_false) { test_xor(false); }

void test_xor_inplace(bool copy_on_write) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r2, copy_on_write);

    for (uint32_t i = 0; i < 300; ++i) {
        if (i % 2 == 0) roaring_bitmap_add(r1, i);
        if (i % 3 == 0) roaring_bitmap_add(r2, i);
    }
    roaring_bitmap_xor_inplace(r1, r2);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), 166 - 16);

    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);

    // some tougher tests on synthetic data

    roaring_bitmap_t *r[] = {
        // ascending density, last containers might be runs
        gen_bitmap(0.0, 1e-6, 1, 0, 0, 1000000),
        // descending density, first containers might be runs
        gen_bitmap(1.0, -1e-6, 1, 0, 0, 1000000),
        // uniformly rather sparse
        gen_bitmap(1e-5, 0.0, 1, 0, 0, 2000000),
        // uniformly rather sparse with runs
        gen_bitmap(1e-5, 0.0, 3, 0, 0, 2000000),
        // uniformly rather dense
        gen_bitmap(1e-1, 0.0, 1, 0, 0, 2000000),
        // ascending density but never too dense
        gen_bitmap(0.001, 1e-7, 1, 0, 0, 1000000),
        // ascending density but very sparse
        gen_bitmap(0.0, 1e-10, 1, 0, 0, 1000000),
        // descending with a gap
        gen_bitmap(0.5, -1e-6, 1, 600000, 800000, 1000000),
        //  gap elsewhere
        gen_bitmap(1, -1e-6, 1, 300000, 500000, 1000000),
        0  // sentinel
    };

    for (int i = 0; r[i]; ++i) {
        for (int j = i + 1; r[j]; ++j) {
            roaring_bitmap_t *expected = synthesized_xor(r[i], r[j]);
            roaring_bitmap_t *copy = roaring_bitmap_copy(r[i]);
            roaring_bitmap_set_copy_on_write(copy, copy_on_write);

            roaring_bitmap_xor_inplace(copy, r[j]);

            bool is_equal = roaring_bitmap_equals(expected, copy);
            if (!is_equal) {
                printf("problem with i=%d j=%d\n", i, j);
                printf("copy's cardinality  is %d and expected's is %d\n",
                       (int)roaring_bitmap_get_cardinality(copy),
                       (int)roaring_bitmap_get_cardinality(expected));
                show_difference(copy, expected);
            }

            assert_true(is_equal);
            roaring_bitmap_free(expected);
            roaring_bitmap_free(copy);
        }
    }
    for (int i = 0; r[i]; ++i) roaring_bitmap_free(r[i]);
}

DEFINE_TEST(test_xor_inplace_true) { test_xor_inplace(true); }

DEFINE_TEST(test_xor_inplace_false) { test_xor_inplace(false); }

void test_xor_lazy(bool copy_on_write) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r2, copy_on_write);

    for (uint32_t i = 0; i < 300; ++i) {
        if (i % 2 == 0) roaring_bitmap_add(r1, i);
        if (i % 3 == 0) roaring_bitmap_add(r2, i);
    }

    roaring_bitmap_t *r1_xor_r2 = roaring_bitmap_lazy_xor(r1, r2);
    roaring_bitmap_repair_after_lazy(r1_xor_r2);

    roaring_bitmap_set_copy_on_write(r1_xor_r2, copy_on_write);

    int ansctr = 0;
    for (int i = 0; i < 300; ++i) {
        if (((i % 2 == 0) || (i % 3 == 0)) && (i % 6 != 0)) {
            ansctr++;
            if (!roaring_bitmap_contains(r1_xor_r2, i))
                printf("missing %d\n", i);
        } else if (roaring_bitmap_contains(r1_xor_r2, i))
            printf("surplus %d\n", i);
    }

    assert_int_equal(roaring_bitmap_get_cardinality(r1_xor_r2), ansctr);

    roaring_bitmap_free(r1_xor_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);

    // some tougher tests on synthetic data
    roaring_bitmap_t *r[] = {
        // ascending density, last containers might be runs
        gen_bitmap(0.0, 1e-6, 1, 0, 0, 1000000),
        // descending density, first containers might be runs
        gen_bitmap(1.0, -1e-6, 1, 0, 0, 1000000),
        // uniformly rather sparse
        gen_bitmap(1e-5, 0.0, 1, 0, 0, 2000000),
        // uniformly rather sparse with runs
        gen_bitmap(1e-5, 0.0, 3, 0, 0, 2000000),
        // uniformly rather dense
        gen_bitmap(1e-1, 0.0, 1, 0, 0, 2000000),
        // ascending density but never too dense
        gen_bitmap(0.001, 1e-7, 1, 0, 0, 1000000),
        // ascending density but very sparse
        gen_bitmap(0.0, 1e-10, 1, 0, 0, 1000000),
        // descending with a gap
        gen_bitmap(0.5, -1e-6, 1, 600000, 800000, 1000000),
        //  gap elsewhere
        gen_bitmap(1, -1e-6, 1, 300000, 500000, 1000000),
        0  // sentinel
    };

    for (int i = 0; r[i]; ++i) {
        for (int j = i; r[j]; ++j) {
            roaring_bitmap_t *expected = synthesized_xor(r[i], r[j]);

            roaring_bitmap_t *result = roaring_bitmap_lazy_xor(r[i], r[j]);
            roaring_bitmap_repair_after_lazy(result);

            bool is_equal = roaring_bitmap_equals(expected, result);
            if (!is_equal) {
                printf("problem with i=%d j=%d\n", i, j);
                printf("result's cardinality  is %d and expected's is %d\n",
                       (int)roaring_bitmap_get_cardinality(result),
                       (int)roaring_bitmap_get_cardinality(expected));
                show_difference(result, expected);
            }

            assert_true(is_equal);
            roaring_bitmap_free(expected);
            roaring_bitmap_free(result);
        }
    }
    for (int i = 0; r[i]; ++i) roaring_bitmap_free(r[i]);
}

DEFINE_TEST(test_xor_lazy_true) { test_xor_lazy(true); }

DEFINE_TEST(test_xor_lazy_false) { test_xor_lazy(false); }

void test_xor_lazy_inplace(bool copy_on_write) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r2, copy_on_write);

    for (uint32_t i = 0; i < 300; ++i) {
        if (i % 2 == 0) roaring_bitmap_add(r1, i);
        if (i % 3 == 0) roaring_bitmap_add(r2, i);
    }

    roaring_bitmap_t *r1_xor_r2 = roaring_bitmap_copy(r1);
    roaring_bitmap_set_copy_on_write(r1_xor_r2, copy_on_write);

    roaring_bitmap_lazy_xor_inplace(r1_xor_r2, r2);
    roaring_bitmap_repair_after_lazy(r1_xor_r2);

    int ansctr = 0;
    for (int i = 0; i < 300; ++i) {
        if (((i % 2 == 0) || (i % 3 == 0)) && (i % 6 != 0)) {
            ansctr++;
            if (!roaring_bitmap_contains(r1_xor_r2, i))
                printf("missing %d\n", i);
        } else if (roaring_bitmap_contains(r1_xor_r2, i))
            printf("surplus %d\n", i);
    }

    assert_int_equal(roaring_bitmap_get_cardinality(r1_xor_r2), ansctr);

    roaring_bitmap_free(r1_xor_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);

    // some tougher tests on synthetic data
    roaring_bitmap_t *r[] = {
        // ascending density, last containers might be runs
        gen_bitmap(0.0, 1e-6, 1, 0, 0, 1000000),
        // descending density, first containers might be runs
        gen_bitmap(1.0, -1e-6, 1, 0, 0, 1000000),
        // uniformly rather sparse
        gen_bitmap(1e-5, 0.0, 1, 0, 0, 2000000),
        // uniformly rather sparse with runs
        gen_bitmap(1e-5, 0.0, 3, 0, 0, 2000000),
        // uniformly rather dense
        gen_bitmap(1e-1, 0.0, 1, 0, 0, 2000000),
        // ascending density but never too dense
        gen_bitmap(0.001, 1e-7, 1, 0, 0, 1000000),
        // ascending density but very sparse
        gen_bitmap(0.0, 1e-10, 1, 0, 0, 1000000),
        // descending with a gap
        gen_bitmap(0.5, -1e-6, 1, 600000, 800000, 1000000),
        //  gap elsewhere
        gen_bitmap(1, -1e-6, 1, 300000, 500000, 1000000),
        0  // sentinel
    };

    for (int i = 0; r[i]; ++i) {
        for (int j = i; r[j]; ++j) {
            roaring_bitmap_t *expected = synthesized_xor(r[i], r[j]);

            roaring_bitmap_t *result = roaring_bitmap_copy(r[i]);
            roaring_bitmap_lazy_xor_inplace(result, r[j]);
            roaring_bitmap_repair_after_lazy(result);

            bool is_equal = roaring_bitmap_equals(expected, result);
            if (!is_equal) {
                printf("problem with i=%d j=%d\n", i, j);
                printf("result's cardinality  is %d and expected's is %d\n",
                       (int)roaring_bitmap_get_cardinality(result),
                       (int)roaring_bitmap_get_cardinality(expected));
                show_difference(result, expected);
            }

            assert_true(is_equal);
            roaring_bitmap_free(expected);
            roaring_bitmap_free(result);
        }
    }
    for (int i = 0; r[i]; ++i) roaring_bitmap_free(r[i]);
}

DEFINE_TEST(test_xor_lazy_inplace_true) { test_xor_lazy_inplace(true); }

DEFINE_TEST(test_xor_lazy_inplace_false) { test_xor_lazy_inplace(false); }

static roaring_bitmap_t *roaring_from_sentinel_array(int *data,
                                                     bool copy_on_write) {
    roaring_bitmap_t *ans = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(ans, copy_on_write);

    for (; *data != -1; ++data) {
        roaring_bitmap_add(ans, *data);
    }
    return ans;
}

void test_andnot(bool copy_on_write) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r2, copy_on_write);

    int data1[] = {1,
                   2,
                   65536 * 2 + 1,
                   65536 * 2 + 2,
                   65536 * 3 + 1,
                   65536 * 3 + 2,
                   65536 * 10 + 1,
                   65536 * 10 + 2,
                   65536 * 16 + 1,
                   65536 * 16 + 2,
                   65536 * 20 + 1,
                   65536 * 21 + 1,
                   -1};
    roaring_bitmap_t *rb1 = roaring_from_sentinel_array(data1, copy_on_write);
    int data2[] = {2,
                   3,
                   65536 * 10 + 2,
                   65536 * 10 + 3,
                   65536 * 12 + 2,
                   65536 * 12 + 3,
                   65536 * 14 + 2,
                   65536 * 14 + 3,
                   65536 * 16 + 2,
                   65536 * 16 + 3,
                   -1};
    roaring_bitmap_t *rb2 = roaring_from_sentinel_array(data2, copy_on_write);

    int data3[] = {2,
                   3,
                   65536 * 10 + 1,
                   65536 * 10 + 2,
                   65536 * 12 + 2,
                   65536 * 12 + 3,
                   65536 * 14 + 2,
                   65536 * 14 + 3,
                   65536 * 16 + 2,
                   65536 * 16 + 3,
                   -1};
    roaring_bitmap_t *rb3 = roaring_from_sentinel_array(data3, copy_on_write);
    int d1_minus_d2[] = {1,
                         65536 * 2 + 1,
                         65536 * 2 + 2,
                         65536 * 3 + 1,
                         65536 * 3 + 2,
                         65536 * 10 + 1,
                         65536 * 16 + 1,
                         65536 * 20 + 1,
                         65536 * 21 + 1,
                         -1};
    roaring_bitmap_t *rb1_minus_rb2 =
        roaring_from_sentinel_array(d1_minus_d2, copy_on_write);

    int d1_minus_d3[] = {1, 65536 * 2 + 1, 65536 * 2 + 2, 65536 * 3 + 1,
                         65536 * 3 + 2,
                         // 65536*10+1,
                         65536 * 16 + 1, 65536 * 20 + 1, 65536 * 21 + 1, -1};
    roaring_bitmap_t *rb1_minus_rb3 =
        roaring_from_sentinel_array(d1_minus_d3, copy_on_write);

    int d2_minus_d1[] = {3,
                         65536 * 10 + 3,
                         65536 * 12 + 2,
                         65536 * 12 + 3,
                         65536 * 14 + 2,
                         65536 * 14 + 3,
                         65536 * 16 + 3,
                         -1};

    roaring_bitmap_t *rb2_minus_rb1 =
        roaring_from_sentinel_array(d2_minus_d1, copy_on_write);

    int d3_minus_d1[] = {3,
                         // 65536*10+3,
                         65536 * 12 + 2, 65536 * 12 + 3, 65536 * 14 + 2,
                         65536 * 14 + 3, 65536 * 16 + 3, -1};
    roaring_bitmap_t *rb3_minus_rb1 =
        roaring_from_sentinel_array(d3_minus_d1, copy_on_write);

    int d3_minus_d2[] = {65536 * 10 + 1, -1};
    roaring_bitmap_t *rb3_minus_rb2 =
        roaring_from_sentinel_array(d3_minus_d2, copy_on_write);

    roaring_bitmap_t *temp = roaring_bitmap_andnot(rb1, rb2);
    assert_true(roaring_bitmap_equals(rb1_minus_rb2, temp));
    roaring_bitmap_free(temp);

    temp = roaring_bitmap_andnot(rb1, rb3);
    assert_true(roaring_bitmap_equals(rb1_minus_rb3, temp));
    roaring_bitmap_free(temp);

    temp = roaring_bitmap_andnot(rb2, rb1);
    assert_true(roaring_bitmap_equals(rb2_minus_rb1, temp));
    roaring_bitmap_free(temp);

    temp = roaring_bitmap_andnot(rb3, rb1);
    assert_true(roaring_bitmap_equals(rb3_minus_rb1, temp));
    roaring_bitmap_free(temp);

    temp = roaring_bitmap_andnot(rb3, rb2);
    assert_true(roaring_bitmap_equals(rb3_minus_rb2, temp));
    roaring_bitmap_free(temp);

    roaring_bitmap_t *large_run_bitmap =
        roaring_bitmap_from_range(2, 11 * 65536 + 27, 1);
    temp = roaring_bitmap_andnot(rb1, large_run_bitmap);

    int d1_minus_largerun[] = {
        1, 65536 * 16 + 1, 65536 * 16 + 2, 65536 * 20 + 1, 65536 * 21 + 1, -1};
    roaring_bitmap_t *rb1_minus_largerun =
        roaring_from_sentinel_array(d1_minus_largerun, copy_on_write);
    assert_true(roaring_bitmap_equals(rb1_minus_largerun, temp));
    roaring_bitmap_free(temp);

    roaring_bitmap_free(rb1);
    roaring_bitmap_free(rb2);
    roaring_bitmap_free(rb3);
    roaring_bitmap_free(rb1_minus_rb2);
    roaring_bitmap_free(rb1_minus_rb3);
    roaring_bitmap_free(rb2_minus_rb1);
    roaring_bitmap_free(rb3_minus_rb1);
    roaring_bitmap_free(rb3_minus_rb2);
    roaring_bitmap_free(rb1_minus_largerun);
    roaring_bitmap_free(large_run_bitmap);

    for (uint32_t i = 0; i < 300; ++i) {
        if (i % 2 == 0) roaring_bitmap_add(r1, i);
        if (i % 3 == 0) roaring_bitmap_add(r2, i);
    }

    roaring_bitmap_t *r1_andnot_r2 = roaring_bitmap_andnot(r1, r2);
    roaring_bitmap_set_copy_on_write(r1_andnot_r2, copy_on_write);

    int ansctr = 0;
    for (int i = 0; i < 300; ++i) {
        if ((i % 2 == 0) && (i % 3 != 0)) {
            ansctr++;
            if (!roaring_bitmap_contains(r1_andnot_r2, i))
                printf("missing %d\n", i);
        } else if (roaring_bitmap_contains(r1_andnot_r2, i))
            printf("surplus %d\n", i);
    }

    assert_int_equal(roaring_bitmap_get_cardinality(r1_andnot_r2), ansctr);
    roaring_bitmap_free(r1_andnot_r2);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);

    // some tougher tests on synthetic data

    roaring_bitmap_t *r[] = {
        // ascending density, last containers might be runs
        gen_bitmap(0.0, 1e-6, 1, 0, 0, 1000000),
        // descending density, first containers might be runs
        gen_bitmap(1.0, -1e-6, 1, 0, 0, 1000000),
        // uniformly rather sparse
        gen_bitmap(1e-5, 0.0, 1, 0, 0, 2000000),
        // uniformly rather sparse with runs
        gen_bitmap(1e-5, 0.0, 3, 0, 0, 2000000),
        // uniformly rather dense
        gen_bitmap(1e-1, 0.0, 1, 0, 0, 2000000),
        // ascending density but never too dense
        gen_bitmap(0.001, 1e-7, 1, 0, 0, 1000000),
        // ascending density but very sparse
        gen_bitmap(0.0, 1e-10, 1, 0, 0, 1000000),
        // descending with a gap
        gen_bitmap(0.5, -1e-6, 1, 600000, 800000, 1000000),
        //  gap elsewhere
        gen_bitmap(1, -1e-6, 1, 300000, 500000, 1000000),
        0  // sentinel
    };

    for (int i = 0; r[i]; ++i) {
        for (int j = i; r[j]; ++j) {
            roaring_bitmap_t *expected = synthesized_andnot(r[i], r[j]);
            roaring_bitmap_t *result = roaring_bitmap_andnot(r[i], r[j]);

            bool is_equal = roaring_bitmap_equals(expected, result);

            assert_true(is_equal);
            roaring_bitmap_free(expected);
            roaring_bitmap_free(result);
        }
    }
    for (int i = 0; r[i]; ++i) roaring_bitmap_free(r[i]);
}

DEFINE_TEST(test_andnot_true) { test_andnot(true); }

DEFINE_TEST(test_andnot_false) { test_andnot(false); }

void test_andnot_inplace(bool copy_on_write) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r2, copy_on_write);

    int data1[] = {1,
                   2,
                   65536 * 2 + 1,
                   65536 * 2 + 2,
                   65536 * 3 + 1,
                   65536 * 3 + 2,
                   65536 * 10 + 1,
                   65536 * 10 + 2,
                   65536 * 16 + 1,
                   65536 * 16 + 2,
                   65536 * 20 + 1,
                   65536 * 21 + 1,
                   -1};
    roaring_bitmap_t *rb1 = roaring_from_sentinel_array(data1, copy_on_write);
    int data2[] = {2,
                   3,
                   65536 * 10 + 2,
                   65536 * 10 + 3,
                   65536 * 12 + 2,
                   65536 * 12 + 3,
                   65536 * 14 + 2,
                   65536 * 14 + 3,
                   65536 * 16 + 2,
                   65536 * 16 + 3,
                   -1};
    roaring_bitmap_t *rb2 = roaring_from_sentinel_array(data2, copy_on_write);

    int data3[] = {2,
                   3,
                   65536 * 10 + 1,
                   65536 * 10 + 2,
                   65536 * 12 + 2,
                   65536 * 12 + 3,
                   65536 * 14 + 2,
                   65536 * 14 + 3,
                   65536 * 16 + 2,
                   65536 * 16 + 3,
                   -1};
    roaring_bitmap_t *rb3 = roaring_from_sentinel_array(data3, copy_on_write);
    int d1_minus_d2[] = {1,
                         65536 * 2 + 1,
                         65536 * 2 + 2,
                         65536 * 3 + 1,
                         65536 * 3 + 2,
                         65536 * 10 + 1,
                         65536 * 16 + 1,
                         65536 * 20 + 1,
                         65536 * 21 + 1,
                         -1};
    roaring_bitmap_t *rb1_minus_rb2 =
        roaring_from_sentinel_array(d1_minus_d2, copy_on_write);

    int d1_minus_d3[] = {1, 65536 * 2 + 1, 65536 * 2 + 2, 65536 * 3 + 1,
                         65536 * 3 + 2,
                         // 65536*10+1,
                         65536 * 16 + 1, 65536 * 20 + 1, 65536 * 21 + 1, -1};
    roaring_bitmap_t *rb1_minus_rb3 =
        roaring_from_sentinel_array(d1_minus_d3, copy_on_write);

    int d2_minus_d1[] = {3,
                         65536 * 10 + 3,
                         65536 * 12 + 2,
                         65536 * 12 + 3,
                         65536 * 14 + 2,
                         65536 * 14 + 3,
                         65536 * 16 + 3,
                         -1};

    roaring_bitmap_t *rb2_minus_rb1 =
        roaring_from_sentinel_array(d2_minus_d1, copy_on_write);

    int d3_minus_d1[] = {3,
                         // 65536*10+3,
                         65536 * 12 + 2, 65536 * 12 + 3, 65536 * 14 + 2,
                         65536 * 14 + 3, 65536 * 16 + 3, -1};
    roaring_bitmap_t *rb3_minus_rb1 =
        roaring_from_sentinel_array(d3_minus_d1, copy_on_write);

    int d3_minus_d2[] = {65536 * 10 + 1, -1};
    roaring_bitmap_t *rb3_minus_rb2 =
        roaring_from_sentinel_array(d3_minus_d2, copy_on_write);

    roaring_bitmap_t *cpy = roaring_bitmap_copy(rb1);
    roaring_bitmap_andnot_inplace(cpy, rb2);
    assert_true(roaring_bitmap_equals(rb1_minus_rb2, cpy));
    roaring_bitmap_free(cpy);

    cpy = roaring_bitmap_copy(rb1);
    roaring_bitmap_andnot_inplace(cpy, rb3);
    assert_true(roaring_bitmap_equals(rb1_minus_rb3, cpy));
    roaring_bitmap_free(cpy);

    cpy = roaring_bitmap_copy(rb2);
    roaring_bitmap_andnot_inplace(cpy, rb1);
    assert_true(roaring_bitmap_equals(rb2_minus_rb1, cpy));
    roaring_bitmap_free(cpy);

    cpy = roaring_bitmap_copy(rb3);
    roaring_bitmap_andnot_inplace(cpy, rb1);
    assert_true(roaring_bitmap_equals(rb3_minus_rb1, cpy));
    roaring_bitmap_free(cpy);

    cpy = roaring_bitmap_copy(rb3);
    roaring_bitmap_andnot_inplace(cpy, rb2);
    assert_true(roaring_bitmap_equals(rb3_minus_rb2, cpy));
    roaring_bitmap_free(cpy);

    roaring_bitmap_t *large_run_bitmap =
        roaring_bitmap_from_range(2, 11 * 65536 + 27, 1);

    cpy = roaring_bitmap_copy(rb1);
    roaring_bitmap_andnot_inplace(cpy, large_run_bitmap);

    int d1_minus_largerun[] = {
        1, 65536 * 16 + 1, 65536 * 16 + 2, 65536 * 20 + 1, 65536 * 21 + 1, -1};
    roaring_bitmap_t *rb1_minus_largerun =
        roaring_from_sentinel_array(d1_minus_largerun, copy_on_write);
    assert_true(roaring_bitmap_equals(rb1_minus_largerun, cpy));
    roaring_bitmap_free(cpy);

    roaring_bitmap_free(rb1);
    roaring_bitmap_free(rb2);
    roaring_bitmap_free(rb3);
    roaring_bitmap_free(rb1_minus_rb2);
    roaring_bitmap_free(rb1_minus_rb3);
    roaring_bitmap_free(rb2_minus_rb1);
    roaring_bitmap_free(rb3_minus_rb1);
    roaring_bitmap_free(rb3_minus_rb2);
    roaring_bitmap_free(rb1_minus_largerun);
    roaring_bitmap_free(large_run_bitmap);

    int diff_cardinality = 0;
    for (uint32_t i = 0; i < 300; ++i) {
        if (i % 2 == 0) roaring_bitmap_add(r1, i);
        if (i % 3 == 0) roaring_bitmap_add(r2, i);
        if ((i % 2 == 0) && (i % 3 != 0)) ++diff_cardinality;
    }
    roaring_bitmap_andnot_inplace(r1, r2);
    assert_int_equal(roaring_bitmap_get_cardinality(r1), diff_cardinality);

    roaring_bitmap_free(r2);
    roaring_bitmap_free(r1);

    // some tougher tests on synthetic data

    roaring_bitmap_t *r[] = {
        // ascending density, last containers might be runs
        gen_bitmap(0.0, 1e-6, 1, 0, 0, 1000000),
        // descending density, first containers might be runs
        gen_bitmap(1.0, -1e-6, 1, 0, 0, 1000000),
        // uniformly rather sparse
        gen_bitmap(1e-5, 0.0, 1, 0, 0, 2000000),
        // uniformly rather sparse with runs
        gen_bitmap(1e-5, 0.0, 3, 0, 0, 2000000),
        // uniformly rather dense
        gen_bitmap(1e-1, 0.0, 1, 0, 0, 2000000),
        // ascending density but never too dense
        gen_bitmap(0.001, 1e-7, 1, 0, 0, 1000000),
        // ascending density but very sparse
        gen_bitmap(0.0, 1e-10, 1, 0, 0, 1000000),
        // descending with a gap
        gen_bitmap(0.5, -1e-6, 1, 600000, 800000, 1000000),
        //  gap elsewhere
        gen_bitmap(1, -1e-6, 1, 300000, 500000, 1000000),
        0  // sentinel
    };

    for (int i = 0; r[i]; ++i) {
        for (int j = i + 1; r[j]; ++j) {
            roaring_bitmap_t *expected = synthesized_andnot(r[i], r[j]);
            roaring_bitmap_t *copy = roaring_bitmap_copy(r[i]);
            roaring_bitmap_set_copy_on_write(copy, copy_on_write);

            roaring_bitmap_andnot_inplace(copy, r[j]);

            bool is_equal = roaring_bitmap_equals(expected, copy);
            if (!is_equal) {
                printf("problem with i=%d j=%d\n", i, j);
                printf("copy's cardinality  is %d and expected's is %d\n",
                       (int)roaring_bitmap_get_cardinality(copy),
                       (int)roaring_bitmap_get_cardinality(expected));
                show_difference(copy, expected);
            }

            assert_true(is_equal);
            roaring_bitmap_free(expected);
            roaring_bitmap_free(copy);
        }
    }
    for (int i = 0; r[i]; ++i) roaring_bitmap_free(r[i]);
}

DEFINE_TEST(test_andnot_inplace_true) { test_andnot_inplace(true); }

DEFINE_TEST(test_andnot_inplace_false) { test_xor_inplace(false); }

static roaring_bitmap_t *make_roaring_from_array(uint32_t *a, int len) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    for (int i = 0; i < len; ++i) roaring_bitmap_add(r1, a[i]);
    return r1;
}

DEFINE_TEST(test_conversion_to_int_array) {
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // a dense bitmap container  (best done with runs)
    for (uint32_t i = 0; i < 50000; ++i) {
        if (i != 30000) {  // making 2 runs
            ans[ans_ctr++] = i;
        }
    }

    // a sparse one
    for (uint32_t i = 70000; i < 130000; i += 17) {
        ans[ans_ctr++] = i;
    }

    // a dense one but not good for runs

    for (uint32_t i = 65536 * 3; i < 65536 * 4; i++) {
        if (i % 3 != 0) {
            ans[ans_ctr++] = i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

DEFINE_TEST(test_conversion_to_int_array_with_runoptimize) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // a dense bitmap container  (best done with runs)
    for (uint32_t i = 0; i < 50000; ++i) {
        if (i != 30000) {  // making 2 runs
            ans[ans_ctr++] = i;
        }
    }

    // a sparse one
    for (uint32_t i = 70000; i < 130000; i += 17) {
        ans[ans_ctr++] = i;
    }

    // a dense one but not good for runs

    for (uint32_t i = 65536 * 3; i < 65536 * 4; i++) {
        if (i % 3 != 0) {
            ans[ans_ctr++] = i;
        }
    }
    roaring_bitmap_free(r1);

    r1 = make_roaring_from_array(ans, ans_ctr);
    assert_true(roaring_bitmap_run_optimize(r1));

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

DEFINE_TEST(test_array_to_run) {
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // array container  (best done with runs)
    for (uint32_t i = 0; i < 500; ++i) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_true(roaring_bitmap_run_optimize(r1));

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

DEFINE_TEST(test_array_to_self) {
    int ans_ctr = 0;

    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // array container  (best not done with runs)
    for (uint32_t i = 0; i < 500; i += 2) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_false(roaring_bitmap_run_optimize(r1));

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

DEFINE_TEST(test_bitset_to_self) {
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // bitset container  (best not done with runs)
    for (uint32_t i = 0; i < 50000; i += 2) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_false(roaring_bitmap_run_optimize(r1));

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

DEFINE_TEST(test_bitset_to_run) {
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // bitset container  (best done with runs)
    for (uint32_t i = 0; i < 50000; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_true(roaring_bitmap_run_optimize(r1));

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

// not sure how to get containers that are runcontainers but not efficient

DEFINE_TEST(test_run_to_self) {
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // bitset container  (best done with runs)
    for (uint32_t i = 0; i < 50000; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    bool b = roaring_bitmap_run_optimize(r1);  // will make a run container
    b = roaring_bitmap_run_optimize(r1);       // we hope it will keep it
    assert_true(b);  // still true there is a runcontainer

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

DEFINE_TEST(test_remove_run_to_bitset) {
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // bitset container  (best done with runs)
    for (uint32_t i = 0; i < 50000; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_true(roaring_bitmap_run_optimize(r1));  // will make a run container
    assert_true(roaring_bitmap_remove_run_compression(r1));  // removal done
    assert_true(
        roaring_bitmap_run_optimize(r1));  // there is again a run container

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

DEFINE_TEST(test_remove_run_to_array) {
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // array  (best done with runs)
    for (uint32_t i = 0; i < 500; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    assert_true(roaring_bitmap_run_optimize(r1));  // will make a run container
    assert_true(roaring_bitmap_remove_run_compression(r1));  // removal done
    assert_true(
        roaring_bitmap_run_optimize(r1));  // there is again a run container

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    free(arr);
    free(ans);
}

DEFINE_TEST(test_remove_run_to_bitset_cow) {
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // bitset container  (best done with runs)
    for (uint32_t i = 0; i < 50000; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    roaring_bitmap_set_copy_on_write(r1, true);
    assert_true(roaring_bitmap_run_optimize(r1));  // will make a run container
    roaring_bitmap_t *r2 = roaring_bitmap_copy(r1);
    assert_true(roaring_bitmap_remove_run_compression(r1));  // removal done
    assert_true(roaring_bitmap_remove_run_compression(r2));  // removal done
    assert_true(
        roaring_bitmap_run_optimize(r1));  // there is again a run container

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    free(arr);
    free(ans);
}

DEFINE_TEST(test_remove_run_to_array_cow) {
    int ans_ctr = 0;
    uint32_t *ans = (uint32_t *)calloc(100000, sizeof(int32_t));

    // array  (best done with runs)
    for (uint32_t i = 0; i < 500; i++) {
        if (i != 300) {  // making 2 runs
            ans[ans_ctr++] = 65536 + i;
        }
    }

    roaring_bitmap_t *r1 = make_roaring_from_array(ans, ans_ctr);
    roaring_bitmap_set_copy_on_write(r1, true);
    assert_true(roaring_bitmap_run_optimize(r1));  // will make a run container
    roaring_bitmap_t *r2 = roaring_bitmap_copy(r1);
    assert_true(roaring_bitmap_remove_run_compression(r1));  // removal done
    assert_true(roaring_bitmap_remove_run_compression(r2));  // removal done
    assert_true(
        roaring_bitmap_run_optimize(r1));  // there is again a run container

    uint64_t card = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr);

    assert_true(array_equals(arr, (int)card, ans, ans_ctr));
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    free(arr);
    free(ans);
}

// array in, array out
DEFINE_TEST(test_negation_array0) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    roaring_bitmap_t *notted_r1 = roaring_bitmap_flip(r1, 200U, 500U);
    assert_non_null(notted_r1);
    assert_int_equal(300, roaring_bitmap_get_cardinality(notted_r1));

    roaring_bitmap_free(notted_r1);
    roaring_bitmap_free(r1);
}

// array in, array out
DEFINE_TEST(test_negation_array1) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    roaring_bitmap_add(r1, 1);
    roaring_bitmap_add(r1, 2);
    // roaring_bitmap_add(r1,3);
    roaring_bitmap_add(r1, 4);
    roaring_bitmap_add(r1, 5);
    roaring_bitmap_t *notted_r1 = roaring_bitmap_flip(r1, 2U, 5U);
    assert_non_null(notted_r1);
    assert_int_equal(3, roaring_bitmap_get_cardinality(notted_r1));

    roaring_bitmap_free(notted_r1);
    roaring_bitmap_free(r1);
}

// arrays to bitmaps and runs
DEFINE_TEST(test_negation_array2) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
    }

    assert_int_equal(roaring_bitmap_get_cardinality(r1), 200);

    // get the first batch of ones but not the second
    roaring_bitmap_t *notted_r1 = roaring_bitmap_flip(r1, 0U, 100000U);
    assert_non_null(notted_r1);

    // lose 100 for key 0, but gain 100 for key 5
    assert_int_equal(100000, roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // flip all ones and beyond
    notted_r1 = roaring_bitmap_flip(r1, 0U, 1000000U);
    assert_non_null(notted_r1);
    assert_int_equal(1000000 - 200, roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // Flip some bits in the middle
    notted_r1 = roaring_bitmap_flip(r1, 100000U, 200000U);
    assert_non_null(notted_r1);
    assert_int_equal(100000 + 200, roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // flip almost all of the bits, end at an even boundary
    notted_r1 = roaring_bitmap_flip(r1, 1U, 65536 * 6);
    assert_non_null(notted_r1);
    assert_int_equal(65536 * 6 - 200 + 1,
                     roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // flip first bunch of the bits, end at an even boundary
    notted_r1 = roaring_bitmap_flip(r1, 1U, 65536 * 5);
    assert_non_null(notted_r1);
    assert_int_equal(65536 * 5 - 100 + 1 + 100,
                     roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    roaring_bitmap_free(r1);
}

// bitmaps to bitmaps and runs
DEFINE_TEST(test_negation_bitset1) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 25000; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
    }

    assert_int_equal(roaring_bitmap_get_cardinality(r1), 50000);

    // get the first batch of ones but not the second
    roaring_bitmap_t *notted_r1 = roaring_bitmap_flip(r1, 0U, 100000U);
    assert_non_null(notted_r1);

    // lose 25000 for key 0, but gain 25000 for key 5
    assert_int_equal(100000, roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // flip all ones and beyond
    notted_r1 = roaring_bitmap_flip(r1, 0U, 1000000U);
    assert_non_null(notted_r1);
    assert_int_equal(1000000 - 50000,
                     roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // Flip some bits in the middle
    notted_r1 = roaring_bitmap_flip(r1, 100000U, 200000U);
    assert_non_null(notted_r1);
    assert_int_equal(100000 + 50000, roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // flip almost all of the bits, end at an even boundary
    notted_r1 = roaring_bitmap_flip(r1, 1U, 65536 * 6);
    assert_non_null(notted_r1);
    assert_int_equal(65536 * 6 - 50000 + 1,
                     roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // flip first bunch of the bits, end at an even boundary
    notted_r1 = roaring_bitmap_flip(r1, 1U, 65536 * 5);
    assert_non_null(notted_r1);
    assert_int_equal(65536 * 5 - 25000 + 1 + 25000,
                     roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    roaring_bitmap_free(r1);
}

void test_negation_helper(bool runopt, uint32_t gap) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 65536; ++i) {
        if (i % 147 < gap) continue;
        roaring_bitmap_add(r1, i);
        roaring_bitmap_add(r1, 5 * 65536 + i);
    }
    if (runopt) {
        bool hasrun = roaring_bitmap_run_optimize(r1);
        assert_true(hasrun);
    }

    int orig_card = (int)roaring_bitmap_get_cardinality(r1);

    // get the first batch of ones but not the second
    roaring_bitmap_t *notted_r1 = roaring_bitmap_flip(r1, 0U, 100000U);
    assert_non_null(notted_r1);

    // lose some for key 0, but gain same num for key 5
    assert_int_equal(100000, roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // flip all ones and beyond
    notted_r1 = roaring_bitmap_flip(r1, 0U, 1000000U);
    assert_non_null(notted_r1);
    assert_int_equal(1000000 - orig_card,
                     roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // Flip some bits in the middle
    notted_r1 = roaring_bitmap_flip(r1, 100000U, 200000U);
    assert_non_null(notted_r1);
    assert_int_equal(100000 + orig_card,
                     roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // flip almost all of the bits, end at an even boundary
    notted_r1 = roaring_bitmap_flip(r1, 1U, 65536 * 6);
    assert_non_null(notted_r1);
    assert_int_equal((65536 * 6 - 1) - orig_card,
                     roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    // flip first bunch of the bits, end at an even boundary
    notted_r1 = roaring_bitmap_flip(r1, 1U, 65536 * 5);
    assert_non_null(notted_r1);
    assert_int_equal(65536 * 5 - 1 - (orig_card / 2) + (orig_card / 2),
                     roaring_bitmap_get_cardinality(notted_r1));
    roaring_bitmap_free(notted_r1);

    roaring_bitmap_free(r1);
}

// bitmaps to arrays and runs
DEFINE_TEST(test_negation_bitset2) { test_negation_helper(false, 2); }

// runs to arrays
DEFINE_TEST(test_negation_run1) { test_negation_helper(true, 1); }

// runs to runs
DEFINE_TEST(test_negation_run2) { test_negation_helper(true, 30); }

/* Now, same thing except inplace.  At this level, cannot really know if
 * inplace
 * done */

// array in, array out
DEFINE_TEST(test_inplace_negation_array0) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    roaring_bitmap_flip_inplace(r1, 200U, 500U);
    assert_non_null(r1);
    assert_int_equal(300, roaring_bitmap_get_cardinality(r1));

    roaring_bitmap_free(r1);
}

// array in, array out
DEFINE_TEST(test_inplace_negation_array1) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    roaring_bitmap_add(r1, 1);
    roaring_bitmap_add(r1, 2);

    roaring_bitmap_add(r1, 4);
    roaring_bitmap_add(r1, 5);
    roaring_bitmap_flip_inplace(r1, 2U, 5U);
    assert_non_null(r1);
    assert_int_equal(3, roaring_bitmap_get_cardinality(r1));

    roaring_bitmap_free(r1);
}

// arrays to bitmaps and runs
DEFINE_TEST(test_inplace_negation_array2) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
    }
    roaring_bitmap_t *r1_orig = roaring_bitmap_copy(r1);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), 200);

    // get the first batch of ones but not the second
    roaring_bitmap_flip_inplace(r1, 0U, 100000U);
    assert_non_null(r1);

    // lose 100 for key 0, but gain 100 for key 5
    assert_int_equal(100000, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);
    r1 = roaring_bitmap_copy(r1_orig);

    // flip all ones and beyond
    roaring_bitmap_flip_inplace(r1, 0U, 1000000U);
    assert_non_null(r1);
    assert_int_equal(1000000 - 200, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);
    r1 = roaring_bitmap_copy(r1_orig);

    // Flip some bits in the middle
    roaring_bitmap_flip_inplace(r1, 100000U, 200000U);
    assert_non_null(r1);
    assert_int_equal(100000 + 200, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);
    r1 = roaring_bitmap_copy(r1_orig);

    // flip almost all of the bits, end at an even boundary
    roaring_bitmap_flip_inplace(r1, 1U, 65536 * 6);
    assert_non_null(r1);
    assert_int_equal(65536 * 6 - 200 + 1, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);
    r1 = roaring_bitmap_copy(r1_orig);

    // flip first bunch of the bits, end at an even boundary
    roaring_bitmap_flip_inplace(r1, 1U, 65536 * 5);
    assert_non_null(r1);
    assert_int_equal(65536 * 5 - 100 + 1 + 100,
                     roaring_bitmap_get_cardinality(r1));
    /* */
    roaring_bitmap_free(r1_orig);
    roaring_bitmap_free(r1);
}

// bitmaps to bitmaps and runs
DEFINE_TEST(test_inplace_negation_bitset1) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 25000; ++i) {
        roaring_bitmap_add(r1, 2 * i);
        roaring_bitmap_add(r1, 5 * 65536 + 2 * i);
    }

    roaring_bitmap_t *r1_orig = roaring_bitmap_copy(r1);

    assert_int_equal(roaring_bitmap_get_cardinality(r1), 50000);

    // get the first batch of ones but not the second
    roaring_bitmap_flip_inplace(r1, 0U, 100000U);
    assert_non_null(r1);

    // lose 25000 for key 0, but gain 25000 for key 5
    assert_int_equal(100000, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);
    r1 = roaring_bitmap_copy(r1_orig);

    // flip all ones and beyond
    roaring_bitmap_flip_inplace(r1, 0U, 1000000U);
    assert_non_null(r1);
    assert_int_equal(1000000 - 50000, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);
    r1 = roaring_bitmap_copy(r1_orig);

    // Flip some bits in the middle
    roaring_bitmap_flip_inplace(r1, 100000U, 200000U);
    assert_non_null(r1);
    assert_int_equal(100000 + 50000, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);
    r1 = roaring_bitmap_copy(r1_orig);

    // flip almost all of the bits, end at an even boundary
    roaring_bitmap_flip_inplace(r1, 1U, 65536 * 6);
    assert_non_null(r1);
    assert_int_equal(65536 * 6 - 50000 + 1, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);
    r1 = roaring_bitmap_copy(r1_orig);

    // flip first bunch of the bits, end at an even boundary
    roaring_bitmap_flip_inplace(r1, 1U, 65536 * 5);
    assert_non_null(r1);
    assert_int_equal(65536 * 5 - 25000 + 1 + 25000,
                     roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);

    roaring_bitmap_free(r1_orig);
}

void test_inplace_negation_helper(bool runopt, uint32_t gap) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t i = 0; i < 65536; ++i) {
        if (i % 147 < gap) continue;
        roaring_bitmap_add(r1, i);
        roaring_bitmap_add(r1, 5 * 65536 + i);
    }
    if (runopt) {
        bool hasrun = roaring_bitmap_run_optimize(r1);
        assert_true(hasrun);
    }

    int orig_card = (int)roaring_bitmap_get_cardinality(r1);
    roaring_bitmap_t *r1_orig = roaring_bitmap_copy(r1);

    // get the first batch of ones but not the second
    roaring_bitmap_flip_inplace(r1, 0U, 100000U);
    assert_non_null(r1);

    // lose some for key 0, but gain same num for key 5
    assert_int_equal(100000, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);

    // flip all ones and beyond
    r1 = roaring_bitmap_copy(r1_orig);
    roaring_bitmap_flip_inplace(r1, 0U, 1000000U);
    assert_non_null(r1);
    assert_int_equal(1000000 - orig_card, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);

    // Flip some bits in the middle
    r1 = roaring_bitmap_copy(r1_orig);
    roaring_bitmap_flip_inplace(r1, 100000U, 200000U);
    assert_non_null(r1);
    assert_int_equal(100000 + orig_card, roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);

    // flip almost all of the bits, end at an even boundary
    r1 = roaring_bitmap_copy(r1_orig);
    roaring_bitmap_flip_inplace(r1, 1U, 65536 * 6);
    assert_non_null(r1);
    assert_int_equal((65536 * 6 - 1) - orig_card,
                     roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);

    // flip first bunch of the bits, end at an even boundary
    r1 = roaring_bitmap_copy(r1_orig);
    roaring_bitmap_flip_inplace(r1, 1U, 65536 * 5);
    assert_non_null(r1);
    assert_int_equal(65536 * 5 - 1 - (orig_card / 2) + (orig_card / 2),
                     roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);

    roaring_bitmap_free(r1_orig);
}

// bitmaps to arrays and runs
DEFINE_TEST(test_inplace_negation_bitset2) {
    test_inplace_negation_helper(false, 2);
}

// runs to arrays
DEFINE_TEST(test_inplace_negation_run1) {
    test_inplace_negation_helper(true, 1);
}

// runs to runs
DEFINE_TEST(test_inplace_negation_run2) {
    test_inplace_negation_helper(true, 30);
}

// runs to bitmaps is hard to do.
// TODO it

DEFINE_TEST(test_rand_flips) {
    srand(1234);
    const int min_runs = 1;
    const int flip_trials = 5;  // these are expensive tests
    const int range = 2000000;
    char *input = (char *)malloc(range);
    char *output = (char *)malloc(range);

    for (int card = 2; card < 1000000; card *= 8) {
        printf("test_rand_flips with attempted card %d", card);

        roaring_bitmap_t *r = roaring_bitmap_create();
        memset(input, 0, range);
        for (int i = 0; i < card; ++i) {
            double f1 = our_rand() / (double)OUR_RAND_MAX;
            double f2 = our_rand() / (double)OUR_RAND_MAX;
            double f3 = our_rand() / (double)OUR_RAND_MAX;
            int pos = (int)(f1 * f2 * f3 *
                            range);  // denser at the start, sparser at end
            assert_true(pos < range);
            assert_true(pos >= 0);
            roaring_bitmap_add(r, pos);
            input[pos] = 1;
        }
        for (int i = 0; i < min_runs; ++i) {
            int startpos = our_rand() % (range / 2);
            for (int j = startpos; j < startpos + 65536 * 2; ++j)
                if (j % 147 < 100) {
                    roaring_bitmap_add(r, j);
                    input[j] = 1;
                }
        }
        roaring_bitmap_run_optimize(r);
        printf(" and actual card = %d\n",
               (int)roaring_bitmap_get_cardinality(r));

        for (int i = 0; i < flip_trials; ++i) {
            int start = our_rand() % (range - 1);
            int len = our_rand() % (range - start);
            roaring_bitmap_t *ans = roaring_bitmap_flip(r, start, start + len);
            memcpy(output, input, range);
            for (int j = start; j < start + len; ++j) output[j] = 1 - input[j];

            // verify answer
            for (int j = 0; j < range; ++j) {
                assert_true(((bool)output[j]) ==
                            roaring_bitmap_contains(ans, j));
            }

            roaring_bitmap_free(ans);
        }
        roaring_bitmap_free(r);
    }
    free(output);
    free(input);
}

// randomized flipping test - inplace version
DEFINE_TEST(test_inplace_rand_flips) {
    srand(1234);
    const int min_runs = 1;
    const int flip_trials = 5;  // these are expensive tests
    const int range = 2000000;
    char *input = (char *)malloc(range);
    char *output = (char *)malloc(range);

    for (int card = 2; card < 1000000; card *= 8) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        memset(input, 0, range);
        for (int i = 0; i < card; ++i) {
            double f1 = our_rand() / (double)OUR_RAND_MAX;
            double f2 = our_rand() / (double)OUR_RAND_MAX;
            double f3 = our_rand() / (double)OUR_RAND_MAX;
            int pos = (int)(f1 * f2 * f3 *
                            range);  // denser at the start, sparser at end
            assert_true(pos < range);
            assert_true(pos >= 0);
            roaring_bitmap_add(r, pos);
            input[pos] = 1;
        }
        for (int i = 0; i < min_runs; ++i) {
            int startpos = our_rand() % (range / 2);
            for (int j = startpos; j < startpos + 65536 * 2; ++j)
                if (j % 147 < 100) {
                    roaring_bitmap_add(r, j);
                    input[j] = 1;
                }
        }
        roaring_bitmap_run_optimize(r);

        roaring_bitmap_t *r_orig = roaring_bitmap_copy(r);

        for (int i = 0; i < flip_trials; ++i) {
            int start = our_rand() % (range - 1);
            int len = our_rand() % (range - start);

            roaring_bitmap_flip_inplace(r, start, start + len);
            memcpy(output, input, range);
            for (int j = start; j < start + len; ++j) output[j] = 1 - input[j];

            // verify answer
            for (int j = 0; j < range; ++j) {
                assert_true(((bool)output[j]) == roaring_bitmap_contains(r, j));
            }

            roaring_bitmap_free(r);
            r = roaring_bitmap_copy(r_orig);
        }
        roaring_bitmap_free(r_orig);
        roaring_bitmap_free(r);
    }
    free(output);
    free(input);
}

DEFINE_TEST(test_flip_array_container_removal) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    for (unsigned val = 0; val < 100; val++) {
        roaring_bitmap_add(bm, val);
    }
    roaring_bitmap_flip_inplace(bm, 0, 100);
    roaring_bitmap_free(bm);
}

DEFINE_TEST(test_flip_bitset_container_removal) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    for (unsigned val = 0; val < 10000; val++) {
        roaring_bitmap_add(bm, val);
    }
    roaring_bitmap_flip_inplace(bm, 0, 10000);
    roaring_bitmap_free(bm);
}

DEFINE_TEST(test_flip_run_container_removal) {
    roaring_bitmap_t *bm = roaring_bitmap_from_range(0, 10000, 1);
    roaring_bitmap_flip_inplace(bm, 0, 10000);
    roaring_bitmap_free(bm);
}

DEFINE_TEST(test_flip_run_container_removal2) {
    roaring_bitmap_t *bm = roaring_bitmap_from_range(0, 66002, 1);
    roaring_bitmap_flip_inplace(bm, 0, 987653576);
    roaring_bitmap_free(bm);
}

// randomized test for rank query
DEFINE_TEST(select_test) {
    srand(1234);
    const int min_runs = 1;
    const uint32_t range = 2000000;
    char *input = (char *)malloc(range);

    for (int card = 2; card < 1000000; card *= 8) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        memset(input, 0, range);
        for (int i = 0; i < card; ++i) {
            double f1 = our_rand() / (double)OUR_RAND_MAX;
            double f2 = our_rand() / (double)OUR_RAND_MAX;
            double f3 = our_rand() / (double)OUR_RAND_MAX;
            uint32_t pos =
                (uint32_t)(f1 * f2 * f3 *
                           range);  // denser at the start, sparser at end
            assert_true(pos < range);
            roaring_bitmap_add(r, pos);
            input[pos] = 1;
        }
        for (int i = 0; i < min_runs; ++i) {
            int startpos = our_rand() % (range / 2);
            for (int j = startpos; j < startpos + 65536 * 2; ++j)
                if (j % 147 < 100) {
                    roaring_bitmap_add(r, j);
                    input[j] = 1;
                }
        }
        roaring_bitmap_run_optimize(r);
        uint64_t true_card = roaring_bitmap_get_cardinality(r);

        roaring_bitmap_set_copy_on_write(r, true);
        roaring_bitmap_t *r_copy = roaring_bitmap_copy(r);

        roaring_bitmap_t *bitmaps[] = {r, r_copy};
        for (unsigned i_bm = 0; i_bm < 2; i_bm++) {
            uint32_t rank = 0;
            uint32_t element;
            for (uint32_t i = 0; i < true_card; i++) {
                if (input[i]) {
                    assert_true(
                        roaring_bitmap_select(bitmaps[i_bm], rank, &element));
                    assert_int_equal(i, element);
                    rank++;
                }
            }
            for (uint32_t n = 0; n < 10; n++) {
                assert_false(roaring_bitmap_select(bitmaps[i_bm], true_card + n,
                                                   &element));
            }
        }

        roaring_bitmap_free(r);
        roaring_bitmap_free(r_copy);
    }
    free(input);
}

DEFINE_TEST(test_maximum_minimum) {
    for (uint32_t mymin = 123; mymin < 1000000; mymin *= 2) {
        // just arrays
        roaring_bitmap_t *r = roaring_bitmap_create();
        uint32_t x = mymin;
        for (; x < 1000 + mymin; x += 100) {
            roaring_bitmap_add(r, x);
        }
        assert_true(roaring_bitmap_minimum(r) == mymin);
        assert_true(roaring_bitmap_maximum(r) == x - 100);
        // now bitmap
        x = mymin;
        for (; x < 64000 + mymin; x += 2) {
            roaring_bitmap_add(r, x);
        }
        assert_true(roaring_bitmap_minimum(r) == mymin);
        assert_true(roaring_bitmap_maximum(r) == x - 2);
        // now run
        x = mymin;
        for (; x < 64000 + mymin; x++) {
            roaring_bitmap_add(r, x);
        }
        roaring_bitmap_run_optimize(r);
        assert_true(roaring_bitmap_minimum(r) == mymin);
        assert_true(roaring_bitmap_maximum(r) == x - 1);
        roaring_bitmap_free(r);
    }
}

static uint64_t rank(uint32_t *arr, size_t length, uint32_t x) {
    uint64_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        if (arr[i] > x) break;
        sum++;
    }
    return sum;
}

static int64_t get_index(uint32_t *arr, size_t length, uint32_t x) {
    for (size_t i = 0; i < length; ++i) {
        if (arr[i] == x) {
            return i;
        }
    }
    return -1;
}

DEFINE_TEST(test_rank) {
    for (uint32_t mymin = 123; mymin < 1000000; mymin *= 2) {
        // just arrays
        roaring_bitmap_t *r = roaring_bitmap_create();
        uint32_t x = mymin;
        for (; x < 1000 + mymin; x += 100) {
            roaring_bitmap_add(r, x);
        }
        uint64_t card = roaring_bitmap_get_cardinality(r);
        uint32_t *ans = (uint32_t *)malloc(card * sizeof(uint32_t));
        roaring_bitmap_to_uint32_array(r, ans);
        for (uint32_t z = 0; z < 1000 + mymin + 10; z += 10) {
            uint64_t truerank = rank(ans, card, z);
            uint64_t computedrank = roaring_bitmap_rank(r, z);
            if (truerank != computedrank)
                printf("%d != %d \n", (int)truerank, (int)computedrank);
            assert_true(truerank == computedrank);

            uint32_t input[] = {z, z + 1, z + 10, z + 100, z + 1000};
            uint64_t output[5];
            roaring_bitmap_rank_many(r, input, input + 5, output);
            for (uint32_t i = 0; i < 5; i++) {
                truerank = rank(ans, card, input[i]);
                computedrank = output[i];
                if (truerank != computedrank)
                    printf("%d != %d \n", (int)truerank, (int)computedrank);
                assert_true(truerank == computedrank);
            }
        }
        free(ans);
        // now bitmap
        x = mymin;
        for (; x < 64000 + mymin; x += 2) {
            roaring_bitmap_add(r, x);
        }
        card = roaring_bitmap_get_cardinality(r);
        ans = (uint32_t *)malloc(card * sizeof(uint32_t));
        roaring_bitmap_to_uint32_array(r, ans);
        for (uint32_t z = 0; z < 64000 + mymin + 10; z += 10) {
            uint64_t truerank = rank(ans, card, z);
            uint64_t computedrank = roaring_bitmap_rank(r, z);
            if (truerank != computedrank)
                printf("%d != %d \n", (int)truerank, (int)computedrank);
            assert_true(truerank == computedrank);

            uint32_t input[] = {z, z + 1, z + 10, z + 100, z + 1000};
            uint64_t output[5];
            roaring_bitmap_rank_many(r, input, input + 5, output);
            for (uint32_t i = 0; i < 5; i++) {
                truerank = rank(ans, card, input[i]);
                computedrank = output[i];
                if (truerank != computedrank)
                    printf("%d != %d \n", (int)truerank, (int)computedrank);
                assert_true(truerank == computedrank);
            }
        }
        free(ans);
        // now run
        x = mymin;
        for (; x < 64000 + mymin; x++) {
            roaring_bitmap_add(r, x);
        }
        roaring_bitmap_run_optimize(r);
        card = roaring_bitmap_get_cardinality(r);
        ans = (uint32_t *)malloc(card * sizeof(uint32_t));
        roaring_bitmap_to_uint32_array(r, ans);
        for (uint32_t z = 0; z < 64000 + mymin + 10; z += 10) {
            uint64_t truerank = rank(ans, card, z);
            uint64_t computedrank = roaring_bitmap_rank(r, z);
            if (truerank != computedrank)
                printf("%d != %d \n", (int)truerank, (int)computedrank);
            assert_true(truerank == computedrank);

            uint32_t input[] = {z, z + 1, z + 10, z + 100, z + 1000};
            uint64_t output[5];
            roaring_bitmap_rank_many(r, input, input + 5, output);
            for (uint32_t i = 0; i < 5; i++) {
                truerank = rank(ans, card, input[i]);
                computedrank = output[i];
                if (truerank != computedrank)
                    printf("%d != %d \n", (int)truerank, (int)computedrank);
                assert_true(truerank == computedrank);
            }
        }
        free(ans);

        roaring_bitmap_free(r);
    }
}

DEFINE_TEST(test_get_index) {
    for (uint32_t mymin = 123; mymin < 1000000; mymin *= 2) {
        // just arrays
        roaring_bitmap_t *r = roaring_bitmap_create();
        uint32_t x = mymin;
        for (; x < 1000 + mymin; x += 100) {
            roaring_bitmap_add(r, x);
        }
        uint64_t card = roaring_bitmap_get_cardinality(r);
        uint32_t *ans = (uint32_t *)malloc(card * sizeof(uint32_t));
        roaring_bitmap_to_uint32_array(r, ans);
        for (uint32_t z = 0; z < 1000 + mymin + 10; z += 10) {
            int64_t trueidx = get_index(ans, card, z);
            int64_t computedidx = roaring_bitmap_get_index(r, z);
            if (trueidx != computedidx)
                printf("%d != %d \n", (int)trueidx, (int)computedidx);
            assert_true(trueidx == computedidx);
        }
        free(ans);
        // now bitmap
        x = mymin;
        for (; x < 64000 + mymin; x += 2) {
            roaring_bitmap_add(r, x);
        }
        card = roaring_bitmap_get_cardinality(r);
        ans = (uint32_t *)malloc(card * sizeof(uint32_t));
        roaring_bitmap_to_uint32_array(r, ans);
        for (uint32_t z = 0; z < 64000 + mymin + 10; z += 10) {
            int64_t trueidx = get_index(ans, card, z);
            int64_t computedidx = roaring_bitmap_get_index(r, z);
            if (trueidx != computedidx)
                printf("%d != %d \n", (int)trueidx, (int)computedidx);
            assert_true(trueidx == computedidx);
        }
        free(ans);
        // now run
        x = mymin;
        for (; x < 64000 + mymin; x++) {
            roaring_bitmap_add(r, x);
        }
        roaring_bitmap_run_optimize(r);
        card = roaring_bitmap_get_cardinality(r);
        ans = (uint32_t *)malloc(card * sizeof(uint32_t));
        roaring_bitmap_to_uint32_array(r, ans);
        for (uint32_t z = 0; z < 64000 + mymin + 10; z += 10) {
            int64_t trueidx = get_index(ans, card, z);
            int64_t computedidx = roaring_bitmap_get_index(r, z);
            if (trueidx != computedidx)
                printf("%d != %d \n", (int)trueidx, (int)computedidx);
            assert_true(trueidx == computedidx);
        }
        free(ans);

        roaring_bitmap_free(r);
    }
}

// Return a random value which does not belong to the roaring bitmap.
// Value will be lower than upper_bound.
uint32_t choose_missing_value(roaring_bitmap_t *rb, uint32_t upper_bound) {
    do {
        uint32_t value = our_rand() % upper_bound;
        if (!roaring_bitmap_contains(rb, value)) return value;
    } while (true);
}

DEFINE_TEST(test_intersect_small_run_bitset) {
    roaring_bitmap_t *rb1 = roaring_bitmap_from_range(0, 1, 1);
    roaring_bitmap_t *rb2 = roaring_bitmap_from_range(1, 8194, 2);
    assert_false(roaring_bitmap_intersect(rb1, rb2));
    roaring_bitmap_free(rb1);
    roaring_bitmap_free(rb2);
}

DEFINE_TEST(issue316) {
    roaring_bitmap_t *rb1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(rb1, true);
    roaring_bitmap_add_range(rb1, 1, 100);
    roaring_bitmap_t *rb2 = roaring_bitmap_create();
    roaring_bitmap_or_inplace(rb2, rb1);
    assert_true(roaring_bitmap_is_subset(rb2, rb1));
    assert_true(roaring_bitmap_is_subset(rb1, rb2));
    assert_true(roaring_bitmap_equals(rb1, rb2));
    roaring_bitmap_t *rb3 = roaring_bitmap_copy(rb2);
    assert_true(roaring_bitmap_is_subset(rb3, rb1));
    assert_true(roaring_bitmap_is_subset(rb1, rb3));
    assert_true(roaring_bitmap_equals(rb1, rb3));
    assert_true(roaring_bitmap_equals(rb1, rb2));
    roaring_bitmap_free(rb1);
    roaring_bitmap_free(rb2);
    roaring_bitmap_free(rb3);
}

DEFINE_TEST(test_subset) {
    uint32_t value;
    roaring_bitmap_t *rb1 = roaring_bitmap_create();
    roaring_bitmap_t *rb2 = roaring_bitmap_create();
    assert_true(roaring_bitmap_is_subset(rb1, rb2));
    assert_false(roaring_bitmap_is_strict_subset(rb1, rb2));
    // Sparse values
    for (int i = 0; i < 1000; i++) {
        roaring_bitmap_add(rb2, choose_missing_value(rb2, UINT32_C(1) << 31));
    }
    assert_true(roaring_bitmap_is_subset(rb1, rb2));
    assert_true(roaring_bitmap_is_strict_subset(rb1, rb2));
    roaring_bitmap_or_inplace(rb1, rb2);
    assert_true(roaring_bitmap_is_subset(rb1, rb2));
    assert_false(roaring_bitmap_is_strict_subset(rb1, rb2));
    value = choose_missing_value(rb1, UINT32_C(1) << 31);
    roaring_bitmap_add(rb1, value);
    roaring_bitmap_add(rb2, choose_missing_value(rb1, UINT32_C(1) << 31));
    assert_false(roaring_bitmap_is_subset(rb1, rb2));
    assert_false(roaring_bitmap_is_strict_subset(rb1, rb2));
    roaring_bitmap_add(rb2, value);
    assert_true(roaring_bitmap_is_subset(rb1, rb2));
    assert_true(roaring_bitmap_is_strict_subset(rb1, rb2));
    // Dense values
    for (int i = 0; i < 50000; i++) {
        value = choose_missing_value(rb2, 1 << 17);
        roaring_bitmap_add(rb1, value);
        roaring_bitmap_add(rb2, value);
    }
    assert_true(roaring_bitmap_is_subset(rb1, rb2));
    assert_true(roaring_bitmap_is_strict_subset(rb1, rb2));
    value = choose_missing_value(rb2, 1 << 16);
    roaring_bitmap_add(rb1, value);
    roaring_bitmap_add(rb2, choose_missing_value(rb1, 1 << 16));
    assert_false(roaring_bitmap_is_subset(rb1, rb2));
    assert_false(roaring_bitmap_is_strict_subset(rb1, rb2));
    roaring_bitmap_add(rb2, value);
    assert_true(roaring_bitmap_is_subset(rb1, rb2));
    assert_true(roaring_bitmap_is_strict_subset(rb1, rb2));
    roaring_bitmap_free(rb1);
    roaring_bitmap_free(rb2);
}

DEFINE_TEST(test_or_many_memory_leak) {
    for (int i = 0; i < 10; i++) {
        roaring_bitmap_t *bm1 = roaring_bitmap_create();
        for (int j = 0; j < 10; j++) {
            roaring_bitmap_t *bm2 = roaring_bitmap_create();
            const roaring_bitmap_t *buff[] = {bm1, bm2};
            roaring_bitmap_t *bm3 = roaring_bitmap_or_many(2, buff);
            roaring_bitmap_free(bm2);
            roaring_bitmap_free(bm3);
        }
        roaring_bitmap_free(bm1);
    }
}

void test_iterator_generate_data(uint32_t **values_out, uint32_t *count_out) {
    const size_t capacity = 1000 * 1000;
    uint32_t *values =
        (uint32_t *)malloc(sizeof(uint32_t) * capacity);  // ascending order
    uint32_t count = 0;
    uint32_t base = 1234;  // container index

    // min allowed value
    values[count++] = 0;

    // only the very first value in container is set
    values[count++] = base * 65536;
    base += 2;

    // only the very last value in container is set
    values[count++] = base * 65536 + 65535;
    base += 2;

    // fully filled container
    for (uint32_t i = 0; i < 65536; i++) {
        values[count++] = base * 65536 + i;
    }
    base += 2;

    // even values
    for (uint32_t i = 0; i < 65536; i += 2) {
        values[count++] = base * 65536 + i;
    }
    base += 2;

    // odd values
    for (uint32_t i = 1; i < 65536; i += 2) {
        values[count++] = base * 65536 + i;
    }
    base += 2;

    // each next 64-bit word is ROR'd by one
    for (uint32_t i = 0; i < 65536; i += 65) {
        values[count++] = base * 65536 + i;
    }
    base += 2;

    // runs of increasing length: 0, 1,0, 1,1,0, 1,1,1,0, ...
    for (uint32_t i = 0, run_index = 0; i < 65536; i++) {
        if (i != (run_index + 1) * (run_index + 2) / 2 - 1) {
            values[count++] = base * 65536 + i;
        } else {
            run_index++;
        }
    }
    base += 2;

    // 00000XX, XXXXXX, XX0000
    for (uint32_t i = 65536 - 100; i < 65536; i++) {
        values[count++] = base * 65536 + i;
    }
    base += 1;
    for (uint32_t i = 0; i < 65536; i++) {
        values[count++] = base * 65536 + i;
    }
    base += 1;
    for (uint32_t i = 0; i < 100; i++) {
        values[count++] = base * 65536 + i;
    }
    base += 2;

    // random
    for (int i = 0; i < 65536; i += our_rand() % 10 + 1) {
        values[count++] = base * 65536 + i;
    }
    base += 2;

    // max allowed value
    values[count++] = UINT32_MAX;

    assert_true(count <= capacity);
    *values_out = values;
    *count_out = count;
}

/*
 * Read bitmap in steps of given size, compare with reference values.
 * If step is UINT32_MAX (special value), then read single non-empty container
 * at a time.
 */
void read_compare(roaring_bitmap_t *r, const uint32_t *ref_values,
                  uint32_t ref_count, uint32_t step) {
    roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
    uint32_t *buffer = (uint32_t *)malloc(sizeof(uint32_t) *
                                          (step == UINT32_MAX ? 65536 : step));
    while (ref_count > 0) {
        assert_true(iter->has_value == true);
        assert_true(iter->current_value == ref_values[0]);

        uint32_t num_ask = step;
        if (step == UINT32_MAX) {
            num_ask = 0;
            for (uint32_t i = 0; i < ref_count; i++) {
                if ((ref_values[i] >> 16) == (ref_values[0] >> 16)) {
                    num_ask++;
                } else {
                    break;
                }
            }
        }

        uint32_t num_got = roaring_uint32_iterator_read(iter, buffer, num_ask);
        assert_true(num_got == minimum_uint32(num_ask, ref_count));
        for (uint32_t i = 0; i < num_got; i++) {
            assert_true(ref_values[i] == buffer[i]);
        }
        ref_values += num_got;
        ref_count -= num_got;
    }

    assert_true(iter->has_value == false);
    assert_true(iter->current_value == UINT32_MAX);

    assert_true(roaring_uint32_iterator_read(iter, buffer, step) == 0);
    assert_true(iter->has_value == false);
    assert_true(iter->current_value == UINT32_MAX);

    free(buffer);
    roaring_uint32_iterator_free(iter);
}

void test_read_uint32_iterator(uint8_t type) {
    uint32_t *ref_values;
    uint32_t ref_count;
    test_iterator_generate_data(&ref_values, &ref_count);

    roaring_bitmap_t *r = roaring_bitmap_create();
    for (uint32_t i = 0; i < ref_count; i++) {
        roaring_bitmap_add(r, ref_values[i]);
    }
    if (type != UINT8_MAX) {
        convert_all_containers(r, type);
    }

    roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
    uint32_t buffer[1];
    uint32_t got = roaring_uint32_iterator_read(iter, buffer, 0);
    assert_true(got == 0);
    assert_true(iter->has_value);
    assert_true(iter->current_value == 0);
    roaring_uint32_iterator_free(iter);

    read_compare(r, ref_values, ref_count, 1);
    read_compare(r, ref_values, ref_count, 2);
    read_compare(r, ref_values, ref_count, 7);
    read_compare(r, ref_values, ref_count, ref_count - 1);
    read_compare(r, ref_values, ref_count, ref_count);
    read_compare(r, ref_values, ref_count, UINT32_MAX);  // special value

    roaring_bitmap_free(r);
    free(ref_values);
}

DEFINE_TEST(test_read_uint32_iterator_array) {
    test_read_uint32_iterator(ARRAY_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_uint32_iterator_bitset) {
    test_read_uint32_iterator(BITSET_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_uint32_iterator_run) {
    test_read_uint32_iterator(RUN_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_uint32_iterator_native) {
    test_read_uint32_iterator(UINT8_MAX);  // special value
}

void test_previous_iterator(uint8_t type) {
    uint32_t *ref_values;
    uint32_t ref_count;
    test_iterator_generate_data(&ref_values, &ref_count);

    roaring_bitmap_t *r = roaring_bitmap_create();
    for (uint32_t i = 0; i < ref_count; i++) {
        roaring_bitmap_add(r, ref_values[i]);
    }
    if (type != UINT8_MAX) {
        convert_all_containers(r, type);
    }

    roaring_uint32_iterator_t iterator;
    roaring_iterator_init_last(r, &iterator);
    uint32_t count = 0;

    do {
        assert_true(iterator.has_value);
        ++count;
        assert_true((int64_t)ref_count - (int64_t)count >= 0);  // sanity check
        assert_true(ref_values[ref_count - count] == iterator.current_value);
    } while (roaring_uint32_iterator_previous(&iterator));

    assert_true(ref_count == count);

    roaring_bitmap_free(r);
    free(ref_values);
}

DEFINE_TEST(test_previous_iterator_array) {
    test_previous_iterator(ARRAY_CONTAINER_TYPE);
}

DEFINE_TEST(test_previous_iterator_bitset) {
    test_previous_iterator(BITSET_CONTAINER_TYPE);
}

DEFINE_TEST(test_previous_iterator_run) {
    test_previous_iterator(RUN_CONTAINER_TYPE);
}

DEFINE_TEST(test_previous_iterator_native) {
    test_previous_iterator(UINT8_MAX);  // special value
}

void test_iterator_reuse_retry_count(int retry_count) {
    uint32_t *ref_values;
    uint32_t ref_count;
    test_iterator_generate_data(&ref_values, &ref_count);

    roaring_bitmap_t *with_edges = roaring_bitmap_create();
    // We don't want min and max values inside this bitmap
    roaring_bitmap_t *without_edges = roaring_bitmap_create();

    for (uint32_t i = 0; i < ref_count; i++) {
        roaring_bitmap_add(with_edges, ref_values[i]);
        if (i != 0 && i != ref_count - 1) {
            roaring_bitmap_add(without_edges, ref_values[i]);
        }
    }

    // sanity checks
    assert_true(roaring_bitmap_contains(with_edges, 0));
    assert_true(roaring_bitmap_contains(with_edges, UINT32_MAX));
    assert_true(!roaring_bitmap_contains(without_edges, 0));
    assert_true(!roaring_bitmap_contains(without_edges, UINT32_MAX));
    assert_true(roaring_bitmap_get_cardinality(with_edges) - 2 ==
                roaring_bitmap_get_cardinality(without_edges));

    const roaring_bitmap_t *bitmaps[] = {with_edges, without_edges};
    int num_bitmaps = sizeof(bitmaps) / sizeof(bitmaps[0]);

    for (int i = 0; i < num_bitmaps; ++i) {
        roaring_uint32_iterator_t iterator;
        roaring_iterator_init(bitmaps[i], &iterator);
        assert_true(iterator.has_value);
        uint32_t first_value = iterator.current_value;

        uint32_t count = 0;
        while (iterator.has_value) {
            count++;
            roaring_uint32_iterator_advance(&iterator);
        }
        assert_true(count == roaring_bitmap_get_cardinality(bitmaps[i]));

        // Test advancing the iterator more times than necessary
        for (int retry = 0; retry < retry_count; ++retry) {
            roaring_uint32_iterator_advance(&iterator);
        }

        // Using same iterator we want to go backwards through the list
        roaring_uint32_iterator_previous(&iterator);
        count = 0;
        while (iterator.has_value) {
            count++;
            roaring_uint32_iterator_previous(&iterator);
        }
        assert_true(count == roaring_bitmap_get_cardinality(bitmaps[i]));

        // Test decrement the iterator more times than necessary
        for (int retry = 0; retry < retry_count; ++retry) {
            roaring_uint32_iterator_previous(&iterator);
        }

        roaring_uint32_iterator_advance(&iterator);
        assert_true(iterator.has_value);
        assert_true(first_value == iterator.current_value);
    }

    roaring_bitmap_free(without_edges);
    roaring_bitmap_free(with_edges);
    free(ref_values);
}

DEFINE_TEST(test_iterator_reuse) { test_iterator_reuse_retry_count(0); }

DEFINE_TEST(test_iterator_reuse_many) { test_iterator_reuse_retry_count(10); }

DEFINE_TEST(read_uint32_iterator_zero_count) {
    roaring_bitmap_t *r = roaring_bitmap_from_range(0, 10000, 1);
    roaring_uint32_iterator_t *iterator = roaring_iterator_create(r);
    uint32_t buf[1];
    uint32_t read = roaring_uint32_iterator_read(iterator, buf, 0);
    assert_true(read == 0);
    assert_true(iterator->has_value);
    assert_true(iterator->current_value == 0);
    roaring_uint32_iterator_free(iterator);
    roaring_bitmap_free(r);
}

DEFINE_TEST(test_add_range) {
    // autoconversion: BITSET -> BITSET -> RUN
    {
        sbs_t *sbs = sbs_create();
        sbs_add_value(sbs, 100);
        sbs_convert(sbs, BITSET_CONTAINER_TYPE);
        sbs_add_range(sbs, 0, 299);
        assert_true(sbs_check_type(sbs, BITSET_CONTAINER_TYPE));
        sbs_add_range(sbs, 301, 65535);
        assert_true(sbs_check_type(sbs, BITSET_CONTAINER_TYPE));
        // after and only after BITSET becomes [0, 65535], it is converted to
        // RUN
        sbs_add_range(sbs, 300, 300);
        assert_true(sbs_check_type(sbs, RUN_CONTAINER_TYPE));
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // autoconversion: ARRAY -> ARRAY -> BITSET
    {
        sbs_t *sbs = sbs_create();
        sbs_add_value(sbs, 100);
        sbs_convert(sbs, ARRAY_CONTAINER_TYPE);

        // unless threshold was hit, it is still ARRAY
        for (int i = 0; i < 100; i += 2) {
            sbs_add_value(sbs, i);
            assert_true(sbs_check_type(sbs, ARRAY_CONTAINER_TYPE));
        }

        // after threshold on number of elements was hit, it is converted to
        // BITSET
        for (int i = 0; i < 65535; i += 2) {
            sbs_add_value(sbs, i);
        }
        assert_true(sbs_check_type(sbs, BITSET_CONTAINER_TYPE));

        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // autoconversion: ARRAY -> RUN
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 0, 100);
        sbs_convert(sbs, ARRAY_CONTAINER_TYPE);

        // after ARRAY becomes full [0, 65535], it is converted to RUN
        sbs_add_range(sbs, 100, 65535);
        assert_true(sbs_check_type(sbs, RUN_CONTAINER_TYPE));

        sbs_compare(sbs);
        sbs_free(sbs);
    }
    // autoconversion: RUN -> RUN -> BITSET
    {
        sbs_t *sbs = sbs_create();
        // by default, RUN container is used
        for (int i = 0; i < 100; i += 2) {
            sbs_add_range(sbs, 4 * i, 4 * i + 1);
            assert_true(sbs_check_type(sbs, RUN_CONTAINER_TYPE));
        }
        // after number of RLE runs exceeded threshold, it is converted to
        // BITSET
        for (int i = 0; i < 65535; i += 2) {
            sbs_add_range(sbs, i, i);
        }
        assert_true(sbs_check_type(sbs, BITSET_CONTAINER_TYPE));
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // autoconversion: ARRAY -> ARRAY -> BITSET
    {
        sbs_t *sbs = sbs_create();
        for (int i = 0; i < 100; i += 2) {
            sbs_add_range(sbs, i, i);
            assert_true(sbs_check_type(sbs, ARRAY_CONTAINER_TYPE));
        }
        // after number of RLE runs exceeded threshold, it is converted to
        // BITSET
        for (int i = 0; i < 65535; i += 2) {
            sbs_add_range(sbs, i, i);
        }
        assert_true(sbs_check_type(sbs, BITSET_CONTAINER_TYPE));
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // append new container to the end
    {
        sbs_t *sbs = sbs_create();
        sbs_add_value(sbs, 5);
        sbs_add_range(sbs, 65536 + 5, 65536 + 20);
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // prepend new container to the beginning
    {
        sbs_t *sbs = sbs_create();
        sbs_add_value(sbs, 65536 * 1 + 5);
        sbs_add_range(sbs, 5, 20);
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // add new container between existing ones
    {
        sbs_t *sbs = sbs_create();
        sbs_add_value(sbs, 65536 * 0 + 5);
        sbs_add_value(sbs, 65536 * 2 + 5);
        sbs_add_range(sbs, 65536 * 1 + 5, 65536 * 1 + 20);
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // invalid range
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 200, 100);
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // random data inside [0..span)
    const uint32_t span = 16 * 65536;
    for (uint32_t range_length = 1; range_length < 16384; range_length *= 3) {
        sbs_t *sbs = sbs_create();
        for (int i = 0; i < 50; i++) {
            uint32_t value = our_rand() % span;
            sbs_add_value(sbs, value);
        }
        for (int i = 0; i < 50; i++) {
            uint64_t range_start = our_rand() % (span - range_length);
            sbs_add_range(sbs, range_start, range_start + range_length - 1);
        }
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // max range
    {
        roaring_bitmap_t *r = roaring_bitmap_create();
        roaring_bitmap_add_range(r, 0, UINT32_MAX + UINT64_C(1));
        assert_true(roaring_bitmap_get_cardinality(r) == UINT64_C(0x100000000));
        roaring_bitmap_free(r);
    }

    // bug: segfault
    {
        roaring_bitmap_t *r1 = roaring_bitmap_from_range(0, 1, 1);
        roaring_bitmap_set_copy_on_write(r1, true);
        roaring_bitmap_t *r2 = roaring_bitmap_copy(r1);
        roaring_bitmap_add_range(r1, 0, 1);
        assert_true(roaring_bitmap_get_cardinality(r1) == 1);
        assert_true(roaring_bitmap_get_cardinality(r2) == 1);
        roaring_bitmap_free(r2);
        roaring_bitmap_free(r1);
    }
}

DEFINE_TEST(test_remove_range) {
    // autoconversion: ARRAY -> ARRAY -> NULL
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 100, 200);
        sbs_convert(sbs, ARRAY_CONTAINER_TYPE);
        sbs_remove_range(sbs, 100, 105);
        sbs_remove_range(sbs, 195, 200);
        sbs_remove_range(sbs, 150, 155);
        assert_true(sbs_check_type(sbs, ARRAY_CONTAINER_TYPE));
        sbs_compare(sbs);
        sbs_remove_range(sbs, 102, 198);
        assert_true(sbs_is_empty(sbs));
        sbs_free(sbs);
    }

    // autoconversion: BITSET -> BITSET -> ARRAY
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 0, 40000);
        sbs_convert(sbs, BITSET_CONTAINER_TYPE);
        sbs_remove_range(sbs, 100, 200);
        assert_true(sbs_check_type(sbs, BITSET_CONTAINER_TYPE));
        sbs_remove_range(sbs, 200, 39900);
        assert_true(sbs_check_type(sbs, ARRAY_CONTAINER_TYPE));
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // autoconversion: BITSET -> NULL
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 100, 200);
        sbs_convert(sbs, BITSET_CONTAINER_TYPE);
        sbs_remove_range(sbs, 50, 250);
        assert_true(sbs_is_empty(sbs));
        sbs_free(sbs);
    }

    // autoconversion: RUN -> RUN -> BITSET
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 0, 40000);
        sbs_add_range(sbs, 50000, 60000);
        sbs_convert(sbs, RUN_CONTAINER_TYPE);
        sbs_remove_range(sbs, 100, 200);
        sbs_remove_range(sbs, 40000, 50000);
        assert_true(sbs_check_type(sbs, RUN_CONTAINER_TYPE));
        for (int i = 0; i < 65535; i++) {
            if (i % 2 == 0) {
                sbs_remove_range(sbs, i, i);
            }
        }
        assert_true(sbs_check_type(sbs, BITSET_CONTAINER_TYPE));
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // autoconversion: RUN -> NULL
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 100, 200);
        sbs_add_range(sbs, 300, 400);
        sbs_convert(sbs, RUN_CONTAINER_TYPE);
        sbs_remove_range(sbs, 50, 450);
        assert_true(sbs_is_empty(sbs));
        sbs_free(sbs);
    }

    // remove containers
    {
        sbs_t *sbs = sbs_create();
        sbs_add_value(sbs, 65536 * 1 + 100);
        sbs_add_value(sbs, 65536 * 3 + 100);
        sbs_add_value(sbs, 65536 * 5 + 100);
        sbs_add_value(sbs, 65536 * 7 + 100);
        sbs_remove_range(sbs, 65536 * 3 + 0,
                         65536 * 3 + 65535);  // from the middle
        sbs_compare(sbs);
        sbs_remove_range(sbs, 65536 * 1 + 0,
                         65536 * 1 + 65535);  // from the beginning
        sbs_compare(sbs);
        sbs_remove_range(sbs, 65536 * 7 + 0,
                         65536 * 7 + 65535);  // from the end
        sbs_compare(sbs);
        sbs_remove_range(sbs, 65536 * 5 + 0,
                         65536 * 5 + 65535);  // the last one
        sbs_compare(sbs);
        sbs_remove_range(sbs, 65536 * 9 + 0,
                         65536 * 9 + 65535);  // non-existent
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // random data inside [0..span)
    const uint32_t span = 16 * 65536;
    for (uint32_t range_length = 3; range_length <= 16384; range_length *= 3) {
        sbs_t *sbs = sbs_create();
        for (int i = 0; i < 50; i++) {
            uint64_t range_start = our_rand() % (span - range_length);
            sbs_add_range(sbs, range_start, range_start + range_length - 1);
        }
        for (int i = 0; i < 50; i++) {
            uint64_t range_start = our_rand() % (span - range_length);
            sbs_remove_range(sbs, range_start, range_start + range_length - 1);
        }
        sbs_compare(sbs);
        sbs_free(sbs);
    }
}

DEFINE_TEST(test_remove_many) {
    // multiple values per container (sorted)
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 0, 65536 * 2 - 1);
        uint32_t values[] = {1,         3,         5,         7,
                             65536 + 1, 65536 + 3, 65536 + 5, 65536 + 7};
        sbs_remove_many(sbs, sizeof(values) / sizeof(values[0]), values);
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // multiple values per container (interleaved)
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 0, 65536 * 2 - 1);
        uint32_t values[] = {65536 + 7, 65536 + 5, 7,         5,
                             1,         65536 + 1, 65536 + 3, 3};
        sbs_remove_many(sbs, sizeof(values) / sizeof(values[0]), values);
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // no-op checks
    {
        sbs_t *sbs = sbs_create();
        sbs_add_value(sbs, 500);
        uint32_t values[] = {501, 80000};  // non-existent value/container
        sbs_remove_many(sbs, sizeof(values) / sizeof(values[0]), values);
        sbs_remove_many(sbs, 0, NULL);  // NULL ptr is not dereferenced
        sbs_compare(sbs);
        sbs_free(sbs);
    }

    // container type changes and container removal
    {
        sbs_t *sbs = sbs_create();
        sbs_add_range(sbs, 0, 65535);
        for (uint32_t v = 0; v <= 65535; v++) {
            sbs_remove_many(sbs, 1, &v);
            assert_true(roaring_bitmap_get_cardinality(sbs->roaring) ==
                        65535 - v);
        }
        assert_true(sbs_is_empty(sbs));
        sbs_free(sbs);
    }
}

DEFINE_TEST(test_range_cardinality) {
    const uint64_t s = 65536;

    roaring_bitmap_t *r = roaring_bitmap_create();
    roaring_bitmap_add_range(r, s * 2, s * 10);

    // single container (minhb == maxhb)
    assert_true(roaring_bitmap_range_cardinality(r, s * 2, s * 3) == s);
    assert_true(roaring_bitmap_range_cardinality(r, s * 2 + 100, s * 3) ==
                s - 100);
    assert_true(roaring_bitmap_range_cardinality(r, s * 2, s * 3 - 200) ==
                s - 200);
    assert_true(roaring_bitmap_range_cardinality(r, s * 2 + 100, s * 3 - 200) ==
                s - 300);

    // multiple containers (maxhb > minhb)
    assert_true(roaring_bitmap_range_cardinality(r, s * 2, s * 5) == s * 3);
    assert_true(roaring_bitmap_range_cardinality(r, s * 2 + 100, s * 5) ==
                s * 3 - 100);
    assert_true(roaring_bitmap_range_cardinality(r, s * 2, s * 5 - 200) ==
                s * 3 - 200);
    assert_true(roaring_bitmap_range_cardinality(r, s * 2 + 100, s * 5 - 200) ==
                s * 3 - 300);

    // boundary checks
    assert_true(roaring_bitmap_range_cardinality(r, s * 20, s * 21) == 0);
    assert_true(roaring_bitmap_range_cardinality(r, 100, 100) == 0);
    assert_true(roaring_bitmap_range_cardinality(r, 0, s * 7) == s * 5);
    assert_true(roaring_bitmap_range_cardinality(r, s * 7, UINT64_MAX) ==
                s * 3);

    roaring_bitmap_free(r);
}

void frozen_serialization_compare(roaring_bitmap_t *r1) {
    size_t num_bytes = roaring_bitmap_frozen_size_in_bytes(r1);
    char *buf = (char *)roaring_aligned_malloc(32, num_bytes);
    roaring_bitmap_frozen_serialize(r1, buf);

    const roaring_bitmap_t *r2 = roaring_bitmap_frozen_view(buf, num_bytes);

    assert_true(roaring_bitmap_equals(r1, r2));
    assert_true(roaring_bitmap_frozen_view(buf + 1, num_bytes - 1) == NULL);

    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    roaring_aligned_free(buf);
}

DEFINE_TEST(test_frozen_serialization) {
    const uint64_t s = 65536;

    roaring_bitmap_t *r = roaring_bitmap_create();
    roaring_bitmap_add(r, 0);
    roaring_bitmap_add(r, UINT32_MAX);
    roaring_bitmap_add(r, 1000);
    roaring_bitmap_add(r, 2000);
    roaring_bitmap_add(r, 100000);
    roaring_bitmap_add(r, 200000);
    roaring_bitmap_add_range(r, s * 10 + 100, s * 13 - 100);
    for (uint64_t i = 0; i < s * 3; i += 2) {
        roaring_bitmap_add(r, s * 20 + i);
    }
    roaring_bitmap_run_optimize(r);
    // roaring_bitmap_printf_describe(r);
    frozen_serialization_compare(r);
}

DEFINE_TEST(test_frozen_serialization_max_containers) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    for (int64_t i = 0; i < 65536; i++) {
        roaring_bitmap_add(r, 65536 * i);
    }
    assert_true(r->high_low_container.size == 65536);
    frozen_serialization_compare(r);
}

DEFINE_TEST(test_portable_deserialize_frozen) {
    roaring_bitmap_t *r1 =
        roaring_bitmap_from(1, 2, 3, 100, 1000, 10000, 1000000, 20000000);
    assert_non_null(r1);

    uint32_t serialize_len;
    roaring_bitmap_t *r2;

    for (int i = 0, top_val = 384000; i < top_val; i++)
        roaring_bitmap_add(r1, 3 * i);

    uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    char *serialized = (char *)malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert_int_equal(serialize_len, expectedsize);
    r2 = roaring_bitmap_portable_deserialize_frozen(serialized);
    assert_non_null(r2);

    uint64_t card1 = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr1);

    uint64_t card2 = roaring_bitmap_get_cardinality(r2);
    uint32_t *arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r2, arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    free(serialized);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);

    r1 = roaring_bitmap_from(2946000, 2997491, 10478289, 10490227, 10502444,
                             19866827);
    expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    serialized = (char *)malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert_int_equal(serialize_len, expectedsize);
    assert_int_equal(serialize_len, expectedsize);

    r2 = roaring_bitmap_portable_deserialize_frozen(serialized);
    assert_non_null(r2);

    card1 = roaring_bitmap_get_cardinality(r1);
    arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr1);

    card2 = roaring_bitmap_get_cardinality(r2);
    arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r2, arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    free(serialized);

    r1 = roaring_bitmap_create();
    assert_non_null(r1);

    for (uint32_t k = 100; k < 100000; ++k) {
        roaring_bitmap_add(r1, k);
    }

    roaring_bitmap_run_optimize(r1);
    expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    serialized = (char *)malloc(expectedsize);
    serialize_len = roaring_bitmap_portable_serialize(r1, serialized);
    assert_int_equal(serialize_len, expectedsize);

    r2 = roaring_bitmap_portable_deserialize_frozen(serialized);
    assert_non_null(r2);

    card1 = roaring_bitmap_get_cardinality(r1);
    arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r1, arr1);

    card2 = roaring_bitmap_get_cardinality(r2);
    arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(r2, arr2);

    assert_true(array_equals(arr1, card1, arr2, card2));
    assert_true(roaring_bitmap_equals(r1, r2));
    free(arr1);
    free(arr2);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    free(serialized);
}

DEFINE_TEST(convert_to_bitset) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    for (uint32_t i = 100; i < 100000; i += 1 + (i % 5)) {
        roaring_bitmap_add(r1, i);
    }
    for (uint32_t i = 100000; i < 500000; i += 100) {
        roaring_bitmap_add(r1, i);
    }
    roaring_bitmap_add_range(r1, 500000, 600000);
    bitset_t *bitset = bitset_create();
    bool success = roaring_bitmap_to_bitset(r1, bitset);
    assert_true(success);  // could fail due to memory allocation.
    assert_true(bitset_count(bitset) == roaring_bitmap_get_cardinality(r1));
    // You can then query the bitset:
    for (uint32_t i = 100; i < 100000; i += 1 + (i % 5)) {
        assert_true(bitset_get(bitset, i));
    }
    for (uint32_t i = 100000; i < 500000; i += 100) {
        assert_true(bitset_get(bitset, i));
    }
    // you must free the memory:
    bitset_free(bitset);
    roaring_bitmap_free(r1);
}

// simple execution test
DEFINE_TEST(simple_roaring_bitmap_or_many) {
    roaring_bitmap_t *roaring_bitmaps[2];
    roaring_bitmaps[0] = roaring_bitmap_create();
    roaring_bitmaps[1] = roaring_bitmap_create();
    for (uint32_t i = 100; i < 1000; i++)
        roaring_bitmap_add(roaring_bitmaps[0], i);
    for (uint32_t i = 1000; i < 2000; i++)
        roaring_bitmap_add(roaring_bitmaps[1], i);
    roaring_bitmap_t *bigunion =
        roaring_bitmap_or_many(2, (const roaring_bitmap_t **)roaring_bitmaps);
    roaring_bitmap_free(roaring_bitmaps[0]);
    roaring_bitmap_free(roaring_bitmaps[1]);
    roaring_bitmap_free(bigunion);
}

bool deserialization_test(const char *data, size_t size) {
    // We test that deserialization never fails.
    roaring_bitmap_t *bitmap =
        roaring_bitmap_portable_deserialize_safe(data, size);
    if (bitmap) {
        // The bitmap may not be usable if it does not follow the specification.
        // We can validate the bitmap we recovered to make sure it is proper.
        const char *reason_failure = NULL;
        if (roaring_bitmap_internal_validate(bitmap, &reason_failure)) {
            // the bitmap is ok!
            uint32_t cardinality = roaring_bitmap_get_cardinality(bitmap);

            for (uint32_t i = 100; i < 1000; i++) {
                if (!roaring_bitmap_contains(bitmap, i)) {
                    cardinality++;
                    roaring_bitmap_add(bitmap, i);
                }
            }

            uint32_t new_cardinality = roaring_bitmap_get_cardinality(bitmap);
            if (cardinality != new_cardinality) {
                return false;
            }
        }
        roaring_bitmap_free(bitmap);
    }
    return true;
}

DEFINE_TEST(robust_deserialization) {
    assert_true(deserialization_test(NULL, 0));
    // contains a run container that overflows the 16-bit boundary.
    const char test1[] =
        "\x3b\x30\x00\x00\x01\x00\x00\xfa\x2e\x01\x00\x00\x02\xff\xff";
    assert_true(deserialization_test(test1, sizeof(test1)));
}

DEFINE_TEST(issue538) {
    roaring_bitmap_t *dense = roaring_bitmap_create();
    int *values = (int *)malloc(4500 * sizeof(int));

    // Make a bitmap with enough entries to need a bitset container
    for (int k = 0; k < 4500; ++k) {
        roaring_bitmap_add(dense, 2 * k);
        values[k] = 2 * k;
    }

    // Shift it to partly overlap with the next container.
    roaring_bitmap_t *dense_shift = roaring_bitmap_add_offset(dense, 64000);

    // Serialise and deserialise
    int buffer_size = roaring_bitmap_portable_size_in_bytes(dense_shift);

    char *arr = (char *)malloc(buffer_size * sizeof(char));

    roaring_bitmap_portable_serialize(dense_shift, arr);

    roaring_bitmap_t *deserialized = roaring_bitmap_portable_deserialize(arr);

    // Iterate through the deserialised bitmap - This should be the same set as
    // before just shifted...
    roaring_uint32_iterator_t *iterator = roaring_iterator_create(deserialized);
    int i = 0;
    while (iterator->has_value) {
        assert_true((int)iterator->current_value == values[i++] + 64000);
        roaring_uint32_iterator_advance(iterator);
    }

    roaring_uint32_iterator_free(iterator);

    assert_true(roaring_bitmap_get_cardinality(dense_shift) ==
                roaring_bitmap_get_cardinality(deserialized));
    free(arr);
    free(values);
    roaring_bitmap_free(dense);
    roaring_bitmap_free(dense_shift);
    roaring_bitmap_free(deserialized);
}

DEFINE_TEST(issue538b) {
    int shift = -65536;
    roaring_bitmap_t *toshift = roaring_bitmap_from_range(131074, 131876, 1);
    roaring_bitmap_set_copy_on_write(toshift, 1);
    roaring_bitmap_t *toshift_copy = roaring_bitmap_copy(toshift);

    roaring_bitmap_t *shifted = roaring_bitmap_add_offset(toshift, shift);
    roaring_bitmap_equals(toshift, toshift_copy);

    roaring_bitmap_t *expected =
        roaring_bitmap_from_range(131074 + shift, 131876 + shift, 1);
    roaring_bitmap_set_copy_on_write(expected, 1);
    assert_true(roaring_bitmap_get_cardinality(toshift) ==
                roaring_bitmap_get_cardinality(expected));
    assert_true(roaring_bitmap_equals(shifted, expected));
    roaring_bitmap_free(toshift_copy);
    roaring_bitmap_free(toshift);
    roaring_bitmap_free(shifted);
    roaring_bitmap_free(expected);
}

DEFINE_TEST(issue_15jan2024) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_add(r1, 1);
    // Serialized bitmap data
    char diff_bitmap[] = {0x3b, 0x30, 0x00, 0x00, 0x01, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00};
    roaring_bitmap_t *r2 = roaring_bitmap_portable_deserialize_safe(
        diff_bitmap, sizeof(diff_bitmap));
    assert_true(r2 != NULL);
    const char *reason = NULL;
    assert_false(roaring_bitmap_internal_validate(r2, &reason));
    printf("reason = %s\n", reason);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
}

int main() {
    tellmeall();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(issue538b),
        cmocka_unit_test(issue538),
        cmocka_unit_test(simple_roaring_bitmap_or_many),
        cmocka_unit_test(robust_deserialization),
        cmocka_unit_test(issue457),
        cmocka_unit_test(convert_to_bitset),
        cmocka_unit_test(issue440),
        cmocka_unit_test(issue436),
        cmocka_unit_test(issue433),
        cmocka_unit_test(issue429),
        cmocka_unit_test(issue431),
        cmocka_unit_test(test_contains_range_PyRoaringBitMap_issue81),
        cmocka_unit_test(issue316),
        cmocka_unit_test(issue288),
#if !CROARING_IS_BIG_ENDIAN
        cmocka_unit_test(issue245),
#endif
        cmocka_unit_test(issue208),
        cmocka_unit_test(issue208b),
        cmocka_unit_test(range_contains),
        cmocka_unit_test(contains_bulk),
        cmocka_unit_test(inplaceorwide),
        cmocka_unit_test(test_contains_range),
        cmocka_unit_test(check_range_contains_from_end),
        cmocka_unit_test(check_iterate_to_end),
        cmocka_unit_test(check_iterate_to_beginning),
        cmocka_unit_test(test_iterator_reuse),
        cmocka_unit_test(check_full_flip),
        cmocka_unit_test(test_adversarial_range),
        cmocka_unit_test(check_full_inplace_flip),
        cmocka_unit_test(test_stress_memory_true),
        cmocka_unit_test(test_stress_memory_false),
        cmocka_unit_test(check_interval),
        cmocka_unit_test(test_uint32_iterator_true),
#if !CROARING_IS_BIG_ENDIAN
        cmocka_unit_test(test_example_true),
        cmocka_unit_test(test_example_false),
#endif
        cmocka_unit_test(test_clear),
        cmocka_unit_test(can_copy_empty_true),
        cmocka_unit_test(can_copy_empty_false),
        cmocka_unit_test(test_intersect_small_run_bitset),
        cmocka_unit_test(is_really_empty),
        cmocka_unit_test(test_rank),
        cmocka_unit_test(test_get_index),
        cmocka_unit_test(test_maximum_minimum),
        cmocka_unit_test(test_stats),
        cmocka_unit_test(test_addremove),
        cmocka_unit_test(test_addremove_bulk),
        cmocka_unit_test(test_addremoverun),
        cmocka_unit_test(test_basic_add),
        cmocka_unit_test(test_remove_withrun),
        cmocka_unit_test(test_remove_from_copies_true),
        cmocka_unit_test(test_remove_from_copies_false),
        cmocka_unit_test(test_range_and_serialize),
        cmocka_unit_test(test_silly_range),
        cmocka_unit_test(test_uint32_iterator_true),
        cmocka_unit_test(test_uint32_iterator_false),
        cmocka_unit_test(with_huge_capacity),
        cmocka_unit_test(leaks_with_empty_true),
        cmocka_unit_test(leaks_with_empty_false),
        cmocka_unit_test(test_bitmap_from_range),
        cmocka_unit_test(test_printf),
        cmocka_unit_test(test_printf_withbitmap),
        cmocka_unit_test(test_printf_withrun),
        cmocka_unit_test(test_iterate),
        cmocka_unit_test(test_iterate_empty),
        cmocka_unit_test(test_iterate_withbitmap),
        cmocka_unit_test(test_iterate_withrun),
#if !CROARING_IS_BIG_ENDIAN
        cmocka_unit_test(test_serialize),
        cmocka_unit_test(test_portable_serialize),
#endif
        cmocka_unit_test(test_add),
        cmocka_unit_test(test_add_checked),
        cmocka_unit_test(test_remove_checked),
        cmocka_unit_test(test_contains),
        cmocka_unit_test(test_intersection_array_x_array),
        cmocka_unit_test(test_intersection_array_x_array_inplace),
        cmocka_unit_test(test_intersection_bitset_x_bitset),
        cmocka_unit_test(test_intersection_bitset_x_bitset_inplace),
        cmocka_unit_test(test_union_true),
        cmocka_unit_test(test_union_false),
        cmocka_unit_test(test_xor_false),
        cmocka_unit_test(test_xor_inplace_false),
        cmocka_unit_test(test_xor_lazy_false),
        cmocka_unit_test(test_xor_lazy_inplace_false),
        cmocka_unit_test(test_xor_true),
        cmocka_unit_test(test_xor_inplace_true),
        cmocka_unit_test(test_xor_lazy_true),
        cmocka_unit_test(test_xor_lazy_inplace_true),
        cmocka_unit_test(test_andnot_false),
        cmocka_unit_test(test_andnot_inplace_false),
        cmocka_unit_test(test_andnot_true),
        cmocka_unit_test(test_andnot_inplace_true),
        cmocka_unit_test(test_conversion_to_int_array),
        cmocka_unit_test(test_array_to_run),
        cmocka_unit_test(test_array_to_self),
        cmocka_unit_test(test_bitset_to_self),
        cmocka_unit_test(test_bitset_to_run),
        cmocka_unit_test(test_conversion_to_int_array_with_runoptimize),
        cmocka_unit_test(test_run_to_self),
        cmocka_unit_test(test_remove_run_to_bitset_cow),
        cmocka_unit_test(test_remove_run_to_array_cow),
        cmocka_unit_test(test_remove_run_to_bitset),
        cmocka_unit_test(test_remove_run_to_array),
        cmocka_unit_test(test_negation_array0),
        cmocka_unit_test(test_negation_array1),
        cmocka_unit_test(test_negation_array2),
        cmocka_unit_test(test_negation_bitset1),
        cmocka_unit_test(test_negation_bitset2),
        cmocka_unit_test(test_negation_run1),
        cmocka_unit_test(test_negation_run2),
        cmocka_unit_test(test_rand_flips),
        cmocka_unit_test(test_inplace_negation_array0),
        cmocka_unit_test(test_inplace_negation_array1),
        cmocka_unit_test(test_inplace_negation_array2),
        cmocka_unit_test(test_inplace_negation_bitset1),
        cmocka_unit_test(test_inplace_negation_bitset2),
        cmocka_unit_test(test_inplace_negation_run1),
        cmocka_unit_test(test_inplace_negation_run2),
        cmocka_unit_test(test_inplace_rand_flips),
        cmocka_unit_test(test_flip_array_container_removal),
        cmocka_unit_test(test_flip_bitset_container_removal),
        cmocka_unit_test(test_flip_run_container_removal),
        cmocka_unit_test(test_flip_run_container_removal2),
        cmocka_unit_test(select_test),
        cmocka_unit_test(test_subset),
        cmocka_unit_test(test_or_many_memory_leak),
        // cmocka_unit_test(test_run_to_bitset),
        // cmocka_unit_test(test_run_to_array),
        cmocka_unit_test(test_read_uint32_iterator_array),
        cmocka_unit_test(test_read_uint32_iterator_bitset),
        cmocka_unit_test(test_read_uint32_iterator_run),
        cmocka_unit_test(test_read_uint32_iterator_native),
        cmocka_unit_test(test_previous_iterator_array),
        cmocka_unit_test(test_previous_iterator_bitset),
        cmocka_unit_test(test_previous_iterator_run),
        cmocka_unit_test(test_previous_iterator_native),
        cmocka_unit_test(test_iterator_reuse),
        cmocka_unit_test(test_iterator_reuse_many),
        cmocka_unit_test(read_uint32_iterator_zero_count),
        cmocka_unit_test(test_add_range),
        cmocka_unit_test(test_remove_range),
        cmocka_unit_test(test_remove_many),
        cmocka_unit_test(test_range_cardinality),
#if !CROARING_IS_BIG_ENDIAN
        cmocka_unit_test(test_frozen_serialization),
        cmocka_unit_test(test_frozen_serialization_max_containers),
        cmocka_unit_test(test_portable_deserialize_frozen),
        cmocka_unit_test(issue_15jan2024),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
