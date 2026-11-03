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

DEFINE_TEST(issue660) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_add(r1, 0);
    bitset_t *b = bitset_create();
    bool success =
        roaring_bitmap_to_bitset(r1, b);  // Segfault happens on this line
    assert_true(success);
    bitset_free(b);
    roaring_bitmap_free(r1);
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
    roaring_bulk_context_t context = {0, 0, 0, 0};

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
        roaring_bulk_context_t empty_context = {0, 0, 0, 0};
        bool expected_contains = roaring_bitmap_contains(bm, values[i]);
        assert_true(expected_contains == roaring_bitmap_contains_bulk(
                                             bm, &empty_context, values[i]));
        assert_true(expected_contains ==
                    roaring_bitmap_contains_bulk(bm, &context, values[i]));

        if (expected_contains) {
            assert_int_equal(context.key, values[i] >> 16);
        }
        if (context.container != NULL) {
            assert_uint_in_range(context.idx, 0,
                                 bm->high_low_container.size - 1);
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

// https://github.com/Ezibenroc/PyRoaringBitMap/issues/124
DEFINE_TEST(PyRoaringBitMap124) {
    // adversarial test case
    const char data[] = {
        0x3a, 0x30, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
        0x10, 0x00, 0x00, 0x00, 0x32, 0x00, 0x33, 0x00, 0x34, 0x00, 0x35, 0x00,
        0x36, 0x00, 0x37, 0x00, 0x38, 0x00, 0x3a, 0x00, 0x3b, 0x00, 0x3c, 0x00,
    };
    size_t length = sizeof(data);
    roaring_bitmap_t *deserialized_bitmap =
        roaring_bitmap_portable_deserialize_safe(data, length);
    assert_true(deserialized_bitmap != NULL);
    roaring_bitmap_printf_describe(deserialized_bitmap);
    const char *reason_failure = NULL;
    assert_true(
        roaring_bitmap_internal_validate(deserialized_bitmap, &reason_failure));
    roaring_bitmap_free(deserialized_bitmap);

#if !CROARING_IS_BIG_ENDIAN
    // The frozen format is intentionally non-portable (native byte order).
    // On big-endian hosts the input here (little-endian) won't pass the
    // frozen view validation, but the result is platform-dependent so we
    // only assert the little-endian behavior.
    const roaring_bitmap_t *r2 = roaring_bitmap_frozen_view(data, length);
    assert_true(r2 == NULL);
#endif
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

DEFINE_TEST(issue743) {
    roaring_bitmap_t *A = roaring_bitmap_from(1, 2, 3, 4);
    roaring_bitmap_t *B =
        roaring_bitmap_from(0x38000000, 0x38000001, 0x38000002, 0x38000003);
    roaring_bitmap_set_copy_on_write(A, true);
    roaring_bitmap_set_copy_on_write(B, true);
    roaring_bitmap_t *C = roaring_bitmap_andnot(B, A);
    roaring_bitmap_set_copy_on_write(C, false);
    roaring_bitmap_free(A);
    roaring_bitmap_free(B);
    roaring_bitmap_t *D = roaring_bitmap_from(1, 2, 3, 4, 5, 6);
    roaring_bitmap_t *E = roaring_bitmap_lazy_xor(D, C);
    roaring_bitmap_repair_after_lazy(E);
    roaring_bitmap_t *expectedE = roaring_bitmap_from(
        1, 2, 3, 4, 5, 6, 939524096, 939524097, 939524098, 939524099);
    assert_true(roaring_bitmap_equals(E, expectedE));
    roaring_bitmap_free(C);
    roaring_bitmap_free(D);
    roaring_bitmap_free(E);
    roaring_bitmap_free(expectedE);
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

// Overwriting a copy-on-write bitmap with one that is not must release its
// shared containers and leave the bitmap it shares them with intact.
DEFINE_TEST(overwrite_copy_on_write_with_plain) {
    roaring_bitmap_t *r = roaring_bitmap_from_range(0, 3 * 65536, 3);
    roaring_bitmap_set_copy_on_write(r, true);
    roaring_bitmap_t *dest = roaring_bitmap_copy(r);  // shares every container
    roaring_bitmap_t *src = roaring_bitmap_from(1, 2, 3);

    assert_true(roaring_bitmap_overwrite(dest, src));
    assert_false(roaring_bitmap_get_copy_on_write(dest));
    assert_true(roaring_bitmap_equals(dest, src));
    assert_true(roaring_bitmap_internal_validate(dest, NULL));
    assert_true(roaring_bitmap_get_cardinality(r) == 65536);
    assert_true(roaring_bitmap_internal_validate(r, NULL));
    roaring_bitmap_free(r);
    roaring_bitmap_free(dest);
    roaring_bitmap_free(src);
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
    for (size_t i = 0; i < card1 - offset; ++i) {
        assert_int_equal(arr3[i], arr1[i + offset]);
    }
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
    roaring_bulk_context_t context = {0, 0, 0, 0};
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

DEFINE_TEST(test_andnot_inplace_false) { test_andnot_inplace(false); }

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

// Flipping a sparse array container over a large range gives few runs.
DEFINE_TEST(test_flip_sparse_array_container) {
    const uint32_t cards[] = {1, 2, 100, 2045, 2046, 2047, 3000, 4096};
    const uint32_t ranges[][2] = {
        {0, 65536}, {0, 1 << 20}, {100, 60000}, {7, 5000}, {65530, 131072}};
    for (size_t c = 0; c < sizeof(cards) / sizeof(cards[0]); c++) {
        for (size_t g = 0; g < sizeof(ranges) / sizeof(ranges[0]); g++) {
            roaring_bitmap_t *bm = roaring_bitmap_create();
            // Spread the values over the first chunk, with some neighbors.
            for (uint32_t k = 0; k < cards[c]; k++) {
                roaring_bitmap_add(bm, (k * 15u) % 65536 + (k & 1));
            }
            uint32_t start = ranges[g][0], end = ranges[g][1];
            roaring_bitmap_t *flipped = roaring_bitmap_flip(bm, start, end);
            roaring_bitmap_t *inplace = roaring_bitmap_copy(bm);
            roaring_bitmap_flip_inplace(inplace, start, end);
            assert_true(roaring_bitmap_internal_validate(flipped, NULL));
            assert_true(roaring_bitmap_internal_validate(inplace, NULL));
            assert_true(roaring_bitmap_equals(flipped, inplace));
            uint64_t expected =
                roaring_bitmap_get_cardinality(bm) + end - start -
                2 * roaring_bitmap_range_cardinality(bm, start, end);
            assert_int_equal(roaring_bitmap_get_cardinality(flipped), expected);
            for (uint32_t v = 0; v < 70000; v++) {
                bool in_range = v >= start && v < end;
                assert_true(roaring_bitmap_contains(flipped, v) ==
                            (roaring_bitmap_contains(bm, v) != in_range));
            }
            if (cards[c] <= 2046 && start == 0 && end % 65536 == 0) {
                // The flipped chunks are all runs, whole or partial flips.
                roaring_statistics_t stats;
                roaring_bitmap_statistics(flipped, &stats);
                assert_int_equal(stats.n_run_containers, end / 65536);
                roaring_bitmap_statistics(inplace, &stats);
                assert_int_equal(stats.n_run_containers, end / 65536);
            }
            roaring_bitmap_free(bm);
            roaring_bitmap_free(flipped);
            roaring_bitmap_free(inplace);
        }
    }
    // Clustered values around the ends of the range, so that runs touch.
    uint32_t seed = 4321;
    for (int trial = 0; trial < 300; trial++) {
        seed = seed * 1664525 + 1013904223;
        uint32_t start = (seed >> 8) % 3000;
        seed = seed * 1664525 + 1013904223;
        uint32_t end = 65536 - (seed >> 8) % 3000;
        if (trial % 3 == 0) end = 65536;
        if (trial % 5 == 0) start = 0;
        roaring_bitmap_t *bm = roaring_bitmap_create();
        const uint32_t centers[] = {0, start, end, 30000, 65535};
        for (size_t c = 0; c < 5; c++) {
            for (int k = 0; k < 40; k++) {
                seed = seed * 1664525 + 1013904223;
                int64_t v =
                    (int64_t)centers[c] + (int64_t)((seed >> 8) % 9) - 4;
                if (v >= 0 && v < 65536) roaring_bitmap_add(bm, (uint32_t)v);
            }
        }
        roaring_bitmap_t *flipped = roaring_bitmap_flip(bm, start, end);
        roaring_bitmap_t *inplace = roaring_bitmap_copy(bm);
        roaring_bitmap_flip_inplace(inplace, start, end);
        assert_true(roaring_bitmap_internal_validate(flipped, NULL));
        assert_true(roaring_bitmap_equals(flipped, inplace));
        for (uint32_t v = 0; v < 65536; v++) {
            bool in_range = v >= start && v < end;
            assert_true(roaring_bitmap_contains(flipped, v) ==
                        (roaring_bitmap_contains(bm, v) != in_range));
        }
        roaring_bitmap_free(bm);
        roaring_bitmap_free(flipped);
        roaring_bitmap_free(inplace);
    }
}

// Checks roaring_bitmap_word_range_to_bitset against roaring_bitmap_contains.
static void check_word_range_to_bitset(const roaring_bitmap_t *bm,
                                       uint32_t first_word, uint32_t n_words) {
    uint64_t *words = (uint64_t *)malloc((n_words + 2) * sizeof(uint64_t));
    assert_non_null(words);
    // Guard words around the output must not be touched.
    words[0] = words[n_words + 1] = UINT64_C(0x0123456789abcdef);
    for (uint32_t k = 1; k <= n_words; k++) words[k] = ~UINT64_C(0);
    roaring_bitmap_word_range_to_bitset(bm, first_word, n_words, words + 1);
    assert_true(words[0] == UINT64_C(0x0123456789abcdef));
    assert_true(words[n_words + 1] == UINT64_C(0x0123456789abcdef));
    for (uint32_t k = 0; k < n_words; k++) {
        uint64_t expected = 0;
        for (uint32_t i = 0; i < 64; i++) {
            uint32_t v = (uint32_t)(((uint64_t)first_word + k) * 64 + i);
            if (roaring_bitmap_contains(bm, v)) expected |= UINT64_C(1) << i;
        }
        assert_true(words[k + 1] == expected);
    }
    free(words);
}

DEFINE_TEST(test_word_range_to_bitset) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    uint32_t seed = 1234;
    // Chunk 0: array, chunk 1: bitset, chunk 2: runs, chunk 4: array, and
    // the last chunk of the 32-bit range: runs and values at the very end.
    for (uint32_t k = 0; k < 1000; k++) {
        seed = seed * 1664525 + 1013904223;
        roaring_bitmap_add(bm, (seed >> 16) % 65536);
    }
    for (uint32_t k = 0; k < 30000; k++) {
        seed = seed * 1664525 + 1013904223;
        roaring_bitmap_add(bm, 65536 + (seed >> 16) % 65536);
    }
    for (uint32_t k = 0; k < 300; k++) {
        seed = seed * 1664525 + 1013904223;
        uint32_t s = 2 * 65536 + (seed >> 16) % 65000;
        roaring_bitmap_add_range(bm, s, s + 1 + (seed >> 8) % 300);
    }
    roaring_bitmap_add(bm, 4 * 65536 + 63);
    roaring_bitmap_add(bm, 4 * 65536 + 64);
    roaring_bitmap_add_range(bm, UINT64_C(0xFFFF0000) + 100,
                             UINT64_C(0xFFFF0000) + 40000);
    roaring_bitmap_add_range(bm, UINT64_C(0xFFFFFFF0), UINT64_C(0x100000000));
    roaring_bitmap_run_optimize(bm);
    roaring_statistics_t stats;
    roaring_bitmap_statistics(bm, &stats);
    assert_true(stats.n_array_containers >= 2);
    assert_true(stats.n_bitset_containers >= 1);
    assert_true(stats.n_run_containers >= 2);

    const uint32_t last = (UINT32_C(1) << 26);
    const uint32_t windows[][2] = {{0, 0},
                                   {0, 1},
                                   {0, 1024},
                                   {0, 5000},
                                   {1, 1},
                                   {3, 2},
                                   {1023, 2},
                                   {1000, 100},
                                   {1024, 1024},
                                   {1500, 1},
                                   {2047, 3},
                                   {2048, 1024},
                                   {2100, 7},
                                   {2900, 300},
                                   {4096, 1},
                                   {4096, 2},
                                   {3000, 2000},
                                   {10000, 10},
                                   {last - 1, 1},
                                   {last - 1024, 1024},
                                   {last - 700, 700},
                                   {last - 1100, 200},
                                   {last - 3000, 3000}};
    for (size_t w = 0; w < sizeof(windows) / sizeof(windows[0]); w++) {
        check_word_range_to_bitset(bm, windows[w][0], windows[w][1]);
    }
    for (int trial = 0; trial < 200; trial++) {
        seed = seed * 1664525 + 1013904223;
        uint32_t first = (seed >> 8) % 5200;
        seed = seed * 1664525 + 1013904223;
        check_word_range_to_bitset(bm, first, (seed >> 8) % 1500);
    }
    roaring_bitmap_t *empty = roaring_bitmap_create();
    check_word_range_to_bitset(empty, 100, 50);
    roaring_bitmap_free(empty);
    roaring_bitmap_free(bm);
}

// randomized test for rank query
DEFINE_TEST(select_test) {
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

static uint32_t *intersect_test_snapshot(const roaring_bitmap_t *bitmap,
                                         uint64_t *cardinality) {
    *cardinality = roaring_bitmap_get_cardinality(bitmap);
    size_t count = (size_t)*cardinality;
    uint32_t *values =
        (uint32_t *)malloc((count == 0 ? 1 : count) * sizeof(uint32_t));
    assert_non_null(values);
    if (count != 0) roaring_bitmap_to_uint32_array(bitmap, values);
    return values;
}

static void intersect_test_assert_unchanged(const roaring_bitmap_t *bitmap,
                                            const uint32_t *before,
                                            uint64_t cardinality) {
    assert_int_equal(roaring_bitmap_get_cardinality(bitmap), cardinality);
    size_t count = (size_t)cardinality;
    uint32_t *after =
        (uint32_t *)malloc((count == 0 ? 1 : count) * sizeof(uint32_t));
    assert_non_null(after);
    if (count != 0) {
        roaring_bitmap_to_uint32_array(bitmap, after);
        assert_true(memcmp(after, before, count * sizeof(uint32_t)) == 0);
    }
    free(after);
}

static void intersect_test_pair(const uint32_t *values1, size_t size1,
                                const uint32_t *values2, size_t size2) {
    roaring_bitmap_t *bitmap1 = size1 == 0
                                    ? roaring_bitmap_create()
                                    : roaring_bitmap_of_ptr(size1, values1);
    roaring_bitmap_t *bitmap2 = size2 == 0
                                    ? roaring_bitmap_create()
                                    : roaring_bitmap_of_ptr(size2, values2);
    assert_non_null(bitmap1);
    assert_non_null(bitmap2);
    roaring_bitmap_shrink_to_fit(bitmap1);
    roaring_bitmap_shrink_to_fit(bitmap2);

    uint64_t cardinality1, cardinality2;
    uint32_t *before1 = intersect_test_snapshot(bitmap1, &cardinality1);
    uint32_t *before2 = intersect_test_snapshot(bitmap2, &cardinality2);
    bool expected = roaring_bitmap_and_cardinality(bitmap1, bitmap2) > 0;
    assert_true(roaring_bitmap_intersect(bitmap1, bitmap2) == expected);
    assert_true(roaring_bitmap_intersect(bitmap2, bitmap1) == expected);
    intersect_test_assert_unchanged(bitmap1, before1, cardinality1);
    intersect_test_assert_unchanged(bitmap2, before2, cardinality2);

    free(before1);
    free(before2);
    roaring_bitmap_free(bitmap1);
    roaring_bitmap_free(bitmap2);
}

DEFINE_TEST(test_intersect_array_container_pairs) {
    const size_t sizes[] = {0,  1,  7,  8,  9,  15,  16,  17,
                            31, 32, 33, 64, 65, 128, 4096};
    uint32_t left[4096], right[4096];
    for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); ++k) {
        const size_t n = sizes[k];
        for (size_t i = 0; i < n; ++i) {
            left[i] = (uint32_t)(2 * i);
            right[i] = (uint32_t)(2 * i + 1);
        }
        intersect_test_pair(left, n, right, n);
        if (n != 0) {
            right[0] = 0;
            for (size_t i = 1; i < n; ++i) right[i] = (uint32_t)(2 * i - 1);
            intersect_test_pair(left, n, right, n);

            if (n >= 2) {
                for (size_t i = 0; i < n; ++i) {
                    left[i] = (uint32_t)(2 * i + 2);
                    right[i] = (uint32_t)(2 * i + 1);
                }
                right[1] = 2;
                intersect_test_pair(left, n, right, n);
            }

            for (size_t i = 0; i + 1 < n; ++i) right[i] = (uint32_t)(2 * i + 1);
            right[n - 1] = (uint32_t)(2 * (n - 1));
            intersect_test_pair(left, n, right, n);
        }
    }

    for (size_t i = 0; i < 8; ++i) left[i] = (uint32_t)(2 * i);
    intersect_test_pair(NULL, 0, left, 8);
    intersect_test_pair(left, 8, NULL, 0);

    const size_t tail_size = 4095;
    for (size_t i = 0; i + 1 < tail_size; ++i) {
        left[i] = (uint32_t)(2 * i);
        right[i] = (uint32_t)(2 * i + 1);
    }
    left[tail_size - 1] = 65535;
    right[tail_size - 1] = 65535;
    intersect_test_pair(left, tail_size, right, tail_size);

    const size_t ratios[][2] = {{8, 512},   {8, 513},   {16, 1024},
                                {16, 1025}, {64, 4096}, {65, 4096}};
    for (size_t k = 0; k < sizeof(ratios) / sizeof(ratios[0]); ++k) {
        const size_t small = ratios[k][0];
        const size_t large = ratios[k][1];
        for (size_t i = 0; i < small; ++i) left[i] = (uint32_t)(2 * i);
        for (size_t i = 0; i < large; ++i) right[i] = (uint32_t)(2 * i + 1);
        intersect_test_pair(left, small, right, large);
    }

    // The first blocks are disjoint, but the next block on the left overlaps
    // the current block on the right. Advance the side with the smaller max.
    for (size_t i = 0; i < 16; ++i) {
        left[i] = (uint32_t)i;
        right[i] = (uint32_t)(i + 8);
    }
    intersect_test_pair(left, 16, right, 16);
}

DEFINE_TEST(test_intersect_shared_cow_arrays) {
    roaring_bitmap_t *bitmap1 = roaring_bitmap_create();
    assert_non_null(bitmap1);
    for (uint32_t i = 0; i < 64; ++i) roaring_bitmap_add(bitmap1, 2 * i);
    for (uint32_t i = 0; i < 16; ++i)
        roaring_bitmap_add(bitmap1, 65536 + 2 * i);
    roaring_bitmap_shrink_to_fit(bitmap1);
    roaring_bitmap_set_copy_on_write(bitmap1, true);
    roaring_bitmap_t *bitmap2 = roaring_bitmap_copy(bitmap1);
    assert_non_null(bitmap2);

    // Mutating key zero separates that array; the second container remains
    // shared.
    for (uint32_t i = 0; i < 64; ++i) {
        assert_true(roaring_bitmap_contains(bitmap2, 2 * i));
        roaring_bitmap_remove(bitmap2, 2 * i);
        assert_false(roaring_bitmap_contains(bitmap2, 2 * i));
    }
    for (uint32_t i = 0; i < 64; ++i) {
        roaring_bitmap_add(bitmap2, 2 * i + 1);
        assert_true(roaring_bitmap_contains(bitmap2, 2 * i + 1));
    }
    int32_t shared_index1 = ra_get_index(&bitmap1->high_low_container, 1);
    int32_t shared_index2 = ra_get_index(&bitmap2->high_low_container, 1);
    assert_true(shared_index1 >= 0);
    assert_true(shared_index2 >= 0);
    uint8_t type1, type2;
    ra_get_container_at_index(&bitmap1->high_low_container,
                              (uint16_t)shared_index1, &type1);
    ra_get_container_at_index(&bitmap2->high_low_container,
                              (uint16_t)shared_index2, &type2);
    assert_int_equal(type1, SHARED_CONTAINER_TYPE);
    assert_int_equal(type2, SHARED_CONTAINER_TYPE);

    uint64_t cardinality1, cardinality2;
    uint32_t *before1 = intersect_test_snapshot(bitmap1, &cardinality1);
    uint32_t *before2 = intersect_test_snapshot(bitmap2, &cardinality2);
    assert_int_equal(roaring_bitmap_and_cardinality(bitmap1, bitmap2), 16);
    assert_true(roaring_bitmap_intersect(bitmap1, bitmap2));
    assert_true(roaring_bitmap_intersect(bitmap2, bitmap1));
    intersect_test_assert_unchanged(bitmap1, before1, cardinality1);
    intersect_test_assert_unchanged(bitmap2, before2, cardinality2);

    free(before1);
    free(before2);
    roaring_bitmap_free(bitmap1);
    roaring_bitmap_free(bitmap2);
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

// In-place union of a bitmap with few containers into one with many.
DEFINE_TEST(test_or_inplace_few_containers) {
    uint32_t seed = 1234;
    for (int trial = 0; trial < 200; trial++) {
        // Mixing bitmaps with and without copy-on-write is unsupported.
        bool cow1 = trial & 1, cow2 = cow1;
        roaring_bitmap_t *big = roaring_bitmap_create();
        roaring_bitmap_set_copy_on_write(big, cow1);
        // Keys 2k, with array, bitset, run and full containers.
        for (uint32_t k = 0; k < 1000; k++) {
            uint32_t base = (2 * k) << 16;
            switch (k % 4) {
                case 0:
                    roaring_bitmap_add(big, base + k);
                    break;
                case 1:
                    for (uint32_t v = 0; v < 65536; v += 3)
                        roaring_bitmap_add(big, base + v);
                    break;
                case 2:
                    roaring_bitmap_add_range(big, base + 100, base + 5000);
                    break;
                default:
                    roaring_bitmap_add_range(big, base, base + 65536);
            }
        }
        roaring_bitmap_run_optimize(big);
        roaring_bitmap_t *small = roaring_bitmap_create();
        roaring_bitmap_set_copy_on_write(small, cow2);
        int nkeys = 1 + trial % 20;
        for (int j = 0; j < nkeys; j++) {
            seed = seed * 1103515245 + 12345;
            // Existing keys, keys between them, before them and after them.
            uint32_t key = (seed >> 8) % 2004;
            if (j == 0 && trial % 3 == 0) key = 2003;
            uint32_t base = key << 16;
            if (seed & 1) {
                roaring_bitmap_add_range(small, base + (seed >> 20),
                                         base + 65536);
            } else {
                roaring_bitmap_add(small, base + (seed >> 16));
            }
        }
        roaring_bitmap_t *shared = NULL;
        if (cow1) shared = roaring_bitmap_copy(big);  // Shares containers.
        roaring_bitmap_t *expected = roaring_bitmap_or(big, small);
        roaring_bitmap_t *small_copy = roaring_bitmap_copy(small);
        roaring_bitmap_or_inplace(big, small);
        const char *reason = NULL;
        assert_true(roaring_bitmap_internal_validate(big, &reason));
        assert_true(roaring_bitmap_equals(big, expected));
        assert_true(roaring_bitmap_equals(small, small_copy));
        if (shared) {
            roaring_bitmap_t *expected2 = roaring_bitmap_or(shared, small);
            roaring_bitmap_free(shared);
            assert_true(roaring_bitmap_equals(big, expected2));
            roaring_bitmap_free(expected2);
        }
        roaring_bitmap_free(small);
        // The containers copied from 'small' must outlive it.
        assert_true(roaring_bitmap_internal_validate(big, &reason));
        assert_true(roaring_bitmap_equals(big, expected));
        roaring_bitmap_free(small_copy);
        roaring_bitmap_free(expected);
        roaring_bitmap_free(big);
    }
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

// Two bitset containers whose union is full must collapse to a single run
// container, both in the eager and in the lazy (or_many) code paths. This
// depends on CROARING_OR_BITSET_CONVERSION_TO_FULL and
// CROARING_LAZY_OR_BITSET_CONVERSION_TO_FULL being visible where they are
// tested, which the amalgamation once broke (issue #883).
DEFINE_TEST(test_full_bitset_union_becomes_run) {
    roaring_bitmap_t *evens = roaring_bitmap_create();
    roaring_bitmap_t *odds = roaring_bitmap_create();
    for (uint32_t v = 0; v < 65536; v += 2) {
        roaring_bitmap_add(evens, v);
        roaring_bitmap_add(odds, v + 1);
    }
    roaring_statistics_t stats;
    roaring_bitmap_statistics(evens, &stats);
    assert_int_equal(stats.n_bitset_containers, 1);
    roaring_bitmap_statistics(odds, &stats);
    assert_int_equal(stats.n_bitset_containers, 1);

    roaring_bitmap_t *eager = roaring_bitmap_copy(evens);
    roaring_bitmap_or_inplace(eager, odds);
    assert_int_equal(roaring_bitmap_get_cardinality(eager), 65536);
    roaring_bitmap_statistics(eager, &stats);
    assert_int_equal(stats.n_containers, 1);
    assert_int_equal(stats.n_run_containers, 1);
    assert_int_equal(stats.n_bitset_containers, 0);

    const roaring_bitmap_t *inputs[3] = {evens, odds, odds};
    roaring_bitmap_t *lazy = roaring_bitmap_or_many(3, inputs);
    assert_int_equal(roaring_bitmap_get_cardinality(lazy), 65536);
    roaring_bitmap_statistics(lazy, &stats);
    assert_int_equal(stats.n_containers, 1);
    assert_int_equal(stats.n_run_containers, 1);
    assert_int_equal(stats.n_bitset_containers, 0);

    roaring_bitmap_free(evens);
    roaring_bitmap_free(odds);
    roaring_bitmap_free(eager);
    roaring_bitmap_free(lazy);
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

/*
 * Read bitmap backward in steps of given size, compare with reference values.
 * If step is UINT32_MAX (special value), then read single non-empty container
 * at a time.
 */
void read_backward_compare(roaring_bitmap_t *r, const uint32_t *ref_values,
                           uint32_t ref_count, uint32_t step) {
    roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
    roaring_iterator_init_last(r, iter);
    uint32_t *buffer =
        malloc(sizeof(uint32_t) * (step == UINT32_MAX ? 65536 : step));
    uint32_t remaining = ref_count;
    while (remaining > 0) {
        assert_true(iter->has_value == true);
        assert_true(iter->current_value == ref_values[remaining - 1]);

        uint32_t num_ask = step;
        if (step == UINT32_MAX) {
            num_ask = 0;
            for (uint32_t i = remaining; i > 0; i--) {
                if ((ref_values[i - 1] >> 16) ==
                    (ref_values[remaining - 1] >> 16)) {
                    num_ask++;
                } else {
                    break;
                }
            }
        }

        uint32_t num_got =
            roaring_uint32_iterator_read_backward(iter, buffer, num_ask);
        assert_true(num_got == minimum_uint32(num_ask, remaining));
        for (uint32_t i = 0; i < num_got; i++) {
            assert_true(ref_values[remaining - 1 - i] == buffer[i]);
        }
        remaining -= num_got;
    }

    assert_true(iter->has_value == false);
    assert_true(iter->current_value == UINT32_MAX);

    assert_true(roaring_uint32_iterator_read_backward(iter, buffer, step) == 0);
    assert_true(iter->has_value == false);
    assert_true(iter->current_value == UINT32_MAX);

    free(buffer);
    roaring_uint32_iterator_free(iter);
}

void test_read_backward_uint32_iterator(uint8_t type) {
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
    roaring_iterator_init_last(r, iter);
    uint32_t buffer[1];
    uint32_t got = roaring_uint32_iterator_read_backward(iter, buffer, 0);
    assert_true(got == 0);
    assert_true(iter->has_value);
    assert_true(iter->current_value == ref_values[ref_count - 1]);
    roaring_uint32_iterator_free(iter);

    read_backward_compare(r, ref_values, ref_count, 1);
    read_backward_compare(r, ref_values, ref_count, 2);
    read_backward_compare(r, ref_values, ref_count, 7);
    read_backward_compare(r, ref_values, ref_count, ref_count - 1);
    read_backward_compare(r, ref_values, ref_count, ref_count);
    read_backward_compare(r, ref_values, ref_count,
                          UINT32_MAX);  // special value

    roaring_bitmap_free(r);
    free(ref_values);
}

DEFINE_TEST(test_read_backward_uint32_iterator_array) {
    test_read_backward_uint32_iterator(ARRAY_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_backward_uint32_iterator_bitset) {
    test_read_backward_uint32_iterator(BITSET_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_backward_uint32_iterator_run) {
    test_read_backward_uint32_iterator(RUN_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_backward_uint32_iterator_native) {
    test_read_backward_uint32_iterator(UINT8_MAX);  // special value
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

void test_uint32_iterator_skip(uint8_t type) {
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

    // Ensure skip(n) is equivalent to calling advance() n times
    roaring_uint32_iterator_t iter_skip;
    roaring_iterator_init(r, &iter_skip);

    for (uint32_t count = 0; count <= ref_count + 200; count += 181) {
        // Reset the iterator
        roaring_iterator_init(r, &iter_skip);

        uint32_t skip_result = roaring_uint32_iterator_skip(&iter_skip, count);

        // Should be equivalent
        assert_int_equal(skip_result, minimum_uint32(count, ref_count));
        bool expected_has_value = count < ref_count;
        assert_int_equal(iter_skip.has_value, expected_has_value);
        if (iter_skip.has_value) {
            assert_int_equal(iter_skip.current_value, ref_values[count]);
        }

        // Also skip but after advancing by one already
        if (count > 0) {
            roaring_iterator_init(r, &iter_skip);
            roaring_uint32_iterator_advance(&iter_skip);

            skip_result = roaring_uint32_iterator_skip(&iter_skip, count - 1);

            assert_int_equal(skip_result,
                             minimum_uint32(count - 1, ref_count - 1));
            assert_int_equal(iter_skip.has_value, expected_has_value);
            if (iter_skip.has_value) {
                assert_int_equal(iter_skip.current_value, ref_values[count]);
            }
        }
    }

    // Test skip way beyond end
    roaring_iterator_init(r, &iter_skip);
    uint32_t skipped = roaring_uint32_iterator_skip(&iter_skip, UINT32_MAX);
    assert_int_equal(skipped, ref_count);
    assert_false(iter_skip.has_value);

    // Ensure we can go back after skipping as far as we can.
    roaring_uint32_iterator_previous(&iter_skip);
    assert_true(iter_skip.has_value);
    assert_int_equal(iter_skip.current_value, ref_values[ref_count - 1]);

    roaring_bitmap_free(r);
    free(ref_values);
}

DEFINE_TEST(test_uint32_iterator_skip_array) {
    test_uint32_iterator_skip(ARRAY_CONTAINER_TYPE);
}

DEFINE_TEST(test_uint32_iterator_skip_bitset) {
    test_uint32_iterator_skip(BITSET_CONTAINER_TYPE);
}

DEFINE_TEST(test_uint32_iterator_skip_run) {
    test_uint32_iterator_skip(RUN_CONTAINER_TYPE);
}

DEFINE_TEST(test_uint32_iterator_skip_native) {
    test_uint32_iterator_skip(UINT8_MAX);
}

static void test_uint32_iterator_skip_backward(uint8_t type) {
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

    // Ensure skip(n) is equivalent to calling advance() n times
    roaring_uint32_iterator_t iter_skip;
    roaring_iterator_init(r, &iter_skip);

    for (uint32_t count = 0; count <= ref_count + 200; count += 181) {
        // Reset the iterator
        roaring_iterator_init_last(r, &iter_skip);

        uint32_t skip_result =
            roaring_uint32_iterator_skip_backward(&iter_skip, count);

        // Should be equivalent
        assert_int_equal(skip_result, minimum_uint32(count, ref_count));
        bool expected_has_value = count < ref_count;
        assert_int_equal(iter_skip.has_value, expected_has_value);
        if (iter_skip.has_value) {
            assert_int_equal(iter_skip.current_value,
                             ref_values[ref_count - count - 1]);
        }

        // Also skip but after advancing by one already
        if (count > 0) {
            roaring_iterator_init_last(r, &iter_skip);
            roaring_uint32_iterator_previous(&iter_skip);

            skip_result =
                roaring_uint32_iterator_skip_backward(&iter_skip, count - 1);

            assert_int_equal(skip_result,
                             minimum_uint32(count - 1, ref_count - 1));
            assert_int_equal(iter_skip.has_value, expected_has_value);
            if (iter_skip.has_value) {
                assert_int_equal(iter_skip.current_value,
                                 ref_values[ref_count - count - 1]);
            }
        }
    }

    // Test skip way beyond start
    roaring_iterator_init_last(r, &iter_skip);
    uint32_t skipped =
        roaring_uint32_iterator_skip_backward(&iter_skip, UINT32_MAX);
    assert_int_equal(skipped, ref_count);
    assert_false(iter_skip.has_value);

    // Ensure we can go forward after skipping backward as far as we can.
    roaring_uint32_iterator_advance(&iter_skip);
    assert_true(iter_skip.has_value);
    assert_int_equal(iter_skip.current_value, ref_values[0]);

    roaring_bitmap_free(r);
    free(ref_values);
}

DEFINE_TEST(test_uint32_iterator_skip_backward_array) {
    test_uint32_iterator_skip_backward(ARRAY_CONTAINER_TYPE);
}
DEFINE_TEST(test_uint32_iterator_skip_backward_bitset) {
    test_uint32_iterator_skip_backward(BITSET_CONTAINER_TYPE);
}
DEFINE_TEST(test_uint32_iterator_skip_backward_run) {
    test_uint32_iterator_skip_backward(RUN_CONTAINER_TYPE);
}
DEFINE_TEST(test_uint32_iterator_skip_backward_native) {
    test_uint32_iterator_skip_backward(UINT8_MAX);
}

/*
 * Convert a sorted array of values into an array of closed ranges.
 * Consecutive values are merged. Caller must free the returned array.
 */
static roaring_uint32_range_closed_t *ranges_from_values(
    const uint32_t *values, size_t count, uint32_t *num_ranges_out) {
    roaring_uint32_range_closed_t *ranges =
        (roaring_uint32_range_closed_t *)malloc(
            sizeof(roaring_uint32_range_closed_t) * (count + 1));
    uint32_t num_ranges = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (i == 0 || values[i] != values[i - 1] + 1) {
            ranges[num_ranges].min = values[i];
            ranges[num_ranges].max = values[i];
            num_ranges++;
        } else {
            ranges[num_ranges - 1].max = values[i];
        }
    }
    *num_ranges_out = num_ranges;
    return ranges;
}

/*
 * Read all ranges forward in batches of ${step}, comparing against ${expected}.
 */
static void ranges_read_compare(roaring_bitmap_t *r,
                                const roaring_uint32_range_closed_t *expected,
                                uint32_t num_ranges, uint32_t step) {
    roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
    roaring_uint32_range_closed_t *buffer =
        (roaring_uint32_range_closed_t *)malloc(
            sizeof(roaring_uint32_range_closed_t) * step);
    uint32_t ranges_read = 0;
    while (ranges_read < num_ranges) {
        assert_true(iter->has_value);
        assert_int_equal(iter->current_value, expected[ranges_read].min);

        size_t got = roaring_uint32_iterator_read_ranges(iter, buffer, step);
        size_t expect = minimum_uint32(step, num_ranges - ranges_read);
        assert_int_equal(got, expect);
        for (size_t i = 0; i < got; i++) {
            assert_int_equal(buffer[i].min, expected[ranges_read + i].min);
            assert_int_equal(buffer[i].max, expected[ranges_read + i].max);
        }
        ranges_read += got;
    }
    assert_false(iter->has_value);
    size_t got = roaring_uint32_iterator_read_ranges(iter, buffer, step);
    assert_int_equal(got, 0);
    assert_false(iter->has_value);

    free(buffer);
    roaring_uint32_iterator_free(iter);
}

/*
 * Read all ranges backward in batches of ${step}, comparing against
 * ${expected} (which is in forward order).
 */
static void ranges_read_prev_compare(
    roaring_bitmap_t *r, const roaring_uint32_range_closed_t *expected,
    uint32_t num_ranges, uint32_t step) {
    roaring_uint32_iterator_t iter;
    roaring_iterator_init_last(r, &iter);
    roaring_uint32_range_closed_t *buffer =
        (roaring_uint32_range_closed_t *)malloc(
            sizeof(roaring_uint32_range_closed_t) * step);
    uint32_t ranges_read = 0;
    while (ranges_read < num_ranges) {
        assert_true(iter.has_value);
        uint32_t range_idx = num_ranges - 1 - ranges_read;
        assert_int_equal(iter.current_value, expected[range_idx].max);

        size_t got =
            roaring_uint32_iterator_read_prev_ranges(&iter, buffer, step);
        size_t expect = minimum_uint32(step, num_ranges - ranges_read);
        assert_int_equal(got, expect);
        for (size_t i = 0; i < got; i++) {
            uint32_t idx = num_ranges - 1 - ranges_read - (uint32_t)i;
            assert_int_equal(buffer[i].min, expected[idx].min);
            assert_int_equal(buffer[i].max, expected[idx].max);
        }
        ranges_read += (uint32_t)got;
    }
    assert_false(iter.has_value);
    size_t got = roaring_uint32_iterator_read_prev_ranges(&iter, buffer, step);
    assert_int_equal(got, 0);
    assert_false(iter.has_value);

    free(buffer);
}

static void test_read_ranges_iterator(uint8_t type) {
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

    // Zero count should be a no-op
    roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
    roaring_uint32_range_closed_t buf[1];
    size_t got = roaring_uint32_iterator_read_ranges(iter, buf, 0);
    assert_int_equal(got, 0);
    assert_true(iter->has_value);
    assert_int_equal(iter->current_value, 0);
    roaring_uint32_iterator_free(iter);

    uint32_t num_ranges;
    roaring_uint32_range_closed_t *expected =
        ranges_from_values(ref_values, ref_count, &num_ranges);

    ranges_read_compare(r, expected, num_ranges, 1);
    ranges_read_compare(r, expected, num_ranges, 2);
    ranges_read_compare(r, expected, num_ranges, 7);
    ranges_read_compare(r, expected, num_ranges, 100000);

    free(expected);
    roaring_bitmap_free(r);
    free(ref_values);
}

DEFINE_TEST(test_read_ranges_iterator_array) {
    test_read_ranges_iterator(ARRAY_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_ranges_iterator_bitset) {
    test_read_ranges_iterator(BITSET_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_ranges_iterator_run) {
    test_read_ranges_iterator(RUN_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_ranges_iterator_native) {
    test_read_ranges_iterator(UINT8_MAX);
}

static void test_read_prev_ranges_iterator(uint8_t type) {
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

    // Zero count should be a no-op
    roaring_uint32_iterator_t iter;
    roaring_iterator_init_last(r, &iter);
    roaring_uint32_range_closed_t buf[1];
    size_t got = roaring_uint32_iterator_read_prev_ranges(&iter, buf, 0);
    assert_int_equal(got, 0);
    assert_true(iter.has_value);
    assert_int_equal(iter.current_value, UINT32_MAX);

    uint32_t num_ranges;
    roaring_uint32_range_closed_t *expected =
        ranges_from_values(ref_values, ref_count, &num_ranges);

    ranges_read_prev_compare(r, expected, num_ranges, 1);
    ranges_read_prev_compare(r, expected, num_ranges, 2);
    ranges_read_prev_compare(r, expected, num_ranges, 7);
    ranges_read_prev_compare(r, expected, num_ranges, 100000);

    free(expected);
    roaring_bitmap_free(r);
    free(ref_values);
}

DEFINE_TEST(test_read_prev_ranges_iterator_array) {
    test_read_prev_ranges_iterator(ARRAY_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_prev_ranges_iterator_bitset) {
    test_read_prev_ranges_iterator(BITSET_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_prev_ranges_iterator_run) {
    test_read_prev_ranges_iterator(RUN_CONTAINER_TYPE);
}
DEFINE_TEST(test_read_prev_ranges_iterator_native) {
    test_read_prev_ranges_iterator(UINT8_MAX);
}

DEFINE_TEST(test_read_ranges_empty_bitmap) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    roaring_uint32_range_closed_t buf[1];

    // Forward
    roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
    size_t got = roaring_uint32_iterator_read_ranges(iter, buf, 1);
    assert_int_equal(got, 0);
    assert_false(iter->has_value);
    roaring_uint32_iterator_free(iter);

    // Backward
    roaring_uint32_iterator_t riter;
    roaring_iterator_init_last(r, &riter);
    got = roaring_uint32_iterator_read_prev_ranges(&riter, buf, 1);
    assert_int_equal(got, 0);
    assert_false(riter.has_value);

    roaring_bitmap_free(r);
}

DEFINE_TEST(test_read_ranges_cross_container) {
    roaring_bitmap_t *r = roaring_bitmap_create();

    // Values spanning two containers: high16=0 ending, high16=1 starting
    // This creates a single range [0xFFFE..0x10001]
    roaring_bitmap_add(r, 0xFFFE);
    roaring_bitmap_add(r, 0xFFFF);
    roaring_bitmap_add(r, 0x10000);
    roaring_bitmap_add(r, 0x10001);

    roaring_bitmap_add(r, 0x20000);

    roaring_uint32_range_closed_t buf[4];

    roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
    size_t got = roaring_uint32_iterator_read_ranges(iter, buf, 4);
    assert_int_equal(got, 2);
    assert_int_equal(buf[0].min, 0xFFFE);
    assert_int_equal(buf[0].max, 0x10001);
    assert_int_equal(buf[1].min, 0x20000);
    assert_int_equal(buf[1].max, 0x20000);
    assert_false(iter->has_value);
    roaring_uint32_iterator_free(iter);

    roaring_uint32_iterator_t riter;
    roaring_iterator_init_last(r, &riter);
    got = roaring_uint32_iterator_read_prev_ranges(&riter, buf, 4);
    assert_int_equal(got, 2);
    assert_int_equal(buf[0].min, 0x20000);
    assert_int_equal(buf[0].max, 0x20000);
    assert_int_equal(buf[1].min, 0xFFFE);
    assert_int_equal(buf[1].max, 0x10001);
    assert_false(riter.has_value);

    roaring_bitmap_free(r);
}

DEFINE_TEST(test_read_ranges_mid_range) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    roaring_bitmap_add_range_closed(r, 0, 10);
    roaring_bitmap_add_range_closed(r, 20, 25);

    roaring_uint32_range_closed_t buf[4];

    // Forward: start mid-range at 5, first range should be [5..10]
    {
        roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
        roaring_uint32_iterator_move_equalorlarger(iter, 5);
        assert_int_equal(iter->current_value, 5);

        size_t got = roaring_uint32_iterator_read_ranges(iter, buf, 4);
        assert_int_equal(got, 2);
        assert_int_equal(buf[0].min, 5);
        assert_int_equal(buf[0].max, 10);
        assert_int_equal(buf[1].min, 20);
        assert_int_equal(buf[1].max, 25);
        assert_false(iter->has_value);
        roaring_uint32_iterator_free(iter);
    }

    // Backward: start mid-range at 22, first range should be [20..22]
    {
        roaring_uint32_iterator_t iter;
        roaring_iterator_init_last(r, &iter);
        roaring_uint32_iterator_move_equalorlarger(&iter, 22);
        assert_int_equal(iter.current_value, 22);

        size_t got = roaring_uint32_iterator_read_prev_ranges(&iter, buf, 4);
        assert_int_equal(got, 2);
        assert_int_equal(buf[0].min, 20);
        assert_int_equal(buf[0].max, 22);
        assert_int_equal(buf[1].min, 0);
        assert_int_equal(buf[1].max, 10);
        assert_false(iter.has_value);
    }

    roaring_bitmap_free(r);
}

DEFINE_TEST(test_read_ranges_interleaved) {
    roaring_bitmap_t *r = roaring_bitmap_create();

    // Ranges: [10..12], [20..20], [30..32], [40..40], [50..55]
    roaring_bitmap_add_range_closed(r, 10, 12);
    roaring_bitmap_add(r, 20);
    roaring_bitmap_add_range_closed(r, 30, 32);
    roaring_bitmap_add(r, 40);
    roaring_bitmap_add_range_closed(r, 50, 55);

    roaring_uint32_range_closed_t buf[4];

    // read_ranges then advance then read_ranges
    {
        roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
        size_t got = roaring_uint32_iterator_read_ranges(iter, buf, 1);
        assert_int_equal(got, 1);
        assert_int_equal(buf[0].min, 10);
        assert_int_equal(buf[0].max, 12);
        // Iterator should be at 20
        assert_true(iter->has_value);
        assert_int_equal(iter->current_value, 20);

        // advance past 20
        roaring_uint32_iterator_advance(iter);
        assert_true(iter->has_value);
        assert_int_equal(iter->current_value, 30);

        // read_ranges again
        got = roaring_uint32_iterator_read_ranges(iter, buf, 2);
        assert_int_equal(got, 2);
        assert_int_equal(buf[0].min, 30);
        assert_int_equal(buf[0].max, 32);
        assert_int_equal(buf[1].min, 40);
        assert_int_equal(buf[1].max, 40);

        assert_true(iter->has_value);
        assert_int_equal(iter->current_value, 50);
        roaring_uint32_iterator_free(iter);
    }

    // read_ranges then move_equalorlarger then read_ranges
    {
        roaring_uint32_iterator_t *iter = roaring_iterator_create(r);
        size_t got = roaring_uint32_iterator_read_ranges(iter, buf, 1);
        assert_int_equal(got, 1);
        assert_int_equal(buf[0].min, 10);
        assert_int_equal(buf[0].max, 12);

        // Jump ahead to 40
        roaring_uint32_iterator_move_equalorlarger(iter, 40);
        assert_true(iter->has_value);
        assert_int_equal(iter->current_value, 40);

        got = roaring_uint32_iterator_read_ranges(iter, buf, 4);
        assert_int_equal(got, 2);
        assert_int_equal(buf[0].min, 40);
        assert_int_equal(buf[0].max, 40);
        assert_int_equal(buf[1].min, 50);
        assert_int_equal(buf[1].max, 55);
        assert_false(iter->has_value);
        roaring_uint32_iterator_free(iter);
    }

    // read_prev_ranges then previous then read_prev_ranges
    {
        roaring_uint32_iterator_t iter;
        roaring_iterator_init_last(r, &iter);
        size_t got = roaring_uint32_iterator_read_prev_ranges(&iter, buf, 1);
        assert_int_equal(got, 1);
        assert_int_equal(buf[0].min, 50);
        assert_int_equal(buf[0].max, 55);
        // Iterator should be at 40
        assert_true(iter.has_value);
        assert_int_equal(iter.current_value, 40);

        // previous past 40
        roaring_uint32_iterator_previous(&iter);
        assert_true(iter.has_value);
        assert_int_equal(iter.current_value, 32);

        // read_prev_ranges again
        got = roaring_uint32_iterator_read_prev_ranges(&iter, buf, 2);
        assert_int_equal(got, 2);
        assert_int_equal(buf[0].min, 30);
        assert_int_equal(buf[0].max, 32);
        assert_int_equal(buf[1].min, 20);
        assert_int_equal(buf[1].max, 20);

        assert_true(iter.has_value);
        assert_int_equal(iter.current_value, 12);
    }

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

// A mutator must unshare a container before editing it in place, or it edits a
// container the bitmaps copied from it still own.
#define COW_SPARSE_VALUE (5 << 16)
#define COW_DENSE_END (1 << 17)
#define COW_CARDINALITY (COW_DENSE_END + 1)

static roaring_bitmap_t *cow_source(bool run_optimize) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r, true);
    roaring_bitmap_add_range(r, 0, COW_DENSE_END);
    roaring_bitmap_add(r, COW_SPARSE_VALUE);
    if (run_optimize) {
        roaring_bitmap_run_optimize(r);
    }
    return r;
}

static void assert_cow_intact(const roaring_bitmap_t *r) {
    assert_bitmap_validate(r);
    assert_true(roaring_bitmap_get_cardinality(r) == COW_CARDINALITY);
    assert_true(roaring_bitmap_contains(r, 0));
    assert_true(roaring_bitmap_contains(r, COW_DENSE_END - 1));
    assert_true(roaring_bitmap_contains(r, COW_SPARSE_VALUE));
}

// Mutates one side of a sharing pair and checks the other side is unchanged,
// with the roles reversed the second time.
static void check_unshares(void (*mutate)(roaring_bitmap_t *),
                           bool run_optimize) {
    roaring_bitmap_t *source = cow_source(run_optimize);
    roaring_bitmap_t *copy = roaring_bitmap_copy(source);
    mutate(copy);
    assert_cow_intact(source);
    assert_bitmap_validate(copy);
    roaring_bitmap_free(copy);
    roaring_bitmap_free(source);

    source = cow_source(run_optimize);
    copy = roaring_bitmap_copy(source);
    mutate(source);
    assert_cow_intact(copy);
    assert_bitmap_validate(source);
    roaring_bitmap_free(copy);
    roaring_bitmap_free(source);
}

static void cow_remove_many(roaring_bitmap_t *r) {
    uint32_t values[] = {1, 3, 5, 7, 65536 + 1, 65536 + 3, COW_SPARSE_VALUE};
    roaring_bitmap_remove_many(r, sizeof(values) / sizeof(values[0]), values);
}

static void cow_remove_many_one_container(roaring_bitmap_t *r) {
    uint32_t values[] = {1, 2, 3};
    roaring_bitmap_remove_many(r, sizeof(values) / sizeof(values[0]), values);
}

static void cow_remove_many_empties_container(roaring_bitmap_t *r) {
    uint32_t value = COW_SPARSE_VALUE;
    roaring_bitmap_remove_many(r, 1, &value);
}

static void cow_add_many(roaring_bitmap_t *r) {
    uint32_t values[] = {COW_DENSE_END + 1, COW_DENSE_END + 3, 2, 4};
    roaring_bitmap_add_many(r, sizeof(values) / sizeof(values[0]), values);
}

static void cow_add_bulk(roaring_bitmap_t *r) {
    roaring_bulk_context_t context = {0, 0, 0, 0};
    for (uint32_t v = COW_DENSE_END + 1; v < COW_DENSE_END + 16; v++) {
        roaring_bitmap_add_bulk(r, &context, v);
    }
    roaring_bitmap_add_bulk(r, &context, 2);
}

static void cow_add(roaring_bitmap_t *r) { roaring_bitmap_add(r, 2); }

static void cow_add_checked(roaring_bitmap_t *r) {
    assert_false(roaring_bitmap_add_checked(r, 2));
}

static void cow_remove(roaring_bitmap_t *r) { roaring_bitmap_remove(r, 2); }

static void cow_remove_checked(roaring_bitmap_t *r) {
    assert_true(roaring_bitmap_remove_checked(r, 2));
}

static void cow_add_range(roaring_bitmap_t *r) {
    roaring_bitmap_add_range(r, COW_DENSE_END + 1, COW_DENSE_END + 100);
}

static void cow_remove_range(roaring_bitmap_t *r) {
    roaring_bitmap_remove_range(r, 10, 100);
}

static void cow_flip_inplace(roaring_bitmap_t *r) {
    roaring_bitmap_flip_inplace(r, 10, 100);
}

static void cow_run_optimize(roaring_bitmap_t *r) {
    roaring_bitmap_run_optimize(r);
}

static void cow_remove_run_compression(roaring_bitmap_t *r) {
    roaring_bitmap_remove_run_compression(r);
}

static void cow_shrink_to_fit(roaring_bitmap_t *r) {
    roaring_bitmap_shrink_to_fit(r);
}

static void cow_and_inplace(roaring_bitmap_t *r) {
    roaring_bitmap_t *other = roaring_bitmap_from_range(0, 100, 1);
    roaring_bitmap_and_inplace(r, other);
    roaring_bitmap_free(other);
}

static void cow_or_inplace(roaring_bitmap_t *r) {
    roaring_bitmap_t *other =
        roaring_bitmap_from_range(COW_DENSE_END + 1, COW_DENSE_END + 100, 1);
    roaring_bitmap_or_inplace(r, other);
    roaring_bitmap_free(other);
}

static void cow_xor_inplace(roaring_bitmap_t *r) {
    roaring_bitmap_t *other = roaring_bitmap_from_range(0, 100, 1);
    roaring_bitmap_xor_inplace(r, other);
    roaring_bitmap_free(other);
}

static void cow_andnot_inplace(roaring_bitmap_t *r) {
    roaring_bitmap_t *other = roaring_bitmap_from_range(0, 100, 1);
    roaring_bitmap_andnot_inplace(r, other);
    roaring_bitmap_free(other);
}

DEFINE_TEST(test_remove_many_copy_on_write) {
    for (int run_optimize = 0; run_optimize <= 1; run_optimize++) {
        check_unshares(cow_remove_many, run_optimize);
        check_unshares(cow_remove_many_one_container, run_optimize);
        check_unshares(cow_remove_many_empties_container, run_optimize);
    }
}

DEFINE_TEST(test_remove_many_copy_on_write_many_copies) {
    roaring_bitmap_t *source = cow_source(false);
    roaring_bitmap_t *copies[4];
    for (size_t i = 0; i < 4; i++) {
        copies[i] = roaring_bitmap_copy(source);
    }
    for (size_t i = 0; i < 4; i++) {
        cow_remove_many(copies[i]);
        assert_bitmap_validate(copies[i]);
        assert_cow_intact(source);
        for (size_t j = i + 1; j < 4; j++) {
            assert_cow_intact(copies[j]);
        }
    }
    for (size_t i = 0; i < 4; i++) {
        roaring_bitmap_free(copies[i]);
    }
    assert_cow_intact(source);
    roaring_bitmap_free(source);
}

// The other in-place mutators are already correct; these keep them that way.
DEFINE_TEST(test_inplace_mutators_copy_on_write) {
    void (*const mutators[])(roaring_bitmap_t *) = {
        cow_add,           cow_add_checked,   cow_add_many,
        cow_add_bulk,      cow_remove,        cow_remove_checked,
        cow_add_range,     cow_remove_range,  cow_flip_inplace,
        cow_run_optimize,  cow_shrink_to_fit, cow_remove_run_compression,
        cow_and_inplace,   cow_or_inplace,    cow_xor_inplace,
        cow_andnot_inplace};
    const size_t count = sizeof(mutators) / sizeof(mutators[0]);
    for (int run_optimize = 0; run_optimize <= 1; run_optimize++) {
        for (size_t i = 0; i < count; i++) {
            check_unshares(mutators[i], run_optimize);
        }
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

// The uint64_t range functions must treat an end beyond UINT32_MAX + 1 as
// UINT32_MAX + 1, never let it wrap around when cast to uint32_t.
DEFINE_TEST(test_range_end_beyond_universe) {
    const uint64_t universe = (uint64_t)UINT32_MAX + 1;  // 2^32

    // remove_range (port of Go's TestRoaringRangeEnd)
    roaring_bitmap_t *r = roaring_bitmap_of(1, UINT32_MAX);
    roaring_bitmap_remove_range(r, 0, UINT32_MAX);
    assert_int_equal(roaring_bitmap_get_cardinality(r), 1);
    roaring_bitmap_remove_range(r, 0, UINT64_MAX);
    assert_int_equal(roaring_bitmap_get_cardinality(r), 0);
    roaring_bitmap_add(r, UINT32_MAX);
    roaring_bitmap_remove_range(r, 0, universe + 1);
    assert_int_equal(roaring_bitmap_get_cardinality(r), 0);
    roaring_bitmap_add(r, UINT32_MAX);
    roaring_bitmap_remove_range(r, 0, universe);
    assert_int_equal(roaring_bitmap_get_cardinality(r), 0);
    // a range starting at 2^32 is empty: it must not remove 0
    roaring_bitmap_add(r, 0);
    roaring_bitmap_remove_range(r, universe, universe + 1);
    assert_int_equal(roaring_bitmap_get_cardinality(r), 1);
    roaring_bitmap_free(r);

    // add_range
    r = roaring_bitmap_create();
    roaring_bitmap_add_range(r, universe, universe + 1);
    roaring_bitmap_add_range(r, universe + 5, universe + 10);
    assert_true(roaring_bitmap_is_empty(r));
    roaring_bitmap_add_range(r, UINT32_MAX, UINT64_MAX);
    assert_int_equal(roaring_bitmap_get_cardinality(r), 1);
    assert_true(roaring_bitmap_contains(r, UINT32_MAX));
    roaring_bitmap_add_range(r, 0, universe + 1);
    assert_true(roaring_bitmap_get_cardinality(r) == universe);
    roaring_bitmap_free(r);

    // range_cardinality
    r = roaring_bitmap_of(2, 5, UINT32_MAX);
    assert_int_equal(roaring_bitmap_range_cardinality(r, 0, universe + 1), 2);
    assert_int_equal(roaring_bitmap_range_cardinality(r, 0, UINT64_MAX), 2);
    assert_int_equal(
        roaring_bitmap_range_cardinality(r, UINT32_MAX, UINT64_MAX), 1);
    assert_int_equal(roaring_bitmap_range_cardinality(r, universe, UINT64_MAX),
                     0);
    roaring_bitmap_free(r);

    // contains_range
    r = roaring_bitmap_of(1, 0);
    assert_false(roaring_bitmap_contains_range(r, 0, universe + 1));
    assert_false(roaring_bitmap_contains_range(r, 0, universe + 6));
    roaring_bitmap_add(r, UINT32_MAX);
    assert_true(roaring_bitmap_contains_range(r, UINT32_MAX, UINT64_MAX));
    roaring_bitmap_add_range(r, 0, universe);
    assert_true(roaring_bitmap_contains_range(r, 0, universe + 1));
    assert_true(roaring_bitmap_contains_range(r, 0, UINT64_MAX));
    roaring_bitmap_free(r);

    // flip and flip_inplace
    roaring_bitmap_t *empty = roaring_bitmap_create();
    roaring_bitmap_t *f = roaring_bitmap_flip(empty, universe, universe + 1);
    assert_true(roaring_bitmap_is_empty(f));
    roaring_bitmap_free(f);
    f = roaring_bitmap_flip(empty, 0, universe + 1);
    assert_true(roaring_bitmap_get_cardinality(f) == universe);
    roaring_bitmap_free(f);
    f = roaring_bitmap_flip(empty, UINT32_MAX, UINT64_MAX);
    assert_int_equal(roaring_bitmap_get_cardinality(f), 1);
    assert_true(roaring_bitmap_contains(f, UINT32_MAX));
    roaring_bitmap_flip_inplace(f, universe, universe + 1);
    assert_int_equal(roaring_bitmap_get_cardinality(f), 1);
    roaring_bitmap_flip_inplace(f, UINT32_MAX, universe + 5);
    assert_true(roaring_bitmap_is_empty(f));
    roaring_bitmap_flip_inplace(f, 0, universe + 1);
    assert_true(roaring_bitmap_get_cardinality(f) == universe);
    roaring_bitmap_free(f);
    roaring_bitmap_free(empty);

    // intersect_with_range
    r = roaring_bitmap_of(1, 5);
    assert_false(
        roaring_bitmap_intersect_with_range(r, universe + 5, universe + 10));
    assert_false(roaring_bitmap_intersect_with_range(r, universe, UINT64_MAX));
    assert_true(roaring_bitmap_intersect_with_range(r, 0, UINT64_MAX));
    assert_true(roaring_bitmap_intersect_with_range(r, 5, universe + 1));
    roaring_bitmap_add(r, UINT32_MAX);
    assert_true(roaring_bitmap_intersect_with_range(r, UINT32_MAX, UINT64_MAX));
    assert_false(roaring_bitmap_intersect_with_range(r, universe + UINT32_MAX,
                                                     UINT64_MAX));
    roaring_bitmap_free(r);

    // from_range with a large step and max reaching 2^32 must terminate
    r = roaring_bitmap_from_range(0, universe, 65536);
    assert_int_equal(roaring_bitmap_get_cardinality(r), 65536);
    assert_true(roaring_bitmap_contains(r, UINT32_MAX - 65535));
    roaring_bitmap_free(r);
    r = roaring_bitmap_from_range(UINT32_MAX, universe + 100, 65536);
    assert_int_equal(roaring_bitmap_get_cardinality(r), 1);
    assert_true(roaring_bitmap_contains(r, UINT32_MAX));
    roaring_bitmap_free(r);
    r = roaring_bitmap_from_range(0, UINT64_MAX, UINT32_MAX);
    assert_int_equal(roaring_bitmap_get_cardinality(r), 2);
    assert_true(roaring_bitmap_contains(r, UINT32_MAX));
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

DEFINE_TEST(test_frozen_serialization_empty) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    assert_true(r->high_low_container.keys == NULL);
    frozen_serialization_compare(r);
}

// Copy-on-write bitmaps hold shared containers once they have been copied.
DEFINE_TEST(test_frozen_serialization_shared_containers) {
    const uint64_t s = 65536;

    roaring_bitmap_t *r = roaring_bitmap_create();
    roaring_bitmap_add(r, 1);                 // array container
    roaring_bitmap_add_range(r, s, s + 100);  // run container
    for (uint64_t i = 0; i < s; i += 2) {     // bitset container
        roaring_bitmap_add(r, s * 2 + i);
    }
    roaring_bitmap_run_optimize(r);
    size_t expected_bytes = roaring_bitmap_frozen_size_in_bytes(r);

    roaring_bitmap_set_copy_on_write(r, true);
    roaring_bitmap_t *copy = roaring_bitmap_copy(r);
    for (int32_t i = 0; i < r->high_low_container.size; i++) {
        assert_int_equal(r->high_low_container.typecodes[i],
                         SHARED_CONTAINER_TYPE);
    }
    assert_int_equal(roaring_bitmap_frozen_size_in_bytes(r), expected_bytes);
    frozen_serialization_compare(r);
    roaring_bitmap_free(copy);
}

#if ROARING_UNSAFE_FROZEN_TESTS
// This test is unsafe, as it may trigger unaligned memory access
// It is only enabled if ROARING_UNSAFE_FROZEN_TESTS is defined.
DEFINE_TEST(test_portable_deserialize_frozen) {
#if CROARING_IS_BIG_ENDIAN
    // The portable format is little-endian and this view uses the payloads
    // where they sit, so the function refuses on a big-endian host. Check
    // that contract; the rest of the test has nothing to view.
    {
        roaring_bitmap_t *r = roaring_bitmap_from(1, 2, 3, 100, 1000, 10000);
        uint32_t size = roaring_bitmap_portable_size_in_bytes(r);
        char *buf = (char *)malloc(size);
        assert_int_equal(roaring_bitmap_portable_serialize(r, buf), size);
        assert_null(roaring_bitmap_portable_deserialize_frozen(buf));
        free(buf);
        roaring_bitmap_free(r);
        return;
    }
#else
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
#endif
}

#if !CROARING_IS_BIG_ENDIAN
static bool count_values(uint32_t value, void *param) {
    (void)value;
    (*(uint64_t *)param)++;
    return true;
}

// Run the read-only API over a portable frozen view of "r" whose buffer starts
// "offset" bytes past an aligned address. The container payloads then sit at
// any alignment, which the view must tolerate.
static void check_frozen_view_queries(const roaring_bitmap_t *r,
                                      size_t offset) {
    size_t size = roaring_bitmap_portable_size_in_bytes(r);
    char *storage = (char *)malloc(size + offset);
    char *buf = storage + offset;
    assert_int_equal(roaring_bitmap_portable_serialize(r, buf), size);
    roaring_bitmap_t *v = roaring_bitmap_portable_deserialize_frozen(buf);
    assert_non_null(v);

    const char *reason = NULL;
    assert_true(roaring_bitmap_internal_validate(v, &reason));
    assert_true(roaring_bitmap_equals(v, r));
    assert_true(roaring_bitmap_equals(r, v));
    assert_true(roaring_bitmap_is_subset(v, r));
    assert_true(roaring_bitmap_is_subset(r, v));
    assert_false(roaring_bitmap_is_strict_subset(v, r));
    assert_false(roaring_bitmap_is_empty(v));

    uint64_t card = roaring_bitmap_get_cardinality(r);
    assert_int_equal(roaring_bitmap_get_cardinality(v), card);
    assert_int_equal(roaring_bitmap_minimum(v), roaring_bitmap_minimum(r));
    assert_int_equal(roaring_bitmap_maximum(v), roaring_bitmap_maximum(r));
    assert_int_equal(roaring_bitmap_range_cardinality(v, 100, 200000),
                     roaring_bitmap_range_cardinality(r, 100, 200000));
    assert_int_equal(roaring_bitmap_contains_range(v, 140000, 150000),
                     roaring_bitmap_contains_range(r, 140000, 150000));
    assert_true(roaring_bitmap_intersect(v, r));
    assert_int_equal(roaring_bitmap_intersect_with_range(v, 70000, 70100),
                     roaring_bitmap_intersect_with_range(r, 70000, 70100));

    uint32_t *values = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(v, values);
    for (uint64_t i = 0; i < card; i += 97) {
        uint32_t element;
        assert_true(roaring_bitmap_contains(v, values[i]));
        assert_false(roaring_bitmap_contains(v, values[i] + 1) !=
                     roaring_bitmap_contains(r, values[i] + 1));
        assert_int_equal(roaring_bitmap_rank(v, values[i]), i + 1);
        assert_int_equal(roaring_bitmap_get_index(v, values[i]), (int64_t)i);
        assert_true(roaring_bitmap_select(v, (uint32_t)i, &element));
        assert_int_equal(element, values[i]);
    }
    uint64_t ranks[3];
    uint32_t probes[3] = {values[0], values[card / 2], values[card - 1]};
    roaring_bitmap_rank_many(v, probes, probes + 3, ranks);
    assert_int_equal(ranks[2], card);
    uint32_t page[16];
    assert_true(roaring_bitmap_range_uint32_array(v, card / 2, 16, page));
    assert_int_equal(page[0], values[card / 2]);

    uint64_t visited = 0;
    assert_true(roaring_iterate(v, count_values, &visited));
    assert_int_equal(visited, card);
    roaring_uint32_iterator_t *it = roaring_iterator_create(v);
    uint32_t read[64];
    assert_int_equal(roaring_uint32_iterator_read(it, read, 64), 64);
    assert_int_equal(read[63], values[63]);
    assert_true(
        roaring_uint32_iterator_move_equalorlarger(it, values[card / 3]));
    assert_int_equal(it->current_value, values[card / 3]);
    assert_true(roaring_uint32_iterator_previous(it));
    roaring_uint32_iterator_free(it);
    free(values);

    roaring_bitmap_t *results[] = {
        roaring_bitmap_and(v, r),    roaring_bitmap_and(r, v),
        roaring_bitmap_or(v, r),     roaring_bitmap_or(r, v),
        roaring_bitmap_copy(v),      roaring_bitmap_flip(v, 0, 200000),
        roaring_bitmap_xor(v, r),    roaring_bitmap_andnot(v, r),
        roaring_bitmap_andnot(r, v), roaring_bitmap_lazy_or(v, r, true),
    };
    roaring_bitmap_repair_after_lazy(results[9]);
    for (size_t i = 0; i < 5; i++) {
        assert_true(roaring_bitmap_equals(results[i], r));
    }
    assert_int_equal(roaring_bitmap_get_cardinality(results[6]), 0);
    assert_int_equal(roaring_bitmap_get_cardinality(results[7]), 0);
    assert_true(roaring_bitmap_equals(results[9], r));
    roaring_bitmap_and_inplace(results[0], v);
    roaring_bitmap_or_inplace(results[1], v);
    roaring_bitmap_xor_inplace(results[2], v);
    roaring_bitmap_andnot_inplace(results[3], v);
    assert_true(roaring_bitmap_equals(results[0], r));
    assert_true(roaring_bitmap_equals(results[1], r));
    assert_true(roaring_bitmap_is_empty(results[2]));
    assert_true(roaring_bitmap_is_empty(results[3]));
    for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
        roaring_bitmap_free(results[i]);
    }
    assert_int_equal(roaring_bitmap_and_cardinality(v, r), card);
    assert_int_equal(roaring_bitmap_or_cardinality(v, r), card);
    assert_int_equal(roaring_bitmap_xor_cardinality(v, r), 0);
    assert_int_equal(roaring_bitmap_andnot_cardinality(v, r), 0);
    assert_true(roaring_bitmap_jaccard_index(v, r) == 1.0);

    roaring_statistics_t stats;
    roaring_bitmap_statistics(v, &stats);
    assert_int_equal(stats.cardinality, card);
    char *again = (char *)malloc(size);
    assert_int_equal(roaring_bitmap_portable_serialize(v, again), size);
    assert_memory_equal(again, buf, size);
    free(again);

    roaring_bitmap_free(v);
    free(storage);
}
#endif  // !CROARING_IS_BIG_ENDIAN

// A portable frozen view uses the container payloads where they sit in the
// buffer, so they may be unaligned: every read path must tolerate that.
DEFINE_TEST(test_portable_deserialize_frozen_unaligned) {
#if !CROARING_IS_BIG_ENDIAN
    roaring_bitmap_t *r = roaring_bitmap_create();
    for (uint32_t i = 1; i < 3000; i += 3) roaring_bitmap_add(r, i);  // array
    for (uint32_t i = 65536; i < 65536 + 20000; i += 2) {
        roaring_bitmap_add(r, i);  // bitset
    }
    roaring_bitmap_add_range(r, 140000, 150000);  // run
    roaring_bitmap_run_optimize(r);
    // Three containers: no offset headers, and a single run-flag byte puts
    // every payload at an odd offset.
    for (size_t offset = 0; offset < 8; offset++) {
        check_frozen_view_queries(r, offset);
    }
    // Enough containers for offset headers.
    for (uint32_t k = 3; k < 12; k++) {
        roaring_bitmap_add(r, (k << 16) + 7 * k);
    }
    for (size_t offset = 0; offset < 8; offset++) {
        check_frozen_view_queries(r, offset);
    }
    // No run containers: the other cookie.
    roaring_bitmap_remove_run_compression(r);
    for (size_t offset = 0; offset < 8; offset++) {
        check_frozen_view_queries(r, offset);
    }
    // The bitset container first, for the minimum.
    roaring_bitmap_remove_range(r, 0, 65536);
    for (size_t offset = 0; offset < 8; offset++) {
        check_frozen_view_queries(r, offset);
    }
    roaring_bitmap_free(r);
#endif
}
#endif  // ROARING_UNSAFE_FROZEN_TESTS

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

DEFINE_TEST(fuzz_deserializer) {
    for (size_t i = 0; i < 10000; i++) {
        size_t vec_size = our_rand() % 10000;
        char *buffer = malloc(vec_size);
        for (size_t j = 0; j < vec_size; j++) {
            buffer[j] = our_rand() % 256;
        }
        deserialization_test(buffer, vec_size);
        free(buffer);
    }
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

static roaring_bitmap_t *offset_by_value(const roaring_bitmap_t *r,
                                         int64_t offset) {
    roaring_bitmap_t *answer = roaring_bitmap_create();
    roaring_uint32_iterator_t *it = roaring_iterator_create(r);
    for (; it->has_value; roaring_uint32_iterator_advance(it)) {
        int64_t v = (int64_t)it->current_value + offset;
        if (v >= 0 && v <= UINT32_MAX) roaring_bitmap_add(answer, (uint32_t)v);
    }
    roaring_uint32_iterator_free(it);
    return answer;
}

DEFINE_TEST(add_offset_each_container_type) {
    roaring_bitmap_t *array = roaring_bitmap_from_range(0, 65536, 37);
    roaring_bitmap_t *bitset = roaring_bitmap_from_range(0, 65536, 3);
    roaring_bitmap_t *runs = roaring_bitmap_create();
    for (uint32_t x = 0; x < 65536; x += 50) {
        roaring_bitmap_add_range(runs, x, x + 10);
    }
    roaring_bitmap_t *inputs[] = {array, bitset, runs};
    const int64_t offsets[] = {1, 63, 64, 1000, 65535, -1, -1000};
    for (size_t k = 0; k < 3; ++k) {
        roaring_bitmap_add(inputs[k], 65535);  // a bit in the last word
        roaring_bitmap_run_optimize(inputs[k]);
        for (size_t o = 0; o < sizeof(offsets) / sizeof(offsets[0]); ++o) {
            roaring_bitmap_t *shifted =
                roaring_bitmap_add_offset(inputs[k], offsets[o]);
            roaring_bitmap_t *expected = offset_by_value(inputs[k], offsets[o]);
            assert_true(roaring_bitmap_equals(shifted, expected));
            roaring_bitmap_free(shifted);
            roaring_bitmap_free(expected);
        }
        roaring_bitmap_free(inputs[k]);
    }
}

// The first operand keeps several runs past the second's last one.
DEFINE_TEST(andnot_run_containers_tail) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_t *r2 = roaring_bitmap_create();
    for (uint32_t x = 0; x < 4000; x += 20) {
        roaring_bitmap_add_range(r1, x, x + 5);
        if (x < 1000) roaring_bitmap_add_range(r2, x + 1, x + 4);
    }
    roaring_bitmap_run_optimize(r1);
    roaring_bitmap_run_optimize(r2);
    roaring_bitmap_t *diff = roaring_bitmap_andnot(r1, r2);
    roaring_bitmap_t *expected = roaring_bitmap_create();
    for (uint32_t v = 0; v < 4000; ++v) {
        if (roaring_bitmap_contains(r1, v) && !roaring_bitmap_contains(r2, v)) {
            roaring_bitmap_add(expected, v);
        }
    }
    assert_true(roaring_bitmap_equals(diff, expected));
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(diff);
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

/*
 * Tests ported from the Java (RoaringBitmap) and Go (roaring) libraries so
 * that the three implementations exercise the same scenarios.
 */

static const uint32_t issue330_values0[] = {
    1448147, 1331557, 1166809, 1404655, 1404657, 1448993, 1448994, 1555026,
    1568981, 1166795, 1578735, 1456755, 1581128, 1166754, 1357064, 1166799,
    1581142, 1166774, 1549034, 1090425, 1061936, 1581118, 1568668, 1456470,
    1396063, 1597976, 1458021, 1344102, 1428259, 1166742, 1332378, 1456750,
    1313881, 1371860, 1166770, 1513470, 1456761, 1520695, 1322567, 1456765,
    1457788, 1166816, 1432713, 1581004, 1451025, 1166729, 1587500, 1581022,
    1166707, 1489623, 1581108, 1547596, 1166727, 1345858, 1166741, 1473887,
    1581152, 1581114, 1259737, 1434713, 1456740, 1492705, 1316224, 1448997,
    1481940, 1456767, 1467183, 1576718, 1458286, 1475626, 1166785, 1428366,
    1303084, 1061926, 1388553, 1453950, 1400529, 1581133, 1166713, 1581166,
    1279364, 1322319, 1581027, 1166708, 1442325, 1510314, 1166761, 1404658,
    1062733, 1166764, 1431819, 1568982, 1322271, 1427065, 1374050, 1166721,
    1166714, 1321056, 1303185, 1329366, 1531398, 1071494, 1476413, 1373526,
    1166793, 1404659, 1525886, 1166735, 1593361, 1496990, 1166748, 1366912,
    1541272, 1166697, 1432913, 1559279, 1456736, 1315451, 1365178, 1068808,
    1166768, 1581107, 1345349, 1166792, 1316413, 1449633, 1456758, 1567180,
    1448998, 1423954, 1458607, 1480406, 1493217, 1469065, 1581164, 1581024,
    1486803, 1550949, 1166803, 1166783, 1072253, 1499822, 1166724, 1559280,
    1166732, 1488319, 1166796, 1062666, 1581165, 1507483, 1544041, 1483167,
    1315400, 1166762, 1404660, 1581156, 1166786, 1499824, 1340819, 1166775,
    1166710, 1473917, 1525946, 1166722, 1457787, 1166752, 1581158, 1500566,
    1166736, 1581005, 1525788, 1166700, 1166720, 1341004, 1356523, 1259716,
    1166815, 1166734, 1456734, 1393359, 1315351, 1166808, 1589134, 1166749,
    1405717, 1386258, 1166733, 1166738, 1316508, 1510168, 1434714, 1341261,
    1499823, 1166728, 1089511, 1166790, 1564869, 1316551, 1356413, 1401448,
    1166703, 1456762, 1473571, 1405729, 1340780, 1263511, 1464349, 1166810,
    1314421, 1519020, 1581006, 1514759, 1468171, 1320091, 1522288, 1456753,
    1568393, 1581110, 1530461, 1456744, 1166750, 1518612, 1448325, 1314799,
    1166696, 1166813, 1316622, 1489636, 1456741, 1166699, 1529489, 1481939,
    1347707, 1448995, 1320330, 1466667, 1166739, 1166787, 1581009, 1323304,
    1428380, 1456752, 1450469, 1496415, 1438461, 1450731, 1529496, 1166731,
    1581141, 1581103, 1166706, 1064607, 1587650, 1474681, 1064608, 1166756,
    1581015, 1573170, 1166801, 1581130, 1549033, 1166694, 1166800, 1593359,
    1581155, 1540878, 1599952, 1538583, 1060736, 1166771, 1166711, 1166791,
    1475206, 1166755, 1166798, 1455255, 1456751, 1464345, 1370294, 1401160,
    1530295, 1573169, 1166804, 1061935, 1090681, 1349316, 1447675, 1449634,
    1558271, 1581113, 1587248, 1540108, 1460873, 1562278, 1166788, 1530457,
    1493594, 1456754, 1166753, 1320442, 1581153, 1166758, 1166737, 1322566,
    1588195, 1166746, 1166777, 1492640, 1322682, 1166698, 1528367, 1445599,
    1581026, 1166751, 1445781, 1319119, 1354380, 1581143, 1460447, 1166797,
    1500720, 1369579, 1166806, 1581147, 1464393, 1492632, 1166780, 1448996,
    1166712, 1593362, 1581151, 1166807, 1478408, 1322672, 1581137, 1581002,
    1456764, 1405048, 1166766, 1063491, 1166811, 1296758, 1384972, 1314177,
    1166730, 1066109, 1514764, 1567450, 1581119, 1573634, 1415662, 1563521,
    1059518, 1456738, 1581157, 1550678, 1166726, 1166719, 1581013, 1495998,
    1371312, 1584782, 1456471, 1166778, 1458759, 1060733, 1166784, 1581028,
    1166781, 1583260, 1522601, 1491470, 1166717, 1319114, 1541228, 1456756,
    1448992, 1166744, 1549031, 1570164, 1489634, 1370005, 1475624, 1514141,
    1475625, 1525883, 1166817, 1166759, 1488332, 1313675, 1259957, 1581011,
    1166725, 1483313, 1417102, 1166740, 1260680, 1166779, 1581134, 1315627,
    1430811, 1525947, 1166763, 1435903, 1389401, 1351767, 1343593, 1537920,
    1480377, 1520699, 1319223, 1581148, 1589060, 1060735, 1468871, 1087817,
    1259068, 1581146, 1456759, 1404656, 1166776, 1166695, 1324844, 1589473,
    1374167, 1166769, 1166789, 1456871, 1477673, 1166805, 1581106, 1379727,
    1166723, 1166767, 1166745, 1313278, 1581124, 1451652, 1405710, 1408256,
    1360274, 1325001, 1581136, 1166718, 1072246, 1410198, 1456746, 1557149,
    1166760, 1308054, 1581159, 1581131, 1467536, 1166757, 1434322, 1541831,
    1418436, 1574603, 1437326, 1166814, 1584856, 1388750, 1166743, 1372695,
    1593360, 1166765, 1166794, 1474005, 1166782, 1456748, 1539639, 1056239,
    1166802, 1166747, 1210989};

static const uint32_t issue330_values1[] = {
    1332290, 1447737, 1549291, 1187244, 1598129, 1579851, 1424171, 1538999,
    1445358, 1586739, 1575050, 1437657, 1366343, 1062799, 1421550, 1460317,
    1474875, 1060737, 1330773, 1447797, 1348633, 1559437, 1556214, 1187305,
    1187234, 1187240, 1464660, 1567794, 1187260, 1260646, 1311938, 1573195,
    1318525, 1484524, 1456152, 1087954, 1556007, 1187265, 1460920, 1485316,
    1447849, 1447744, 1474001, 1537891, 1478211, 1313292, 1488405, 1187239,
    1378814, 1343620, 1500498, 1567809, 1435838, 1506575, 1368282, 1447441,
    1598101, 1067076, 1572997, 1598102, 1332697, 1324653, 1561437, 1187290,
    1059945, 1187278, 1457187, 1430003, 1450643, 1447436, 1260650, 1473393,
    1187247, 1087323, 1324967, 1187291, 1480771, 1472729, 1555881, 1187253,
    1456481, 1452672, 1447435, 1378603, 1574771, 1187235, 1417857, 1568706,
    1576739, 1534662, 1410189, 1587745, 1473791, 1187308, 1447730, 1328158,
    1409164, 1506591, 1500147, 1433961, 1483709, 1187227, 1456479, 1562595,
    1314333, 1187281, 1598012, 1415200, 1447791, 1371379, 1598019, 1435836,
    1457188, 1457351, 1187248, 1417111, 1187289, 1187252, 1187257, 1316665,
    1473464, 1187263, 1447732, 1520371, 1525651, 1598177, 1406947, 1465787,
    1524659, 1324213, 1418439, 1575816, 1522696, 1187286, 1497821, 1333179,
    1187282, 1335988, 1548952, 1556066, 1314993, 1187276, 1420503, 1187301,
    1456468, 1423939, 1598089, 1504357, 1343247, 1437659, 1525768, 1539279,
    1307385, 1187275, 1524305, 1332938, 1516498, 1303247, 1304237, 1187238,
    1385283, 1595495, 1187300, 1187241, 1061740, 1316383, 1187307, 1062037,
    1538693, 1454292, 1447731, 1187272, 1561442, 1187268, 1567150, 1597966,
    1447745, 1598178, 1187262, 1464067, 1325394, 1537893, 1332693, 1479200,
    1522335, 1589378, 1450573, 1399161, 1421274, 1561501, 1187232, 1187302,
    1258469, 1331600, 1447740, 1187242, 1328147, 1264069, 1187294, 1299943,
    1598013, 1526975, 1260604, 1487518, 1187229, 1487617, 1354087, 1456595,
    1462047, 1561438, 1598363, 1332691, 1424655, 1567105, 1574774, 1598035,
    1526981, 1384038, 1475987, 1343587, 1447437, 1454912, 1382215, 1447739,
    1456512, 1447779, 1187283, 1440988, 1187293, 1187298, 1574754, 1354772,
    1598018, 1447429, 1598181, 1447738, 1598273, 1312197, 1574752, 1572995,
    1526127, 1473908, 1437660, 1447743, 1362262, 1456513, 1539280, 1348625,
    1415878, 1332694, 1471020, 1432462, 1058088, 1526710, 1371788, 1187288,
    1537984, 1316874, 1187270, 1333565, 1187292, 1447796, 1187311, 1187237,
    1187231, 1574755, 1553822, 1522019, 1447418, 1187269, 1332692, 1447735,
    1529638, 1468154, 1328031, 1447733, 1447402, 1593884, 1332696, 1560622,
    1564819, 1538967, 1315756, 1328338, 1598113, 1324212, 1449895, 1567793,
    1260629, 1430010, 1187266, 1187256, 1312754, 1449417, 1595494, 1529054,
    1187261, 1187306, 1526976, 1425490, 1366922, 1527390, 1187299, 1561510,
    1319222, 1187250, 1057262, 1457999, 1332937, 1187243, 1556213, 1278602,
    1546839, 1187296, 1548950, 1580141, 1187303, 1187255, 1525650, 1572998,
    1576740, 1187267, 1464664, 1440427, 1456467, 1187271, 1187258, 1585428,
    1548760, 1342254, 1447793, 1406348, 1500177, 1260644, 1416954, 1323722,
    1412713, 1187280, 1187310, 1538015, 1537285, 1187285, 1456482, 1260611,
    1490508, 1187274, 1585641, 1416648, 1484655, 1421520, 1347485, 1525652,
    1568987, 1526974, 1314375, 1187246, 1455623, 1488117, 1445025, 1447401,
    1478237, 1561440, 1187287, 1561434, 1509337, 1451859, 1599630, 1348639,
    1449436, 1361844, 1464661, 1263064, 1526973, 1187279, 1562080, 1354770,
    1454521, 1520719, 1478236, 1526972, 1423948, 1334866, 1325026, 1438275,
    1422582, 1437646, 1315530, 1458323, 1447795, 1528218, 1187254, 1598056,
    1417853, 1423514, 1187297, 1187245, 1187264, 1524662, 1187251, 1524660,
    1328113, 1187304, 1374767, 1474057, 1187284, 1331601, 1598180, 1062814,
    1488818, 1187309, 1087494, 1063499, 1458325, 1187295, 1432336, 1260001,
    1597982, 1537147, 1445355, 1595491, 1396111, 1546848, 1474048, 1495251,
    1447734, 1464071, 1526978, 1187236, 1526977, 1566267, 1187277, 1421549,
    1430015, 1316024, 1332695, 1561433, 1435837, 1087250, 1574753, 1476183,
    1325395, 1561432, 1447736, 1500181, 1424164, 1456483, 1187228, 1573384,
    1273769, 1598085, 1437661, 1306415, 1407257, 1187249, 1338215, 1458047,
    1520791, 1447741, 1537263, 1472490, 1524661, 1061729, 1187273, 1417861,
    1470196, 1485881, 1260595, 1538846, 1568762, 1315170, 1500469, 1372455,
    1558140, 1425202, 1432702, 1472734, 1187230, 1187312, 1598191, 1569680,
    1187233, 1263091, 1447417, 1504429, 1430016, 1435839, 1458324, 1546845,
    1575027, 1187259, 1464610};

static const uint32_t issue330_values2[] = {
    1578810, 1166701, 1335901, 1063397, 1578812, 1526471, 1166702, 1067008,
    1412862, 1059750, 1060729, 1166812, 1493768, 1335772, 1336194, 1166772,
    1465775, 1166704, 1355314, 1314138, 1060727, 1388260, 1465786, 1565378,
    1522096, 1312490, 1319284, 1418573, 1319169, 1060730, 1307995, 1465780,
    1060728, 1404061, 1407842, 1326256, 1578754, 1410577, 1060732, 1461338,
    1264209, 1166705, 1581273, 1414196, 1565398, 1355106, 1060731, 1412641,
    1306618, 1060726, 1356302, 1310896, 1457885, 1497721, 1166709, 1067009,
    1310812, 1578800, 1422203, 1484409, 1485278, 1322057, 1369956, 1311089,
    1576614, 1355711, 1355798, 1564832, 1304166, 1166773, 1319071, 1578805,
    1575869, 1403066};

static const uint32_t issue178_values0[] = {
    3585028,   65901253,  143441994, 211160474, 286511937,
    356744840, 434332509, 502812785, 576097614, 646557334,
    714794241, 775083485, 833704249, 889329147, 941367043};

static const uint32_t issue178_values1[] = {
    17883,     54494426,  113908938, 174519827, 235465665, 296685741,
    357644666, 420192495, 476104304, 523046142, 577855081, 634889665,
    692460635, 751350463, 809989192, 863494316, 919127240};

static const uint32_t karpenske_values[] = {
    343798, 343799, 343800, 343801, 343803, 343804, 343805, 343807, 343809,
    343811, 343812, 343815, 343816, 343817, 343818, 343819, 343821, 343825,
    343827, 343828, 343830, 343831, 343832, 343833, 343835, 343836, 343837,
    343838, 343839, 343840, 343841, 343842, 343843, 343844, 343845, 343847,
    343848, 343849, 343850, 343851, 343853, 343854, 343855, 343856, 343858,
    343859, 343860, 343861, 343862, 343863, 343864, 343865, 343866, 343868,
    343869, 343874, 343875, 343877, 343879, 343880, 343881, 343882, 343883,
    343887, 343889, 343890, 343891, 343894, 343895, 343898, 343899, 343900,
    343901, 343902, 343904, 343906, 343907, 343908, 343909, 343910, 343911,
    343912, 343913, 343914, 343915, 343916, 343917, 343918, 343919, 343921,
    343922, 343923, 343924, 343927, 343928, 343929, 343930, 343931, 343932,
    343933, 343934, 343935, 343938, 343939, 343941, 343942, 343943, 343944,
    343945, 343946, 343949, 343951, 343953, 343954, 343955, 343956, 343958,
    343959, 343961, 343962, 343964, 343965, 343966, 343967, 343968, 343969,
    343971, 343972, 343973, 343974, 343976, 343978, 343979, 343981, 343982,
    343983, 343985, 343987, 343988, 343989, 343992, 343993, 343994, 343995,
    343996, 343997, 343998, 344000, 344001, 344002, 344003, 344004, 344006,
    344008, 344009, 344011, 344012, 344013, 344015, 344017, 344019, 344020,
    344021, 344023, 344025, 344026, 344027, 344028, 344029, 344030, 344031,
    344034, 344035, 344036, 344037, 344038, 344039, 344040, 344042, 344043,
    344046, 344047};

static const uint32_t issue370_a[] = {
    239, 240, 241, 242, 243, 244, 259, 260, 261, 262, 263, 264, 265, 266,
    267, 268, 269, 270, 273, 274, 275, 276, 277, 278, 398, 399, 400, 401,
    402, 403, 404, 405, 406, 408, 409, 410, 411, 412, 413, 420, 421, 422,
    509, 510, 511, 512, 513, 514, 539, 540, 541, 542, 543, 544, 547, 548,
    549, 550, 551, 552, 553, 554, 555, 556, 557, 558, 578, 579, 580, 581,
    582, 583, 584, 585, 586, 587, 588, 589, 590, 591, 592, 593, 594, 595,
    624, 625, 634, 635, 636, 649, 650, 651, 652, 653, 654, 714, 715, 716,
    718, 719, 720, 721, 722, 723, 724, 725, 726, 728, 729, 730, 731, 732,
    733, 734, 735, 736, 739, 740, 741, 742, 743, 744, 771, 772, 773};

static const uint32_t issue370_b[] = {
    239, 240, 241, 242, 243, 244, 259, 260, 261, 262, 263, 264, 265, 266, 267,
    268, 269, 270, 273, 274, 275, 276, 277, 278, 398, 399, 400, 401, 402, 403,
    404, 405, 406, 408, 409, 410, 411, 412, 413, 420, 421, 422, 509, 510, 511,
    512, 513, 514, 539, 540, 541, 542, 543, 544, 547, 548, 549, 550, 551, 552,
    553, 554, 555, 556, 557, 558, 578, 579, 580, 581, 582, 583, 584, 585, 586,
    607, 608, 634, 635, 636, 649, 650, 651, 652, 653, 654, 714, 715, 716, 718,
    719, 720, 721, 722, 723, 724, 725, 726, 728, 729, 730, 731, 732, 733, 734,
    735, 736, 739, 740, 741, 742, 743, 744, 771, 772, 773};

static const uint32_t issue467_large_ops[][3] = {
    {0, 0, 16385},       {0, 16385, 20482},   {0, 20482, 27862},
    {1, 0, 16385},       {1, 16385, 20482},   {1, 20482, 27862},
    {0, 27862, 44247},   {0, 44247, 45576},   {0, 45576, 61961},
    {0, 61961, 66058},   {0, 66058, 67247},   {1, 27862, 44247},
    {1, 45576, 61961},   {1, 44247, 45576},   {1, 61961, 66058},
    {1, 66058, 67247},   {0, 67247, 68819},   {0, 68819, 73028},
    {0, 73028, 89413},   {0, 89413, 92266},   {0, 92266, 108651},
    {0, 108651, 113772}, {0, 113772, 118757}, {1, 68819, 73028},
    {1, 73028, 89413},   {1, 92266, 108651},  {1, 89413, 92266},
    {1, 108651, 113772}, {1, 113772, 118757}, {0, 118757, 132098},
    {1, 118757, 132098}, {0, 132098, 137544}, {1, 132098, 137544},
    {0, 137544, 153929}, {0, 153929, 155151}, {0, 155151, 162078},
    {0, 162078, 167119}, {0, 167119, 181012}, {0, 181012, 197397},
    {0, 197397, 201244}, {0, 201244, 217629}, {0, 217629, 222750},
    {0, 222750, 227708}, {0, 227708, 235777}, {0, 235777, 252162},
    {0, 252162, 256259}, {1, 252162, 256259}, {1, 227708, 235777},
    {1, 235777, 252162}};

static roaring_uint32_range_closed_t closed_range(uint32_t min, uint32_t max) {
    roaring_uint32_range_closed_t r;
    r.min = min;
    r.max = max;
    return r;
}

#define UNIVERSE_END (UINT64_C(1) << 32)

// Java intersects(long, long) tests, Go TestIntersectsWithInterval
DEFINE_TEST(test_intersect_with_range_edges) {
    roaring_bitmap_t *r = roaring_bitmap_from(5);
    assert_true(roaring_bitmap_intersect_with_range(r, 5, UINT64_MAX));
    assert_false(roaring_bitmap_intersect_with_range(r, 5, 5));
    roaring_bitmap_free(r);

    r = roaring_bitmap_from(UINT32_MAX);
    assert_true(
        roaring_bitmap_intersect_with_range(r, UINT32_MAX, UNIVERSE_END));
    assert_false(
        roaring_bitmap_intersect_with_range(r, UINT32_MAX - 1, UINT32_MAX));
    assert_true(roaring_bitmap_intersect_with_range(r, 0, UNIVERSE_END));
    assert_true(roaring_bitmap_intersect_with_range(r, 0, UINT64_MAX));
    roaring_bitmap_free(r);

    r = roaring_bitmap_from_range(21, 26, 1);
    assert_false(roaring_bitmap_intersect_with_range(r, 22, 22));
    assert_false(roaring_bitmap_intersect_with_range(r, 27, 27));
    assert_true(roaring_bitmap_intersect_with_range(r, 22, 23));
    assert_true(roaring_bitmap_intersect_with_range(r, 19, 23));
    assert_true(roaring_bitmap_intersect_with_range(r, 23, 30));
    assert_true(roaring_bitmap_intersect_with_range(r, 19, 30));
    assert_false(roaring_bitmap_intersect_with_range(r, 19, 20));
    assert_false(roaring_bitmap_intersect_with_range(r, 19, 21));
    assert_true(roaring_bitmap_intersect_with_range(r, 19, 22));
    assert_false(roaring_bitmap_intersect_with_range(r, 26, 30));
    assert_false(roaring_bitmap_intersect_with_range(r, 28, 30));
    roaring_bitmap_add_range(r, 30, 40);
    assert_false(roaring_bitmap_intersect_with_range(r, 28, 29));
    assert_true(roaring_bitmap_intersect_with_range(r, 28, 31));
    roaring_bitmap_free(r);

    // bitset container (key 0) with values only in the lower half
    r = roaring_bitmap_create();
    for (uint32_t v = 1; v < 32768; v += 2) roaring_bitmap_add(r, v);
    convert_all_containers(r, BITSET_CONTAINER_TYPE);
    assert_false(roaring_bitmap_intersect_with_range(r, 32768, 65535));
    assert_false(roaring_bitmap_intersect_with_range(r, 32768, 65536));
    assert_true(roaring_bitmap_intersect_with_range(r, 32767, 32769));
    roaring_bitmap_free(r);

    // bitset containers at key 0x8000 (values >= 2^31)
    const uint64_t words[] = {UINT64_C(1789303257167203747),
                              UINT64_C(2697219678014362575)};
    for (size_t w = 0; w < 2; w++) {
        r = roaring_bitmap_create();
        for (uint32_t i = 0; i < 65536; i++) {
            if ((words[w] >> (i % 64)) & 1) {
                roaring_bitmap_add(r, UINT32_C(0x80000000) + i);
            }
        }
        convert_all_containers(r, BITSET_CONTAINER_TYPE);
        uint64_t first = roaring_bitmap_minimum(r);
        uint64_t last = roaring_bitmap_maximum(r);
        assert_false(roaring_bitmap_intersect_with_range(r, 0, first));
        assert_false(roaring_bitmap_intersect_with_range(r, first - 1, first));
        assert_true(roaring_bitmap_intersect_with_range(r, first, first + 1));
        assert_true(
            roaring_bitmap_intersect_with_range(r, first - 1, first + 1));
        assert_true(roaring_bitmap_intersect_with_range(r, last, last + 1));
        assert_true(roaring_bitmap_intersect_with_range(r, last - 1, last + 1));
        assert_false(
            roaring_bitmap_intersect_with_range(r, last + 1, UNIVERSE_END));
        roaring_bitmap_free(r);
    }
}

static roaring_bitmap_t *mixed_containers_bitmap(
    const uint32_t *array_keys, size_t n_array, const uint32_t *bitset_keys,
    size_t n_bitset, const uint32_t *run_keys, size_t n_run) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    for (size_t k = 0; k < n_array; k++) {
        for (uint32_t i = 0; i < 1000; i += 17) {
            roaring_bitmap_add(r, (array_keys[k] << 16) + i);
        }
    }
    for (size_t k = 0; k < n_bitset; k++) {
        for (uint32_t i = 0; i < 65536; i += 2) {
            roaring_bitmap_add(r, (bitset_keys[k] << 16) + i);
        }
    }
    for (size_t k = 0; k < n_run; k++) {
        roaring_bitmap_add_range(r, (uint64_t)run_keys[k] << 16,
                                 ((uint64_t)run_keys[k] << 16) + 50000);
    }
    roaring_bitmap_run_optimize(r);
    return r;
}

// Java RoaringBitmapIntervalIntersectionTest: intersect_with_range agrees with
// the intersection with the materialized range (here: its cardinality).
DEFINE_TEST(test_intersect_with_range_matches_cardinality) {
    const uint32_t k0[] = {0}, k1[] = {1}, k2[] = {2}, k10[] = {10},
                   k20[] = {20}, k30[] = {30}, k012[] = {0, 1, 2},
                   k200[] = {200}, k205[] = {205};
    roaring_bitmap_t *bitmaps[10];
    size_t n = 0;
    bitmaps[n++] = roaring_bitmap_from(1, 2, 3);
    bitmaps[n++] = roaring_bitmap_from(UINT32_C(0xC0000000));
    bitmaps[n++] = roaring_bitmap_from(1, UINT32_C(0xC0000000));
    bitmaps[n++] = roaring_bitmap_from(1, 65536, UINT32_C(0xC0000000));
    bitmaps[n++] = mixed_containers_bitmap(k10, 1, k20, 1, k30, 1);
    bitmaps[n++] = mixed_containers_bitmap(k0, 1, k1, 1, k20, 1);
    bitmaps[n++] = mixed_containers_bitmap(k1, 1, k20, 1, k0, 1);
    bitmaps[n++] = mixed_containers_bitmap(k20, 1, k0, 1, k1, 1);
    bitmaps[n++] = mixed_containers_bitmap(k012, 3, k200, 1, k205, 1);
    bitmaps[n++] = mixed_containers_bitmap(k2, 1, k0, 1, k1, 1);
    roaring_bitmap_add_range(bitmaps[4], 70000, 150000);
    const uint64_t points[] = {0,
                               1,
                               2,
                               3,
                               4,
                               256,
                               999,
                               1000,
                               65535,
                               65536,
                               65537,
                               67000,
                               70000,
                               71000,
                               140000,
                               150000,
                               (UINT64_C(10) << 16),
                               (UINT64_C(10) << 16) + 1,
                               (UINT64_C(20) << 16),
                               (UINT64_C(20) << 16) + 1,
                               (UINT64_C(30) << 16),
                               (UINT64_C(30) << 16) + 50000,
                               (UINT64_C(199) << 16),
                               (UINT64_C(200) << 16) + (1 << 14),
                               (UINT64_C(205) << 16),
                               UINT64_C(0xC0000000),
                               UINT64_C(0xC0000001),
                               UINT32_MAX,
                               UNIVERSE_END};
    const size_t npoints = sizeof(points) / sizeof(points[0]);
    for (size_t b = 0; b < n; b++) {
        const roaring_bitmap_t *r = bitmaps[b];
        assert_bitmap_validate(r);
        for (size_t i = 0; i < npoints; i++) {
            for (size_t j = i; j < npoints; j++) {
                bool expected = roaring_bitmap_range_cardinality(r, points[i],
                                                                 points[j]) > 0;
                assert_true(roaring_bitmap_intersect_with_range(
                                r, points[i], points[j]) == expected);
            }
        }
        uint64_t first = roaring_bitmap_minimum(r);
        uint64_t last = roaring_bitmap_maximum(r);
        assert_true(roaring_bitmap_intersect_with_range(
            r, first > 0 ? first - 1 : 0, first + 1));
        assert_true(roaring_bitmap_intersect_with_range(r, last - 1, last + 1));
        assert_false(roaring_bitmap_intersect_with_range(r, 0, first));
        assert_false(
            roaring_bitmap_intersect_with_range(r, last + 1, UNIVERSE_END));
        roaring_bitmap_free(bitmaps[b]);
    }
}

// Java testContainsRange_AgreesWithRangeCardinality / _EndOfUniverse /
// _SmallBitmap and issue623
DEFINE_TEST(test_contains_range_agrees_with_range_cardinality) {
    const uint64_t points[] = {0,          1,          0xFFFF,      0x10000,
                               0x10001,    0x1FFFF,    0x20000,     0x30000,
                               0x30001,    0xFFFEFFFF, 0xFFFF0000,  0xFFFF0001,
                               0xFFFFFFFE, 0xFFFFFFFF, UNIVERSE_END};
    const size_t npoints = sizeof(points) / sizeof(points[0]);
    roaring_bitmap_t *bitmaps[6];
    bitmaps[0] = roaring_bitmap_from_range(0, 65536, 1);
    bitmaps[1] = roaring_bitmap_from_range(0, 65536, 1);
    roaring_bitmap_add(bitmaps[1], 3 << 16);
    bitmaps[2] = roaring_bitmap_from_range(0, 131072, 1);
    roaring_bitmap_add_range(bitmaps[2], 0xFFFF0000, UNIVERSE_END);
    bitmaps[3] = roaring_bitmap_from_range(0xFFFE0000, UNIVERSE_END, 1);
    bitmaps[4] =
        roaring_bitmap_from(0, 1 << 16, 3 << 16, 0xFFFFFFFE, 0xFFFFFFFF);
    bitmaps[5] = roaring_bitmap_from_range(0, UNIVERSE_END, 1);
    for (size_t b = 0; b < 6; b++) {
        for (size_t i = 0; i < npoints; i++) {
            for (size_t j = i + 1; j < npoints; j++) {
                uint64_t card = roaring_bitmap_range_cardinality(
                    bitmaps[b], points[i], points[j]);
                bool expected = card == points[j] - points[i];
                assert_true(roaring_bitmap_contains_range(
                                bitmaps[b], points[i], points[j]) == expected);
            }
        }
        roaring_bitmap_free(bitmaps[b]);
    }

    roaring_bitmap_t *r = roaring_bitmap_from(UINT32_MAX);
    assert_true(roaring_bitmap_contains_range(r, UINT32_MAX, UNIVERSE_END));
    assert_false(
        roaring_bitmap_contains_range(r, UINT32_MAX - 1, UNIVERSE_END));
    roaring_bitmap_free(r);
    r = roaring_bitmap_from_range(0xFFFF0000, UNIVERSE_END, 1);
    assert_true(roaring_bitmap_contains_range(r, 0xFFFF8000, UNIVERSE_END));
    roaring_bitmap_free(r);

    r = roaring_bitmap_from_range(1, 7, 1);
    assert_true(roaring_bitmap_contains_range(r, 1, 7));
    assert_false(roaring_bitmap_contains_range(r, 2, 8));
    assert_false(roaring_bitmap_contains_range(r, 0, 1));
    assert_false(roaring_bitmap_contains_range(r, 1 << 17, 1 << 19));
    roaring_bitmap_free(r);

    // issue 623
    r = roaring_bitmap_create();
    roaring_bitmap_add(r, 65535);
    roaring_bitmap_add(r, 65536);
    assert_true(roaring_bitmap_contains_range(r, 65535, 65537));
    for (uint64_t i = 1; i <= 100000; i++) {
        roaring_bitmap_add_range(r, i, i + 1);
    }
    for (uint64_t i = 1; i <= 100000; i++) {
        assert_true(roaring_bitmap_contains_range(r, i, i + 1));
    }
    assert_true(roaring_bitmap_contains_range(r, 1, 100001));
    assert_false(roaring_bitmap_contains_range(r, 0, 100001));
    roaring_bitmap_free(r);
}

// Go TestCardinalityInRange*, Java rangeCardinalityFullContainer and
// testRangeCardinalityAtBoundary (issue 285)
DEFINE_TEST(test_range_cardinality_layouts) {
    roaring_bitmap_t *r = roaring_bitmap_from_range(0, 300, 3);
    assert_true(roaring_bitmap_range_cardinality(r, 0, 300) == 100);
    assert_true(roaring_bitmap_range_cardinality(r, 0, 1) == 1);
    assert_true(roaring_bitmap_range_cardinality(r, 0, 4) == 2);
    assert_true(roaring_bitmap_range_cardinality(r, 3, 7) == 2);
    roaring_bitmap_free(r);

    r = roaring_bitmap_create();
    for (uint32_t v = 0; v < 16384; v += 2) roaring_bitmap_add(r, v);
    assert_true(r->high_low_container.typecodes[0] == BITSET_CONTAINER_TYPE);
    assert_true(roaring_bitmap_range_cardinality(r, 0, 16384) == 8192);
    assert_true(roaring_bitmap_range_cardinality(r, 1, 2) == 0);
    assert_true(roaring_bitmap_range_cardinality(r, 1, 3) == 1);
    assert_true(roaring_bitmap_range_cardinality(r, 100, 200) == 50);
    roaring_bitmap_free(r);

    r = roaring_bitmap_create();
    const uint32_t keys[] = {0, 1, 3};
    for (size_t k = 0; k < 3; k++) {
        for (uint32_t i = 0; i < 100; i++) {
            roaring_bitmap_add(r, (keys[k] << 16) + i);
        }
    }
    assert_true(roaring_bitmap_range_cardinality(r, 50, 196658) == 200);
    assert_true(roaring_bitmap_range_cardinality(r, 131072, 196608) == 0);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from_range(0, 1024, 1);
    roaring_bitmap_add_range(r, 2048, 3072);
    roaring_bitmap_run_optimize(r);
    assert_true(roaring_bitmap_range_cardinality(r, 1023, 1025) == 1);
    assert_true(roaring_bitmap_range_cardinality(r, 512, 2560) == 1024);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from(0, UINT32_C(0x80000000), UINT32_MAX);
    assert_true(roaring_bitmap_range_cardinality(r, 0, UNIVERSE_END) == 3);
    assert_true(roaring_bitmap_range_cardinality(r, UINT32_MAX, UNIVERSE_END) ==
                1);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from(66236);
    assert_true(roaring_bitmap_range_cardinality(r, 60000, 70000) == 1);
    roaring_bitmap_free(r);

    // whole containers of every type, and agreement with rank
    r = roaring_bitmap_from_range(0, 65536, 37);  // array
    roaring_bitmap_t *tmp = roaring_bitmap_from_range(65536, 131072, 3);
    roaring_bitmap_or_inplace(r, tmp);  // bitset
    roaring_bitmap_free(tmp);
    roaring_bitmap_add_range(r, 131072, 196608);            // full run
    roaring_bitmap_add_range(r, 0xFFFF0000, UNIVERSE_END);  // full run
    roaring_bitmap_run_optimize(r);
    for (int i = 0; i < r->high_low_container.size; i++) {
        uint64_t key = r->high_low_container.keys[i];
        uint64_t card =
            container_get_cardinality(r->high_low_container.containers[i],
                                      r->high_low_container.typecodes[i]);
        assert_true(roaring_bitmap_range_cardinality(r, key << 16,
                                                     (key + 1) << 16) == card);
    }
    const uint64_t probes[] = {
        0,      1,       36,         37,         65535,       65536,
        65537,  100000,  131071,     131072,     150000,      196607,
        196608, 1 << 20, 0xFFFF0000, 0xFFFFFFFF, UNIVERSE_END};
    const size_t nprobes = sizeof(probes) / sizeof(probes[0]);
    for (size_t i = 0; i < nprobes; i++) {
        for (size_t j = i; j < nprobes; j++) {
            uint64_t s = probes[i], e = probes[j];
            uint64_t expected = 0;
            if (e > s) {
                expected =
                    roaring_bitmap_rank(r, (uint32_t)(e - 1)) -
                    (s > 0 ? roaring_bitmap_rank(r, (uint32_t)(s - 1)) : 0);
            }
            assert_true(roaring_bitmap_range_cardinality(r, s, e) == expected);
        }
    }
    roaring_bitmap_free(r);
}

// Go TestRunOptimizeRecoversSpaceAfterRemoveRange (32-bit version of
// cpp_unit's test_cpp_run_optimize_recovers_space_64)
DEFINE_TEST(test_run_optimize_recovers_space) {
    roaring_bitmap_t *r = roaring_bitmap_from_range(0, 65536, 1);
    for (uint32_t i = 0; i < 65536; i += 2) {
        roaring_bitmap_remove_range(r, i, i + 1);
    }
    roaring_bitmap_t *expected = roaring_bitmap_from_range(1, 65536, 2);
    size_t before = roaring_bitmap_portable_size_in_bytes(r);
    // CRoaring may already convert the container while removing; if it is
    // still a degenerate run container, run_optimize must shrink it.
    bool was_run = r->high_low_container.typecodes[0] == RUN_CONTAINER_TYPE;
    assert_false(roaring_bitmap_run_optimize(r));
    size_t after = roaring_bitmap_portable_size_in_bytes(r);
    assert_true(after <= before);
    if (was_run) assert_true(after < before / 10);
    assert_true(roaring_bitmap_equals(r, expected));
    assert_bitmap_validate(r);
    assert_true(r->high_low_container.typecodes[0] != RUN_CONTAINER_TYPE);
    roaring_bitmap_free(r);
    roaring_bitmap_free(expected);

    r = roaring_bitmap_from_range(0, 100, 1);
    assert_true(roaring_bitmap_run_optimize(r));
    for (uint32_t i = 200; i < 65536; i += 2) roaring_bitmap_add(r, i);
    expected = roaring_bitmap_copy(r);
    before = roaring_bitmap_portable_size_in_bytes(r);
    assert_false(roaring_bitmap_run_optimize(r));
    after = roaring_bitmap_portable_size_in_bytes(r);
    assert_true(after <= before);
    assert_true(roaring_bitmap_equals(r, expected));
    assert_true(roaring_bitmap_equals(expected, r));
    assert_bitmap_validate(r);
    roaring_bitmap_free(r);
    roaring_bitmap_free(expected);
}

// Go TestIssue467CaseSmall / TestIssue467CaseLarge
DEFINE_TEST(test_issue467) {
    static const uint32_t small_ranges[][2] = {
        {0, 16385},      {16385, 20482},  {20482, 27862},   {27862, 44247},
        {45576, 61961},  {61961, 66058},  {66058, 67247},   {68819, 73028},
        {73028, 89413},  {92266, 108651}, {108651, 113772}, {113772, 118757},
        {118757, 132098}};
    roaring_bitmap_t *r = roaring_bitmap_create();
    uint64_t total = 0;
    for (size_t i = 0; i < sizeof(small_ranges) / sizeof(small_ranges[0]);
         i++) {
        roaring_bitmap_add_range(r, small_ranges[i][0], small_ranges[i][1]);
        total += small_ranges[i][1] - small_ranges[i][0];
    }
    roaring_bitmap_run_optimize(r);
    assert_bitmap_validate(r);
    assert_true(roaring_bitmap_get_cardinality(r) == total);
    roaring_bitmap_free(r);

    r = roaring_bitmap_create();
    roaring_bitmap_t *expected = roaring_bitmap_create();
    for (size_t i = 0;
         i < sizeof(issue467_large_ops) / sizeof(issue467_large_ops[0]); i++) {
        const uint32_t *op = issue467_large_ops[i];
        if (op[0]) {
            roaring_bitmap_add_range(r, op[1], op[2]);
            for (uint32_t v = op[1]; v < op[2]; v++)
                roaring_bitmap_add(expected, v);
        } else {
            roaring_bitmap_remove_range(r, op[1], op[2]);
            for (uint32_t v = op[1]; v < op[2]; v++)
                roaring_bitmap_remove(expected, v);
        }
    }
    roaring_bitmap_run_optimize(r);
    assert_bitmap_validate(r);
    assert_true(roaring_bitmap_equals(r, expected));
    roaring_bitmap_free(r);
    roaring_bitmap_free(expected);
}

// Java addOffsetMatchesShiftedValues and
// addOffsetMultipleOf65536DiscardsValuesOutOfRange
DEFINE_TEST(add_offset_key_multiples) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    const uint32_t array_keys[] = {0, 0x7FFF, 0xFFFF};
    for (size_t k = 0; k < 3; k++) {
        roaring_bitmap_add(r, (array_keys[k] << 16) | 7);
        roaring_bitmap_add(r, (array_keys[k] << 16) | 0xFFFF);
    }
    roaring_bitmap_add_range(r, 0x10000, 0x20000);
    for (uint32_t i = 0; i < 65536; i += 2) {
        roaring_bitmap_add(r, (UINT32_C(0x8000) << 16) | i);
    }
    roaring_bitmap_add_range(r, UINT64_C(0xFFFE0000) + 100,
                             UINT64_C(0xFFFE0000) + 200);
    roaring_bitmap_run_optimize(r);
    const int64_t key_offsets[] = {-0x10000, -0xFFFF, -0x8001, -0x8000, -2,
                                   -1,       0,       1,       2,       0x7FFF,
                                   0x8000,   0xFFFF,  0x10000};
    const int64_t low_offsets[] = {0, 1, 0xFFFF};
    for (size_t k = 0; k < sizeof(key_offsets) / sizeof(key_offsets[0]); k++) {
        for (size_t l = 0; l < 3; l++) {
            int64_t offset = key_offsets[k] * 65536 + low_offsets[l];
            roaring_bitmap_t *shifted = roaring_bitmap_add_offset(r, offset);
            roaring_bitmap_t *expected = offset_by_value(r, offset);
            assert_true(roaring_bitmap_equals(shifted, expected));
            assert_bitmap_validate(shifted);
            roaring_bitmap_free(shifted);
            roaring_bitmap_free(expected);
        }
    }
    roaring_bitmap_free(r);

    // a value shifted to exactly UINT32_MAX is kept
    r = roaring_bitmap_from(0xFFFEFFFF, 0xFFFFFFFE);
    roaring_bitmap_t *s = roaring_bitmap_add_offset(r, 0x10000);
    assert_true(roaring_bitmap_get_cardinality(s) == 1);
    assert_true(roaring_bitmap_contains(s, UINT32_MAX));
    roaring_bitmap_free(s);
    s = roaring_bitmap_add_offset(r, 1);
    assert_true(roaring_bitmap_get_cardinality(s) == 2);
    assert_true(roaring_bitmap_contains(s, UINT32_MAX));
    roaring_bitmap_free(s);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from(UINT32_MAX);
    s = roaring_bitmap_add_offset(r, 0x10000);
    assert_true(roaring_bitmap_is_empty(s));
    roaring_bitmap_free(s);
    roaring_bitmap_add(r, 5);
    s = roaring_bitmap_add_offset(r, 0x10000);
    assert_true(roaring_bitmap_get_cardinality(s) == 1);
    assert_true(roaring_bitmap_contains(s, 0x10005));
    roaring_bitmap_free(s);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from(5);
    s = roaring_bitmap_add_offset(r, -0x10000);
    assert_true(roaring_bitmap_is_empty(s));
    roaring_bitmap_free(s);
    roaring_bitmap_add(r, 70000);
    s = roaring_bitmap_add_offset(r, -0x10000);
    assert_true(roaring_bitmap_get_cardinality(s) == 1);
    assert_true(roaring_bitmap_contains(s, 70000 - 65536));
    roaring_bitmap_free(s);
    roaring_bitmap_free(r);
}

// Java testCheckedAdd / testCheckedRemove / CheckedRemoveTest
DEFINE_TEST(test_checked_add_remove_transitions) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    for (uint32_t i = 0; i < 131072; i += 2) {
        assert_true(roaring_bitmap_add_checked(r, i));
    }
    for (uint32_t i = 1; i < 131072; i += 2) {
        assert_true(roaring_bitmap_add_checked(r, i));
    }
    assert_true(roaring_bitmap_add_checked(r, 0xFFFFFFF7));
    assert_true(roaring_bitmap_add_checked(r, 0xFFFFFFFF));
    for (uint32_t i = 0; i < 131072; i++) {
        assert_false(roaring_bitmap_add_checked(r, i));
    }
    assert_false(roaring_bitmap_add_checked(r, 0xFFFFFFF7));
    assert_false(roaring_bitmap_add_checked(r, 0xFFFFFFFF));
    assert_bitmap_validate(r);
    for (uint32_t i = 0; i < 131072; i += 2) {
        assert_true(roaring_bitmap_remove_checked(r, i));
        assert_false(roaring_bitmap_remove_checked(r, i));
    }
    assert_bitmap_validate(r);
    for (uint32_t i = 1; i < 131072; i += 2) {
        assert_true(roaring_bitmap_remove_checked(r, i));
    }
    assert_true(roaring_bitmap_remove_checked(r, 0xFFFFFFF7));
    assert_true(roaring_bitmap_remove_checked(r, 0xFFFFFFFF));
    assert_false(roaring_bitmap_remove_checked(r, 0xFFFFFFFF));
    assert_true(roaring_bitmap_is_empty(r));
    assert_bitmap_validate(r);

    // bitset -> array transition while removing
    for (uint32_t i = 0; i < 20000; i += 2) roaring_bitmap_add(r, i);
    assert_true(r->high_low_container.typecodes[0] == BITSET_CONTAINER_TYPE);
    for (uint32_t i = 0; i < 20000; i += 2) {
        assert_true(roaring_bitmap_remove_checked(r, i));
        assert_bitmap_validate(r);
    }
    assert_true(roaring_bitmap_is_empty(r));
    roaring_bitmap_free(r);
}

// Java fliptest_Karpenske
DEFINE_TEST(test_flip_karpenske) {
    const size_t n = sizeof(karpenske_values) / sizeof(karpenske_values[0]);
    roaring_bitmap_t *r = roaring_bitmap_create();
    size_t start = 0;
    for (size_t end = 1; end <= n; end++) {
        if (end == n ||
            karpenske_values[end - 1] + 1 != karpenske_values[end]) {
            if (start == end - 1) {
                roaring_bitmap_add(r, karpenske_values[start]);
            } else {
                roaring_bitmap_flip_inplace(r, karpenske_values[start],
                                            karpenske_values[end - 1] + 1);
            }
            start = end;
        }
    }
    roaring_bitmap_t *expected = roaring_bitmap_of_ptr(n, karpenske_values);
    assert_true(roaring_bitmap_get_cardinality(r) == n);
    assert_true(roaring_bitmap_equals(r, expected));
    assert_bitmap_validate(r);
    roaring_bitmap_free(r);
    roaring_bitmap_free(expected);
}

// Java regressionTestRemove377
DEFINE_TEST(test_remove_range_issue377) {
    roaring_bitmap_t *r = roaring_bitmap_from_range(0, 64, 1);
    roaring_bitmap_run_optimize(r);
    for (uint32_t i = 0; i < 64; i++) {
        if (i != 30 && i != 32) roaring_bitmap_remove(r, i);
    }
    roaring_bitmap_remove_range(r, 0, 31);
    assert_false(roaring_bitmap_contains(r, 30));
    assert_true(roaring_bitmap_contains(r, 32));
    assert_true(roaring_bitmap_get_cardinality(r) == 1);
    assert_bitmap_validate(r);
    roaring_bitmap_free(r);
}

// Java regressionTestEquals370
DEFINE_TEST(test_equals_issue370) {
    roaring_bitmap_t *a = roaring_bitmap_of_ptr(
        sizeof(issue370_a) / sizeof(issue370_a[0]), issue370_a);
    roaring_bitmap_t *b = roaring_bitmap_of_ptr(
        sizeof(issue370_b) / sizeof(issue370_b[0]), issue370_b);
    assert_false(roaring_bitmap_equals(a, b));
    assert_false(roaring_bitmap_equals(b, a));
    roaring_bitmap_run_optimize(a);
    assert_false(roaring_bitmap_equals(a, b));
    assert_false(roaring_bitmap_equals(b, a));
    roaring_bitmap_run_optimize(b);
    assert_false(roaring_bitmap_equals(a, b));
    assert_false(roaring_bitmap_equals(b, a));
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);
}

// 32-bit version of cpp_unit's test_cpp_remove_run_compression
DEFINE_TEST(test_remove_run_compression_top_of_universe) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    assert_false(roaring_bitmap_run_optimize(r));
    assert_false(roaring_bitmap_remove_run_compression(r));
    roaring_bitmap_add_range(r, 0xFFFFFFF5, UNIVERSE_END);
    assert_true(roaring_bitmap_run_optimize(r));
    assert_true(roaring_bitmap_remove_run_compression(r));
    assert_true(roaring_bitmap_get_cardinality(r) == 11);
    assert_true(roaring_bitmap_contains(r, UINT32_MAX));
    assert_true(roaring_bitmap_contains_range(r, 0xFFFFFFF5, UNIVERSE_END));
    assert_bitmap_validate(r);
    assert_false(roaring_bitmap_remove_run_compression(r));
    roaring_bitmap_free(r);
}

// Java testRankOverflow / testRankBigInts
DEFINE_TEST(test_rank_high_values) {
    roaring_bitmap_t *r = roaring_bitmap_from(65537);
    assert_true(roaring_bitmap_rank(r, 1) == 0);
    assert_true(roaring_bitmap_rank(r, 65537) == 1);
    assert_true(roaring_bitmap_rank(r, 65538) == 1);
    assert_true(roaring_bitmap_rank(r, UINT32_MAX) == 1);
    roaring_bitmap_free(r);

    r = roaring_bitmap_create();
    for (uint32_t k = 0; k < 100000; k += 7) {
        roaring_bitmap_add(r, UINT32_C(0x80000000) + k);
    }
    assert_true(roaring_bitmap_rank(r, 0x7FFFFFFF) == 0);
    for (uint32_t k = 0; k < 100000; k += 7) {
        assert_true(roaring_bitmap_rank(r, UINT32_C(0x80000000) + k) ==
                    1 + k / 7);
    }
    assert_true(roaring_bitmap_rank(r, UINT32_MAX) ==
                roaring_bitmap_get_cardinality(r));
    roaring_bitmap_free(r);
}

// Java testEmptyFirst / testEmptyLast / testFirstLastSigned
DEFINE_TEST(test_minimum_maximum_extremes) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    assert_true(roaring_bitmap_minimum(r) == UINT32_MAX);
    assert_true(roaring_bitmap_maximum(r) == 0);
    roaring_bitmap_add(r, 777);
    roaring_bitmap_add(r, 0xFFFFFFF9);
    assert_true(roaring_bitmap_minimum(r) == 777);
    assert_true(roaring_bitmap_maximum(r) == 0xFFFFFFF9);
    roaring_bitmap_free(r);
    r = roaring_bitmap_from(0x7FFFFFFF, 0x80000000);
    assert_true(roaring_bitmap_minimum(r) == 0x7FFFFFFF);
    assert_true(roaring_bitmap_maximum(r) == 0x80000000);
    roaring_bitmap_free(r);
    r = roaring_bitmap_from(0x80000000, UINT32_MAX);
    assert_true(roaring_bitmap_minimum(r) == 0x80000000);
    assert_true(roaring_bitmap_maximum(r) == UINT32_MAX);
    roaring_bitmap_free(r);
}

// Go TestFuzzerRepro_* (minimal run containers), built from ranges here
DEFINE_TEST(test_minimal_run_point_mutations) {
    roaring_bitmap_t *r = roaring_bitmap_from_range(2, 6, 1);
    roaring_bitmap_run_optimize(r);
    roaring_bitmap_remove(r, 3);
    assert_bitmap_validate(r);
    assert_true(roaring_bitmap_get_cardinality(r) == 3);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from_range(54, 58, 1);
    roaring_bitmap_run_optimize(r);
    roaring_bitmap_add(r, 112);
    assert_bitmap_validate(r);
    assert_true(roaring_bitmap_get_cardinality(r) == 5);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from_range(1048552, 1048556, 1);
    roaring_bitmap_run_optimize(r);
    assert_true(r->high_low_container.typecodes[0] == RUN_CONTAINER_TYPE);
    roaring_bitmap_flip_inplace(r, 1048555, 1048556);
    assert_bitmap_validate(r);
    assert_true(roaring_bitmap_get_cardinality(r) == 3);
    assert_false(roaring_bitmap_contains(r, 1048555));
    roaring_bitmap_free(r);
}

// The flips in test_flip_*_container_removal must also leave a valid bitmap.
DEFINE_TEST(test_flip_to_empty_is_valid) {
    roaring_bitmap_t *inputs[3];
    inputs[0] = roaring_bitmap_from_range(0, 100, 1);
    roaring_bitmap_remove_run_compression(inputs[0]);
    inputs[1] = roaring_bitmap_create();
    for (uint32_t i = 0; i < 10000; i++) roaring_bitmap_add(inputs[1], i);
    inputs[2] = roaring_bitmap_from_range(0, 10000, 1);
    const uint32_t ends[] = {100, 10000, 10000};
    for (size_t k = 0; k < 3; k++) {
        roaring_bitmap_t *f = roaring_bitmap_flip(inputs[k], 0, ends[k]);
        assert_true(roaring_bitmap_is_empty(f));
        assert_true(f->high_low_container.size == 0);
        assert_bitmap_validate(f);
        roaring_bitmap_free(f);
        roaring_bitmap_flip_inplace(inputs[k], 0, ends[k]);
        assert_true(roaring_bitmap_is_empty(inputs[k]));
        assert_true(inputs[k]->high_low_container.size == 0);
        assert_bitmap_validate(inputs[k]);
        roaring_bitmap_free(inputs[k]);
    }
    roaring_bitmap_t *r = roaring_bitmap_from_range(0, 66002, 1);
    roaring_bitmap_flip_inplace(r, 0, 987653576);
    assert_true(roaring_bitmap_get_cardinality(r) == 987653576 - 66002);
    assert_true(roaring_bitmap_minimum(r) == 66002);
    assert_bitmap_validate(r);
    roaring_bitmap_free(r);
}

/* ---- 32-bit set operations ---- */

// Java containerSharingWithXor / testAstesana
static void check_container_sharing_after_inplace_op(bool cow, bool use_xor) {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, cow);
    roaring_bitmap_flip_inplace(r1, 131000, 131001);
    roaring_bitmap_t *r2 = roaring_bitmap_from(220000);
    roaring_bitmap_set_copy_on_write(r2, cow);
    roaring_bitmap_t *r3 = roaring_bitmap_from(66000);
    roaring_bitmap_set_copy_on_write(r3, cow);
    if (use_xor) {
        roaring_bitmap_xor_inplace(r2, r1);
    } else {
        roaring_bitmap_or_inplace(r2, r1);
    }
    roaring_bitmap_or_inplace(r2, r3);
    assert_true(roaring_bitmap_contains(r2, 131000));
    assert_true(roaring_bitmap_contains(r2, 66000));
    assert_true(roaring_bitmap_contains(r2, 220000));
    assert_false(roaring_bitmap_contains(r1, 66000));
    assert_true(roaring_bitmap_get_cardinality(r1) == 1);
    assert_true(roaring_bitmap_contains(r1, 131000));
    // mutate the result: the inputs must not change
    roaring_bitmap_add(r2, 131001);
    roaring_bitmap_remove(r2, 66000);
    assert_true(roaring_bitmap_get_cardinality(r1) == 1);
    assert_true(roaring_bitmap_contains(r3, 66000));
    assert_bitmap_validate(r1);
    assert_bitmap_validate(r2);
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r3);
}

DEFINE_TEST(test_inplace_ops_do_not_share_mutated_containers) {
    for (int cow = 0; cow < 2; cow++) {
        check_container_sharing_after_inplace_op(cow, true);
        check_container_sharing_after_inplace_op(cow, false);
    }
}

// Java xorDoesNotShareContainersWithItsInputs (and TestRunContainer
// xorAndAndNotWithAnEmptyArrayReturnANewContainer)
static void check_results_do_not_share(bool cow, bool from_buffer) {
    roaring_bitmap_t *a = roaring_bitmap_from_range(1, 20, 2);
    roaring_bitmap_set_copy_on_write(a, cow);
    roaring_bitmap_t *runs = roaring_bitmap_from_range(100, 105, 1);
    roaring_bitmap_run_optimize(runs);
    char *buf = NULL;
    if (from_buffer) {
        size_t size = roaring_bitmap_portable_size_in_bytes(runs);
        buf = (char *)malloc(size);
        roaring_bitmap_portable_serialize(runs, buf);
        roaring_bitmap_free(runs);
        runs = roaring_bitmap_portable_deserialize_safe(buf, size);
        assert_non_null(runs);
    }
    roaring_bitmap_set_copy_on_write(runs, cow);
    roaring_bitmap_t *expected_runs = roaring_bitmap_from_range(100, 105, 1);
    roaring_bitmap_t *a_copy = roaring_bitmap_copy(a);
    const roaring_bitmap_t *inputs[] = {a, a_copy, runs};
    roaring_bitmap_t *result = roaring_bitmap_xor_many(3, inputs);
    assert_true(roaring_bitmap_equals(result, runs));
    roaring_bitmap_or_inplace(result, runs);
    roaring_bitmap_add(result, 5000);
    roaring_bitmap_remove(result, 102);
    assert_true(roaring_bitmap_get_cardinality(result) == 5);
    assert_true(roaring_bitmap_equals(runs, expected_runs));
    roaring_bitmap_free(result);

    roaring_bitmap_t *empty = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(empty, cow);
    roaring_bitmap_t *results[5];
    results[0] = roaring_bitmap_xor(runs, empty);
    results[1] = roaring_bitmap_andnot(runs, empty);
    results[2] = roaring_bitmap_or(runs, empty);
    results[3] = roaring_bitmap_or(empty, runs);
    const roaring_bitmap_t *single[] = {runs};
    results[4] = roaring_bitmap_or_many(1, single);
    for (size_t i = 0; i < 5; i++) {
        assert_true(roaring_bitmap_equals(results[i], expected_runs));
        roaring_bitmap_remove(results[i], 102);
        roaring_bitmap_add(results[i], 106);
        assert_true(roaring_bitmap_equals(runs, expected_runs));
        assert_bitmap_validate(results[i]);
        roaring_bitmap_free(results[i]);
    }
    roaring_bitmap_free(empty);
    roaring_bitmap_free(a);
    roaring_bitmap_free(a_copy);
    roaring_bitmap_free(runs);
    roaring_bitmap_free(expected_runs);
    free(buf);
}

DEFINE_TEST(test_results_do_not_share_containers_with_inputs) {
    for (int cow = 0; cow < 2; cow++) {
        check_results_do_not_share(cow, false);
        check_results_do_not_share(cow, true);
    }
}

// Java horizontal_xorDropsBlocksThatCancelOut
DEFINE_TEST(test_xor_many_drops_cancelled_containers) {
    roaring_bitmap_t *a = roaring_bitmap_from(1, 3, 5, 70000);
    roaring_bitmap_t *b = roaring_bitmap_from(1, 3, 5);
    const roaring_bitmap_t *ab[] = {a, b};
    roaring_bitmap_t *x = roaring_bitmap_xor_many(2, ab);
    assert_true(roaring_bitmap_get_cardinality(x) == 1);
    assert_true(roaring_bitmap_contains(x, 70000));
    assert_true(x->high_low_container.size == 1);
    assert_bitmap_validate(x);
    roaring_bitmap_free(x);

    x = roaring_bitmap_xor(a, b);
    assert_true(x->high_low_container.size == 1);
    roaring_bitmap_free(x);

    roaring_bitmap_t *a_copy = roaring_bitmap_copy(a);
    const roaring_bitmap_t *aa[] = {a, a_copy};
    x = roaring_bitmap_xor_many(2, aa);
    assert_true(roaring_bitmap_is_empty(x));
    assert_true(x->high_low_container.size == 0);
    assert_bitmap_validate(x);
    roaring_bitmap_free(x);

    x = roaring_bitmap_lazy_xor(a, b);
    roaring_bitmap_repair_after_lazy(x);
    assert_true(roaring_bitmap_get_cardinality(x) == 1);
    assert_true(roaring_bitmap_contains(x, 70000));
    assert_true(x->high_low_container.size == 1);
    assert_bitmap_validate(x);
    roaring_bitmap_free(x);

    x = roaring_bitmap_copy(a);
    roaring_bitmap_lazy_xor_inplace(x, a_copy);
    roaring_bitmap_repair_after_lazy(x);
    assert_true(roaring_bitmap_is_empty(x));
    assert_true(x->high_low_container.size == 0);
    assert_bitmap_validate(x);
    roaring_bitmap_free(x);

    roaring_bitmap_free(a);
    roaring_bitmap_free(a_copy);
    roaring_bitmap_free(b);
}

// Java testLazyOr / testFirstLast_AfterLazyMutation1/2
DEFINE_TEST(test_lazy_or_inplace_then_repair) {
    for (int conversion = 0; conversion < 2; conversion++) {
        roaring_bitmap_t *r = roaring_bitmap_from(65536, 262144, 524288);
        roaring_bitmap_t *r2 = roaring_bitmap_from(4, 7, 8, 9);
        roaring_bitmap_t *r3 =
            roaring_bitmap_from(1, 2, 3, 4, 5, 65536, 131072, 1048576);
        roaring_bitmap_lazy_or_inplace(r, r2, conversion);
        roaring_bitmap_lazy_or_inplace(r, r3, conversion);
        roaring_bitmap_repair_after_lazy(r);
        const uint32_t expected_values[] = {
            1, 2, 3, 4, 5, 7, 8, 9, 65536, 131072, 262144, 524288, 1048576};
        roaring_bitmap_t *expected = roaring_bitmap_of_ptr(13, expected_values);
        assert_true(roaring_bitmap_equals(r, expected));
        assert_bitmap_validate(r);
        roaring_bitmap_free(expected);
        roaring_bitmap_free(r);
        roaring_bitmap_free(r2);
        roaring_bitmap_free(r3);

        // dense case: a bitset container
        r = roaring_bitmap_from_range(3, 65537, 3);
        r2 = roaring_bitmap_from(0, 2, 4, 6, 8);
        roaring_bitmap_t *eager = roaring_bitmap_or(r, r2);
        roaring_bitmap_t *lazy = roaring_bitmap_lazy_or(r, r2, conversion);
        roaring_bitmap_repair_after_lazy(lazy);
        roaring_bitmap_lazy_or_inplace(r, r2, conversion);
        roaring_bitmap_repair_after_lazy(r);
        assert_true(roaring_bitmap_minimum(r) == 0);
        assert_true(roaring_bitmap_maximum(r) == 65535);
        assert_true(roaring_bitmap_get_cardinality(r) ==
                    roaring_bitmap_get_cardinality(eager));
        assert_true(roaring_bitmap_equals(r, eager));
        assert_true(roaring_bitmap_equals(lazy, eager));
        assert_bitmap_validate(r);
        assert_bitmap_validate(lazy);
        roaring_bitmap_free(eager);
        roaring_bitmap_free(lazy);
        roaring_bitmap_free(r);
        roaring_bitmap_free(r2);
    }
}

// Go TestIssue330
DEFINE_TEST(test_issue330_wide_or_with_runs) {
    const uint32_t *values[3] = {issue330_values0, issue330_values1,
                                 issue330_values2};
    const size_t sizes[3] = {
        sizeof(issue330_values0) / sizeof(issue330_values0[0]),
        sizeof(issue330_values1) / sizeof(issue330_values1[0]),
        sizeof(issue330_values2) / sizeof(issue330_values2[0])};
    roaring_bitmap_t *b[3];
    for (int i = 0; i < 3; i++) {
        b[i] = roaring_bitmap_of_ptr(sizes[i], values[i]);
        roaring_bitmap_run_optimize(b[i]);
        assert_true(roaring_bitmap_get_cardinality(b[i]) == sizes[i]);
        assert_bitmap_validate(b[i]);
    }
    assert_true(roaring_bitmap_and_cardinality(b[0], b[1]) == 0);
    assert_true(roaring_bitmap_and_cardinality(b[0], b[2]) == 0);
    assert_true(roaring_bitmap_and_cardinality(b[1], b[2]) == 0);
    const roaring_bitmap_t *order012[] = {b[0], b[1], b[2]};
    const roaring_bitmap_t *order210[] = {b[2], b[1], b[0]};
    roaring_bitmap_t *results[6];
    results[0] = roaring_bitmap_or_many(3, order012);
    results[1] = roaring_bitmap_or_many(3, order210);
    results[2] = roaring_bitmap_or_many_heap(3, order012);
    results[3] = roaring_bitmap_or_many_heap(3, order210);
    results[4] = roaring_bitmap_or(b[0], b[1]);
    roaring_bitmap_or_inplace(results[4], b[2]);
    results[5] = roaring_bitmap_or(b[2], b[1]);
    roaring_bitmap_or_inplace(results[5], b[0]);
    for (int i = 0; i < 6; i++) {
        assert_true(roaring_bitmap_get_cardinality(results[i]) == 1040);
        assert_true(roaring_bitmap_equals(results[i], results[0]));
        assert_bitmap_validate(results[i]);
    }
    for (int i = 0; i < 6; i++) roaring_bitmap_free(results[i]);
    for (int i = 0; i < 3; i++) roaring_bitmap_free(b[i]);
}

// Go aggregation_test.go "issue 178"
DEFINE_TEST(test_issue178_sparse_or_many) {
    roaring_bitmap_t *b1 = roaring_bitmap_of_ptr(
        sizeof(issue178_values0) / sizeof(issue178_values0[0]),
        issue178_values0);
    roaring_bitmap_t *b2 = roaring_bitmap_of_ptr(
        sizeof(issue178_values1) / sizeof(issue178_values1[0]),
        issue178_values1);
    const roaring_bitmap_t *inputs[] = {b1, b2};
    roaring_bitmap_t *u1 = roaring_bitmap_or_many(2, inputs);
    roaring_bitmap_t *u2 = roaring_bitmap_or_many_heap(2, inputs);
    roaring_bitmap_t *u3 = roaring_bitmap_or(b1, b2);
    assert_true(roaring_bitmap_get_cardinality(u1) == 32);
    assert_true(roaring_bitmap_get_cardinality(u2) == 32);
    assert_true(roaring_bitmap_get_cardinality(u3) == 32);
    assert_true(roaring_bitmap_or_cardinality(b1, b2) == 32);
    roaring_bitmap_free(u1);
    roaring_bitmap_free(u2);
    roaring_bitmap_free(u3);
    roaring_bitmap_free(b1);
    roaring_bitmap_free(b2);
}

static roaring_bitmap_t *bitmap_with_type(roaring_bitmap_t *r, uint8_t type) {
    convert_all_containers(r, type);
    return r;
}

// Java subset regressions: issues 887 (bitmap level), 721 and 723 (container
// level, forced container types here)
DEFINE_TEST(test_subset_run_container_regressions) {
    // issue 887
    roaring_bitmap_t *big = roaring_bitmap_create();
    for (uint32_t i = 0; i < 10000; i++) {
        if (i != 100) roaring_bitmap_add(big, i);
    }
    roaring_bitmap_t *sub = roaring_bitmap_from_range(90, 101, 1);
    assert_false(roaring_bitmap_is_subset(sub, big));
    roaring_bitmap_add(big, 100);
    assert_true(roaring_bitmap_is_subset(sub, big));
    roaring_bitmap_free(big);
    roaring_bitmap_free(sub);

    big = bitmap_with_type(roaring_bitmap_from_range(0, 65535, 1),
                           BITSET_CONTAINER_TYPE);
    sub = roaring_bitmap_from_range(65530, 65536, 1);
    assert_false(roaring_bitmap_is_subset(sub, big));
    roaring_bitmap_free(sub);
    sub = roaring_bitmap_from_range(65530, 65535, 1);
    assert_true(roaring_bitmap_is_subset(sub, big));
    roaring_bitmap_free(sub);
    roaring_bitmap_add(big, 65535);
    sub = roaring_bitmap_from_range(65530, 65536, 1);
    assert_true(roaring_bitmap_is_subset(sub, big));
    roaring_bitmap_free(sub);
    roaring_bitmap_free(big);

    // issue 721, case 1: run {[0,60),[63,64),[66,67)} contains bitset {63}
    roaring_bitmap_t *run = roaring_bitmap_from_range(0, 60, 1);
    roaring_bitmap_add(run, 63);
    roaring_bitmap_add(run, 66);
    bitmap_with_type(run, RUN_CONTAINER_TYPE);
    roaring_bitmap_t *bits =
        bitmap_with_type(roaring_bitmap_from(63), BITSET_CONTAINER_TYPE);
    assert_true(roaring_bitmap_is_subset(bits, run));
    roaring_bitmap_free(run);
    roaring_bitmap_free(bits);
    // issue 721, case 2: run {[0,10),[12,13)} does not contain bitset {11}
    run = roaring_bitmap_from_range(0, 10, 1);
    roaring_bitmap_add(run, 12);
    bitmap_with_type(run, RUN_CONTAINER_TYPE);
    bits = bitmap_with_type(roaring_bitmap_from(11), BITSET_CONTAINER_TYPE);
    assert_false(roaring_bitmap_is_subset(bits, run));
    roaring_bitmap_free(run);
    roaring_bitmap_free(bits);
    // run [128,512) contains bitset [256,320) (empty leading words)
    run = bitmap_with_type(roaring_bitmap_from_range(128, 512, 1),
                           RUN_CONTAINER_TYPE);
    bits = bitmap_with_type(roaring_bitmap_from_range(256, 320, 1),
                            BITSET_CONTAINER_TYPE);
    assert_true(roaring_bitmap_is_subset(bits, run));
    roaring_bitmap_add(bits, 100);
    assert_false(roaring_bitmap_is_subset(bits, run));
    roaring_bitmap_free(run);
    roaring_bitmap_free(bits);
    // issue 723: array [0,10) contains run {5} but not run [5,11)
    roaring_bitmap_t *array = bitmap_with_type(
        roaring_bitmap_from_range(0, 10, 1), ARRAY_CONTAINER_TYPE);
    run = bitmap_with_type(roaring_bitmap_from(5), RUN_CONTAINER_TYPE);
    assert_true(roaring_bitmap_is_subset(run, array));
    roaring_bitmap_free(run);
    run = bitmap_with_type(roaring_bitmap_from_range(5, 11, 1),
                           RUN_CONTAINER_TYPE);
    assert_false(roaring_bitmap_is_subset(run, array));
    roaring_bitmap_free(run);
    roaring_bitmap_free(array);
}

// Go TestFuzzerRepro_* / TestFuzzerPanicRepro_* operations
DEFINE_TEST(test_ops_emptying_or_splitting_containers) {
    roaring_bitmap_t *a = roaring_bitmap_from(647253, 1048572);
    roaring_bitmap_t *b = roaring_bitmap_from(647253);
    roaring_bitmap_andnot_inplace(a, b);
    assert_true(roaring_bitmap_get_cardinality(a) == 1);
    assert_true(roaring_bitmap_contains(a, 1048572));
    assert_true(a->high_low_container.size == 1);
    assert_bitmap_validate(a);
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);

    a = roaring_bitmap_from_range(3, 7, 1);
    roaring_bitmap_run_optimize(a);
    b = roaring_bitmap_from(1);
    roaring_bitmap_or_inplace(a, b);
    roaring_bitmap_t *expected = roaring_bitmap_from(1, 3, 4, 5, 6);
    assert_true(roaring_bitmap_equals(a, expected));
    assert_bitmap_validate(a);
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);
    roaring_bitmap_free(expected);

    a = roaring_bitmap_from(0, 1048575);
    b = roaring_bitmap_copy(a);
    roaring_bitmap_t *c = roaring_bitmap_copy(a);
    roaring_bitmap_xor_inplace(a, b);
    assert_true(roaring_bitmap_is_empty(a));
    assert_true(a->high_low_container.size == 0);
    assert_bitmap_validate(a);
    roaring_bitmap_or_inplace(c, b);
    assert_true(roaring_bitmap_equals(c, b));
    assert_bitmap_validate(c);
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);
    roaring_bitmap_free(c);

    a = roaring_bitmap_from_range(1048565, 1048569, 1);
    roaring_bitmap_run_optimize(a);
    b = roaring_bitmap_from(1048566, 1048567);
    roaring_bitmap_andnot_inplace(a, b);
    expected = roaring_bitmap_from(1048565, 1048568);
    assert_true(roaring_bitmap_equals(a, expected));
    assert_bitmap_validate(a);
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);
    roaring_bitmap_free(expected);
}

// Go TestRoaring_AndMask / TestRoaring_OrThenAnd
DEFINE_TEST(test_full_range_mask_with_mixed_data) {
    roaring_bitmap_t *rb = roaring_bitmap_create();
    for (int i = 0; i < 1000000; i++) {
        roaring_bitmap_add(rb, 5000000 + our_rand() % 10000000);
    }
    roaring_bitmap_t *mask = roaring_bitmap_from_range(1, 10000001, 1);
    roaring_bitmap_t *expected = roaring_bitmap_and(rb, mask);
    roaring_bitmap_and_inplace(mask, rb);
    assert_bitmap_validate(mask);
    assert_true(roaring_bitmap_equals(mask, expected));
    roaring_bitmap_free(mask);
    roaring_bitmap_free(expected);
    roaring_bitmap_free(rb);

    // many runs in one container at key 4272, then OR/AND with a range
    for (int optimize = 0; optimize < 2; optimize++) {
        rb = roaring_bitmap_create();
        uint32_t base = UINT32_C(4272) << 16;
        for (uint32_t i = 0; i < 1135; i++) {
            uint32_t start = base + i * 57;
            roaring_bitmap_add_range(rb, start, start + 1 + i % 20);
        }
        roaring_bitmap_run_optimize(rb);
        roaring_bitmap_t *extra =
            roaring_bitmap_from(279981785, 279982923, 279988809, 279995913);
        roaring_bitmap_or_inplace(rb, extra);
        if (optimize) roaring_bitmap_run_optimize(rb);
        assert_bitmap_validate(rb);
        mask = roaring_bitmap_from_range(279900001, 280000001, 1);
        roaring_bitmap_t *anded = roaring_bitmap_and(rb, mask);
        roaring_bitmap_t *ored = roaring_bitmap_or(rb, mask);
        assert_bitmap_validate(anded);
        assert_bitmap_validate(ored);
        assert_true(roaring_bitmap_get_cardinality(anded) ==
                    roaring_bitmap_range_cardinality(rb, 279900001, 280000001));
        assert_true(roaring_bitmap_get_cardinality(ored) ==
                    roaring_bitmap_get_cardinality(rb) + 100000 -
                        roaring_bitmap_get_cardinality(anded));
        roaring_bitmap_and_inplace(mask, rb);
        assert_bitmap_validate(mask);
        assert_true(roaring_bitmap_equals(mask, anded));
        roaring_bitmap_or_inplace(rb, anded);
        assert_bitmap_validate(rb);
        roaring_bitmap_free(mask);
        roaring_bitmap_free(anded);
        roaring_bitmap_free(ored);
        roaring_bitmap_free(extra);
        roaring_bitmap_free(rb);
    }
}

// Java testWithYourself: x op x for every container type
DEFINE_TEST(test_self_aliasing_ops) {
    for (int t = 0; t < 3; t++) {
        roaring_bitmap_t *x;
        if (t == 0) {
            x = roaring_bitmap_from_range(1, 11, 1);
            roaring_bitmap_remove_run_compression(x);
        } else if (t == 1) {
            x = roaring_bitmap_from_range(0, 20000, 3);
        } else {
            x = roaring_bitmap_from_range(10, 5000, 1);
            roaring_bitmap_run_optimize(x);
        }
        roaring_bitmap_t *orig = roaring_bitmap_copy(x);
        uint64_t card = roaring_bitmap_get_cardinality(x);
        roaring_bitmap_t *r;
        r = roaring_bitmap_and(x, x);
        assert_true(roaring_bitmap_equals(r, orig));
        roaring_bitmap_free(r);
        r = roaring_bitmap_or(x, x);
        assert_true(roaring_bitmap_equals(r, orig));
        roaring_bitmap_free(r);
        r = roaring_bitmap_xor(x, x);
        assert_true(roaring_bitmap_is_empty(r));
        roaring_bitmap_free(r);
        r = roaring_bitmap_andnot(x, x);
        assert_true(roaring_bitmap_is_empty(r));
        roaring_bitmap_free(r);
        assert_true(roaring_bitmap_and_cardinality(x, x) == card);
        assert_true(roaring_bitmap_or_cardinality(x, x) == card);
        assert_true(roaring_bitmap_xor_cardinality(x, x) == 0);
        assert_true(roaring_bitmap_andnot_cardinality(x, x) == 0);
        assert_true(roaring_bitmap_intersect(x, x));
        assert_true(roaring_bitmap_is_subset(x, x));
        assert_false(roaring_bitmap_is_strict_subset(x, x));
        assert_true(roaring_bitmap_jaccard_index(x, x) == 1.0);
        roaring_bitmap_and_inplace(x, x);  // documented as allowed
        assert_true(roaring_bitmap_equals(x, orig));
        assert_bitmap_validate(x);
        roaring_bitmap_free(x);
        roaring_bitmap_free(orig);
    }
    roaring_bitmap_t *empty = roaring_bitmap_create();
    assert_false(roaring_bitmap_intersect(empty, empty));
    assert_true(roaring_bitmap_is_subset(empty, empty));
    roaring_bitmap_free(empty);
}

// Go TestRoaringInPlaceAndNotBitmapContainer
DEFINE_TEST(test_andnot_inplace_bitset_to_array_then_serialize) {
    roaring_bitmap_t *bm = roaring_bitmap_create();
    for (uint32_t i = 0; i < 8192; i++) roaring_bitmap_add(bm, i);
    assert_true(bm->high_low_container.typecodes[0] == BITSET_CONTAINER_TYPE);
    roaring_bitmap_t *sub = roaring_bitmap_from_range(128, 8192, 1);
    roaring_bitmap_andnot_inplace(bm, sub);
    assert_true(roaring_bitmap_get_cardinality(bm) == 128);
    assert_true(bm->high_low_container.typecodes[0] == ARRAY_CONTAINER_TYPE);
    assert_bitmap_validate(bm);
    size_t size = roaring_bitmap_portable_size_in_bytes(bm);
    char *buf = (char *)malloc(size);
    assert_true(roaring_bitmap_portable_serialize(bm, buf) == size);
    roaring_bitmap_t *back =
        roaring_bitmap_portable_deserialize_safe(buf, size);
    assert_non_null(back);
    assert_true(roaring_bitmap_equals(back, bm));
    assert_true(roaring_bitmap_portable_size_in_bytes(back) == size);
    assert_true(back->high_low_container.typecodes[0] == ARRAY_CONTAINER_TYPE);
    assert_bitmap_validate(back);
    free(buf);
    roaring_bitmap_free(back);
    roaring_bitmap_free(sub);
    roaring_bitmap_free(bm);
}

// Go TestBitmapOrBulkMergeCopyOnWrite{,Tail}Ownership
DEFINE_TEST(test_or_inplace_copy_on_write_isolation) {
    roaring_bitmap_t *left = roaring_bitmap_create();
    roaring_bitmap_t *right = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(left, true);
    roaring_bitmap_set_copy_on_write(right, true);
    for (uint32_t i = 0; i < 64; i++) {
        roaring_bitmap_add(left, ((2 * i) << 16) | 0);
        roaring_bitmap_add(right, ((2 * i + 1) << 16) | 1);
    }
    roaring_bitmap_t *left_orig = roaring_bitmap_copy(left);
    roaring_bitmap_t *right_orig = roaring_bitmap_copy(right);
    roaring_bitmap_set_copy_on_write(left_orig, false);
    roaring_bitmap_set_copy_on_write(right_orig, false);
    roaring_bitmap_t *receiver = roaring_bitmap_copy(left);
    roaring_bitmap_or_inplace(receiver, right);
    assert_true(roaring_bitmap_get_cardinality(receiver) == 128);

    const uint32_t tail = (127u << 16);
    roaring_bitmap_add(receiver, (1u << 16) | 10);
    roaring_bitmap_add(receiver, tail | 10);
    roaring_bitmap_add(receiver, 10);
    assert_true(roaring_bitmap_equals(right, right_orig));
    assert_true(roaring_bitmap_equals(left, left_orig));
    roaring_bitmap_t *receiver_snapshot = roaring_bitmap_copy(receiver);
    roaring_bitmap_set_copy_on_write(receiver_snapshot, false);
    roaring_bitmap_add(right, (1u << 16) | 11);
    roaring_bitmap_add(right, tail | 11);
    roaring_bitmap_add(left, 11);
    assert_true(roaring_bitmap_equals(receiver, receiver_snapshot));
    assert_false(roaring_bitmap_contains(receiver, (1u << 16) | 11));
    assert_false(roaring_bitmap_contains(receiver, 11));
    assert_bitmap_validate(receiver);
    assert_bitmap_validate(left);
    assert_bitmap_validate(right);
    roaring_bitmap_free(receiver_snapshot);
    roaring_bitmap_free(receiver);
    roaring_bitmap_free(left_orig);
    roaring_bitmap_free(right_orig);
    roaring_bitmap_free(left);
    roaring_bitmap_free(right);
}

// Binary and cardinality operations with an empty operand on either side.
static void check_empty_operands(bool cow) {
    roaring_bitmap_t *r = roaring_bitmap_from_range(100, 70000, 3);
    roaring_bitmap_set_copy_on_write(r, cow);
    roaring_bitmap_t *e = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(e, cow);
    roaring_bitmap_t *orig = roaring_bitmap_copy(r);
    uint64_t card = roaring_bitmap_get_cardinality(r);
    roaring_bitmap_t *x;
    roaring_bitmap_t *same[] = {
        roaring_bitmap_or(r, e), roaring_bitmap_or(e, r),
        roaring_bitmap_xor(r, e), roaring_bitmap_xor(e, r),
        roaring_bitmap_andnot(r, e)};
    for (size_t i = 0; i < 5; i++) {
        assert_true(roaring_bitmap_equals(same[i], r));
        roaring_bitmap_free(same[i]);
    }
    roaring_bitmap_t *empties[] = {roaring_bitmap_and(r, e),
                                   roaring_bitmap_and(e, r),
                                   roaring_bitmap_andnot(e, r)};
    for (size_t i = 0; i < 3; i++) {
        assert_true(roaring_bitmap_is_empty(empties[i]));
        roaring_bitmap_free(empties[i]);
    }
    // in place, with an empty receiver
    x = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(x, cow);
    roaring_bitmap_or_inplace(x, r);
    assert_true(roaring_bitmap_equals(x, r));
    roaring_bitmap_add(x, 1);
    roaring_bitmap_remove(x, 100);
    assert_true(roaring_bitmap_equals(r, orig));
    roaring_bitmap_free(x);
    x = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(x, cow);
    roaring_bitmap_xor_inplace(x, r);
    assert_true(roaring_bitmap_equals(x, r));
    roaring_bitmap_add(x, 1);
    roaring_bitmap_remove(x, 100);
    assert_true(roaring_bitmap_equals(r, orig));
    roaring_bitmap_free(x);
    x = roaring_bitmap_create();
    roaring_bitmap_and_inplace(x, r);
    assert_true(roaring_bitmap_is_empty(x));
    roaring_bitmap_andnot_inplace(x, r);
    assert_true(roaring_bitmap_is_empty(x));
    roaring_bitmap_free(x);
    // in place, with an empty argument
    x = roaring_bitmap_copy(r);
    roaring_bitmap_or_inplace(x, e);
    roaring_bitmap_xor_inplace(x, e);
    roaring_bitmap_andnot_inplace(x, e);
    assert_true(roaring_bitmap_equals(x, r));
    roaring_bitmap_and_inplace(x, e);
    assert_true(roaring_bitmap_is_empty(x));
    roaring_bitmap_free(x);
    // cardinalities
    assert_true(roaring_bitmap_and_cardinality(r, e) == 0);
    assert_true(roaring_bitmap_and_cardinality(e, r) == 0);
    assert_true(roaring_bitmap_or_cardinality(r, e) == card);
    assert_true(roaring_bitmap_or_cardinality(e, r) == card);
    assert_true(roaring_bitmap_xor_cardinality(r, e) == card);
    assert_true(roaring_bitmap_xor_cardinality(e, r) == card);
    assert_true(roaring_bitmap_andnot_cardinality(r, e) == card);
    assert_true(roaring_bitmap_andnot_cardinality(e, r) == 0);
    assert_false(roaring_bitmap_intersect(r, e));
    assert_false(roaring_bitmap_intersect(e, r));
    assert_true(roaring_bitmap_is_subset(e, r));
    assert_false(roaring_bitmap_is_subset(r, e));
    assert_true(roaring_bitmap_is_empty(e));
    assert_true(roaring_bitmap_equals(r, orig));
    roaring_bitmap_free(orig);
    roaring_bitmap_free(e);
    roaring_bitmap_free(r);
}

DEFINE_TEST(test_ops_with_empty_operands) {
    check_empty_operands(false);
    check_empty_operands(true);
}

// Go aggregation tests "aggregate nothing" / "single bitmap"
DEFINE_TEST(test_wide_aggregations_zero_or_one_input) {
    roaring_bitmap_t *r;
    r = roaring_bitmap_or_many(0, NULL);
    assert_non_null(r);
    assert_true(roaring_bitmap_is_empty(r));
    roaring_bitmap_free(r);
    r = roaring_bitmap_or_many_heap(0, NULL);
    assert_non_null(r);
    assert_true(roaring_bitmap_is_empty(r));
    roaring_bitmap_free(r);
    r = roaring_bitmap_xor_many(0, NULL);
    assert_non_null(r);
    assert_true(roaring_bitmap_is_empty(r));
    roaring_bitmap_free(r);
    for (int cow = 0; cow < 2; cow++) {
        roaring_bitmap_t *in = roaring_bitmap_from_range(0, 100000, 3);
        roaring_bitmap_add_range(in, 200000, 300000);
        roaring_bitmap_run_optimize(in);
        roaring_bitmap_set_copy_on_write(in, cow);
        roaring_bitmap_t *orig = roaring_bitmap_copy(in);
        const roaring_bitmap_t *inputs[] = {in};
        roaring_bitmap_t *results[3] = {roaring_bitmap_or_many(1, inputs),
                                        roaring_bitmap_or_many_heap(1, inputs),
                                        roaring_bitmap_xor_many(1, inputs)};
        for (int i = 0; i < 3; i++) {
            assert_true(roaring_bitmap_equals(results[i], in));
            roaring_bitmap_remove(results[i], 3);
            roaring_bitmap_remove(results[i], 250000);
            roaring_bitmap_add(results[i], 1);
            assert_true(roaring_bitmap_equals(in, orig));
            assert_bitmap_validate(results[i]);
            roaring_bitmap_free(results[i]);
        }
        roaring_bitmap_free(orig);
        roaring_bitmap_free(in);
    }
}

/* ---- iterators ---- */

// Java TestIterators advanceIfNeeded(-1), Go "advance out of a container value"
DEFINE_TEST(test_iterator_move_to_uint32_max) {
    for (int variant = 0; variant < 2; variant++) {
        roaring_bitmap_t *r = roaring_bitmap_from(0, 65535, 65536);
        if (variant == 0) {
            roaring_bitmap_add(r, UINT32_MAX);
        } else {
            roaring_bitmap_add_range(r, 0xFFFFFF00, UNIVERSE_END);
            roaring_bitmap_run_optimize(r);
        }
        roaring_uint32_iterator_t it;
        roaring_iterator_init(r, &it);
        assert_true(
            roaring_uint32_iterator_move_equalorlarger(&it, UINT32_MAX));
        assert_true(it.has_value);
        assert_true(it.current_value == UINT32_MAX);
        assert_false(roaring_uint32_iterator_advance(&it));
        assert_false(it.has_value);
        assert_true(roaring_uint32_iterator_previous(&it));
        assert_true(it.current_value == UINT32_MAX);
        roaring_bitmap_free(r);
    }
    roaring_bitmap_t *r = roaring_bitmap_from(1, 2);
    roaring_uint32_iterator_t it;
    roaring_iterator_init(r, &it);
    assert_false(roaring_uint32_iterator_move_equalorlarger(&it, UINT32_MAX));
    assert_false(it.has_value);
    assert_false(roaring_uint32_iterator_move_equalorlarger(&it, UINT32_MAX));
    assert_false(it.has_value);
    roaring_bitmap_free(r);
    r = roaring_bitmap_create();
    roaring_iterator_init(r, &it);
    assert_false(roaring_uint32_iterator_move_equalorlarger(&it, UINT32_MAX));
    assert_false(roaring_uint32_iterator_move_equalorlarger(&it, 0));
    roaring_bitmap_free(r);
}

static void check_read_from(const roaring_bitmap_t *r, uint32_t target,
                            uint32_t bufsize) {
    roaring_uint32_iterator_t it;
    roaring_iterator_init(r, &it);
    bool found = roaring_uint32_iterator_move_equalorlarger(&it, target);
    uint64_t expected_count =
        roaring_bitmap_range_cardinality(r, target, UNIVERSE_END);
    assert_true(found == (expected_count > 0));
    uint32_t *buf = (uint32_t *)malloc(sizeof(uint32_t) * bufsize);
    uint64_t total = 0;
    uint32_t previous = 0;
    while (true) {
        uint32_t got = roaring_uint32_iterator_read(&it, buf, bufsize);
        for (uint32_t i = 0; i < got; i++) {
            assert_true(buf[i] >= target);
            assert_true(roaring_bitmap_contains(r, buf[i]));
            if (total + i > 0) assert_true(buf[i] > previous);
            previous = buf[i];
        }
        total += got;
        if (got < bufsize) break;
    }
    assert_true(total == expected_count);
    assert_false(it.has_value);
    free(buf);
}

// Java RoaringBitmapBatchIteratorTest: batch read after advanceIfNeeded
DEFINE_TEST(test_read_uint32_iterator_after_seek) {
    uint32_t *values;
    uint32_t count;
    test_iterator_generate_data(&values, &count);
    roaring_bitmap_t *r = roaring_bitmap_of_ptr(count, values);
    free(values);
    const uint32_t bufsizes[] = {1, 7, 1000};
    for (int optimize = 0; optimize < 2; optimize++) {
        if (optimize) roaring_bitmap_run_optimize(r);
        uint32_t mid;
        assert_true(roaring_bitmap_select(
            r, (uint32_t)(roaring_bitmap_get_cardinality(r) / 2), &mid));
        uint32_t first_absent = 0;
        while (roaring_bitmap_contains(r, first_absent)) first_absent++;
        uint32_t max = roaring_bitmap_maximum(r);
        for (size_t b = 0; b < 3; b++) {
            check_read_from(r, mid, bufsizes[b]);
            check_read_from(r, first_absent, bufsizes[b]);
            check_read_from(r, 1234u << 16, bufsizes[b]);
            check_read_from(r, (1234u << 16) + 1, bufsizes[b]);
            if (max < UINT32_MAX) check_read_from(r, max + 1, bufsizes[b]);
        }
    }
    roaring_bitmap_free(r);

    uint32_t buf[10];
    roaring_uint32_iterator_t it;
    r = roaring_bitmap_from(8511);
    roaring_iterator_init(r, &it);
    assert_false(roaring_uint32_iterator_move_equalorlarger(&it, 8512));
    assert_false(it.has_value);
    assert_true(roaring_uint32_iterator_read(&it, buf, 10) == 0);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from(3 << 16, (3 << 16) + 5, (3 << 16) + 10);
    roaring_iterator_init(r, &it);
    assert_true(roaring_uint32_iterator_move_equalorlarger(&it, 6));
    assert_true(roaring_uint32_iterator_read(&it, buf, 10) == 3);
    assert_true(buf[0] == (3 << 16));
    roaring_bitmap_free(r);

    r = roaring_bitmap_from_range(10, 16, 1);
    roaring_bitmap_add(r, 18);
    roaring_bitmap_add_range(r, 20, 25);
    roaring_bitmap_run_optimize(r);
    const uint32_t probes[] = {10, 11, 12, 13, 14, 15, 18, 20, 21, 23, 24};
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        roaring_iterator_init(r, &it);
        assert_true(roaring_uint32_iterator_move_equalorlarger(&it, probes[i]));
        uint32_t got = roaring_uint32_iterator_read(&it, buf, 10);
        assert_true(got > 0);
        assert_true(buf[0] == probes[i]);
    }
    roaring_bitmap_free(r);
}

// Java TestEmptyRoaringBatchIterator
DEFINE_TEST(test_read_uint32_iterator_empty_bitmap) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    uint32_t buf[1024];
    roaring_uint32_iterator_t *it = roaring_iterator_create(r);
    assert_false(it->has_value);
    assert_true(roaring_uint32_iterator_read(it, buf, 1024) == 0);
    assert_true(roaring_uint32_iterator_read_backward(it, buf, 1024) == 0);
    assert_false(it->has_value);
    roaring_uint32_iterator_free(it);
    roaring_bitmap_add(r, 1);
    it = roaring_iterator_create(r);
    assert_true(roaring_uint32_iterator_read(it, buf, 1024) == 1);
    assert_true(buf[0] == 1);
    roaring_uint32_iterator_free(it);
    roaring_bitmap_free(r);
}

// Java TestIntIteratorFlyweight testClone / CloneBatchIteratorTest
DEFINE_TEST(test_uint32_iterator_copy_is_independent) {
    roaring_bitmap_t *r = roaring_bitmap_from(10, 20, 30, 65576, 65586, 65596);
    roaring_uint32_iterator_t *it = roaring_iterator_create(r);
    roaring_uint32_iterator_t *cp = roaring_uint32_iterator_copy(it);
    for (int i = 0; i < 3; i++) roaring_uint32_iterator_advance(cp);
    assert_true(cp->current_value == 65576);
    assert_true(it->current_value == 10);
    assert_true(roaring_uint32_iterator_advance(it));
    assert_true(it->current_value == 20);
    roaring_uint32_iterator_free(cp);

    cp = roaring_uint32_iterator_copy(it);
    uint32_t buf[3];
    assert_true(roaring_uint32_iterator_read(cp, buf, 3) == 3);
    assert_true(buf[0] == 20 && buf[1] == 30 && buf[2] == 65576);
    assert_true(it->current_value == 20);
    assert_true(roaring_uint32_iterator_read(it, buf, 3) == 3);
    assert_true(buf[0] == 20 && buf[1] == 30 && buf[2] == 65576);
    roaring_uint32_iterator_free(cp);
    roaring_uint32_iterator_free(it);

    roaring_uint32_iterator_t last;
    roaring_iterator_init_last(r, &last);
    assert_true(last.current_value == 65596);
    cp = roaring_uint32_iterator_copy(&last);
    for (int i = 0; i < 3; i++) roaring_uint32_iterator_previous(cp);
    assert_true(cp->current_value == 30);
    assert_true(last.current_value == 65596);
    assert_true(roaring_uint32_iterator_previous(&last));
    assert_true(last.current_value == 65586);
    roaring_uint32_iterator_free(cp);
    roaring_bitmap_free(r);
}

// Java ReverseIntIteratorFlyweightManyContainersTest
DEFINE_TEST(test_reverse_iteration_many_containers) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    for (uint32_t i = 0; i < 32769; i++) roaring_bitmap_add(r, i << 16);
    roaring_uint32_iterator_t it;
    roaring_iterator_init_last(r, &it);
    assert_true(it.has_value);
    assert_true(it.current_value == (UINT32_C(32768) << 16));
    uint32_t count = 0;
    uint32_t expected = 32768;
    while (it.has_value) {
        assert_true(it.current_value == (expected << 16));
        expected--;
        count++;
        roaring_uint32_iterator_previous(&it);
    }
    assert_true(count == 32769);
    uint32_t *buf = (uint32_t *)malloc(sizeof(uint32_t) * 1000);
    roaring_iterator_init_last(r, &it);
    uint64_t total = 0;
    uint32_t got;
    while ((got = roaring_uint32_iterator_read_backward(&it, buf, 1000)) > 0) {
        total += got;
    }
    assert_true(total == 32769);
    roaring_iterator_init(r, &it);
    total = 0;
    while ((got = roaring_uint32_iterator_read(&it, buf, 1000)) > 0) {
        total += got;
    }
    assert_true(total == 32769);
    free(buf);
    roaring_bitmap_free(r);
}

static bool collect_values(uint32_t value, void *param) {
    roaring_bitmap_t *out = (roaring_bitmap_t *)param;
    assert_false(roaring_bitmap_contains(out, value));  // visited once
    roaring_bitmap_add(out, value);
    return true;
}

// Java testCorruptionInfiniteLoop (issue 475)
DEFINE_TEST(test_iterate_runs_ending_at_signed_and_unsigned_max) {
    const uint32_t ends[] = {0x7FFFFFFF, 0xFFFFFFFF};
    for (int i = 0; i < 2; i++) {
        roaring_bitmap_t *r =
            roaring_bitmap_from_range(ends[i] - 3, (uint64_t)ends[i] + 1, 1);
        roaring_bitmap_run_optimize(r);
        roaring_bitmap_t *seen = roaring_bitmap_create();
        assert_true(roaring_iterate(r, collect_values, seen));
        assert_true(roaring_bitmap_get_cardinality(seen) == 4);
        assert_true(roaring_bitmap_equals(seen, r));
        roaring_bitmap_free(seen);
        roaring_bitmap_free(r);
    }
}

typedef struct {
    uint64_t calls;
    uint64_t stop_after;
} halting_counter_t;

static bool halting_counter(uint32_t value, void *param) {
    (void)value;
    halting_counter_t *c = (halting_counter_t *)param;
    c->calls++;
    return c->calls < c->stop_after;
}

// Go TestIterateHalt
DEFINE_TEST(test_iterate_halts_early) {
    roaring_bitmap_t *r = roaring_bitmap_from_range(150000, 450000, 1);
    uint64_t card = roaring_bitmap_get_cardinality(r);
    for (int optimize = 0; optimize < 2; optimize++) {
        if (optimize) {
            roaring_bitmap_remove_run_compression(r);
        }
        halting_counter_t c = {0, card - 1};
        assert_false(roaring_iterate(r, halting_counter, &c));
        assert_true(c.calls == card - 1);
        halting_counter_t c2 = {0, UINT64_MAX};
        assert_true(roaring_iterate(r, halting_counter, &c2));
        assert_true(c2.calls == card);
        halting_counter_t c3 = {0, 1};
        assert_false(roaring_iterate(r, halting_counter, &c3));
        assert_true(c3.calls == 1);
    }
    roaring_bitmap_free(r);
}

// Go TestRangesMaxUint32
DEFINE_TEST(test_read_ranges_at_uint32_max) {
    const uint32_t steps[] = {1, 2, 100};
    for (int c = 0; c < 7; c++) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        roaring_uint32_range_closed_t expected[5100];
        uint32_t n = 0;
        switch (c) {
            case 0:
                roaring_bitmap_add(r, UINT32_MAX);
                expected[n++] = closed_range(UINT32_MAX, UINT32_MAX);
                break;
            case 1:
                roaring_bitmap_add_range_closed(r, UINT32_MAX - 5, UINT32_MAX);
                expected[n++] = closed_range(UINT32_MAX - 5, UINT32_MAX);
                break;
            case 2:
                roaring_bitmap_add(r, UINT32_MAX - 1);
                roaring_bitmap_add(r, UINT32_MAX);
                expected[n++] = closed_range(UINT32_MAX - 1, UINT32_MAX);
                break;
            case 3:
                roaring_bitmap_add(r, 0);
                roaring_bitmap_add(r, UINT32_MAX);
                expected[n++] = closed_range(0, 0);
                expected[n++] = closed_range(UINT32_MAX, UINT32_MAX);
                break;
            case 4:
                roaring_bitmap_add_range(r, 0xFFFF0000, UNIVERSE_END);
                expected[n++] = closed_range(0xFFFF0000, UINT32_MAX);
                break;
            case 5:
                roaring_bitmap_add_range(r, 0xFFFEFF00, UNIVERSE_END);
                expected[n++] = closed_range(0xFFFEFF00, UINT32_MAX);
                break;
            default:
                for (uint32_t i = 0; i < 5000; i++) {
                    uint32_t v = 0xFFFF0000 + 3 * i;
                    roaring_bitmap_add(r, v);
                    expected[n++] = closed_range(v, v);
                }
                roaring_bitmap_add_range_closed(r, UINT32_MAX - 9, UINT32_MAX);
                expected[n++] = closed_range(UINT32_MAX - 9, UINT32_MAX);
                break;
        }
        for (int optimize = 0; optimize < 2; optimize++) {
            if (optimize) roaring_bitmap_run_optimize(r);
            for (size_t s = 0; s < 3; s++) {
                ranges_read_compare(r, expected, n, steps[s]);
                ranges_read_prev_compare(r, expected, n, steps[s]);
            }
        }
        roaring_bitmap_free(r);
    }
}

// Go iter_test.go range read merges across containers
DEFINE_TEST(test_read_ranges_multi_container_merges) {
    const uint32_t steps[] = {1, 2, 7, 100};
    for (int c = 0; c < 3; c++) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        roaring_uint32_range_closed_t expected[100];
        uint32_t n = 0;
        if (c == 0) {
            roaring_bitmap_add_range(r, 0xFF00, 0x10000);
            roaring_bitmap_add_range(r, 0x10000, 0x20000);
            roaring_bitmap_add_range(r, 0x20000, 0x20100);
            expected[n++] = closed_range(0xFF00, 0x200FF);
        } else if (c == 1) {
            roaring_bitmap_add_range_closed(r, 0xFFF0, 0xFFFE);
            roaring_bitmap_add_range_closed(r, 0x10000, 0x1000F);
            expected[n++] = closed_range(0xFFF0, 0xFFFE);
            expected[n++] = closed_range(0x10000, 0x1000F);
        } else {
            for (uint32_t i = 0; i < 100; i++) {
                uint32_t v = (i << 16) + 7;
                roaring_bitmap_add(r, v);
                expected[n++] = closed_range(v, v);
            }
        }
        for (uint8_t type = 0; type < 4; type++) {
            if (type == 3) {
                roaring_bitmap_run_optimize(r);
            } else {
                convert_all_containers(r, type == 0   ? ARRAY_CONTAINER_TYPE
                                          : type == 1 ? BITSET_CONTAINER_TYPE
                                                      : RUN_CONTAINER_TYPE);
            }
            for (size_t s = 0; s < 4; s++) {
                ranges_read_compare(r, expected, n, steps[s]);
                ranges_read_prev_compare(r, expected, n, steps[s]);
            }
        }
        roaring_bitmap_free(r);
    }
}

/* ---- copy-on-write ---- */

static roaring_bitmap_t *three_containers(bool cow) {
    roaring_bitmap_t *r = roaring_bitmap_from(1, 2, 3);
    roaring_bitmap_add_range(r, 131072, 132072);
    for (uint32_t v = 262144; v < 272144; v += 2) roaring_bitmap_add(r, v);
    roaring_bitmap_run_optimize(r);
    roaring_bitmap_set_copy_on_write(r, cow);
    return r;
}

// Java TestCopyOnWriteEdgeCases: writes after the shared containers moved
DEFINE_TEST(test_copy_on_write_writes_after_index_shift) {
    for (int c = 0; c < 3; c++) {
        roaring_bitmap_t *source = three_containers(true);
        roaring_bitmap_t *copy = roaring_bitmap_copy(source);
        roaring_bitmap_t *plain = three_containers(false);
        roaring_bitmap_t *targets[2] = {copy, plain};
        for (int t = 0; t < 2; t++) {
            roaring_bitmap_t *x = targets[t];
            if (c == 0) {
                roaring_bitmap_add(x, 65536);
                roaring_bitmap_remove(x, 262146);
            } else if (c == 1) {
                roaring_bitmap_add(x, 65536);
                roaring_bitmap_t *other = roaring_bitmap_from(262145);
                roaring_bitmap_or_inplace(x, other);
                roaring_bitmap_free(other);
            } else {
                roaring_bitmap_add(x, 5);
                roaring_bitmap_remove(x, 1);
                roaring_bitmap_remove(x, 2);
                roaring_bitmap_remove(x, 3);
                roaring_bitmap_remove(x, 5);
                roaring_bitmap_remove(x, 131072);
            }
        }
        roaring_bitmap_t *fresh = three_containers(false);
        assert_true(roaring_bitmap_equals(source, fresh));
        assert_true(roaring_bitmap_equals(copy, plain));
        assert_bitmap_validate(source);
        assert_bitmap_validate(copy);
        roaring_bitmap_free(fresh);
        roaring_bitmap_free(plain);
        roaring_bitmap_free(copy);
        roaring_bitmap_free(source);
    }
}

// Java TestCopyOnWriteEdgeCases: replacing the content forgets shared state
DEFINE_TEST(test_copy_on_write_clear_and_overwrite) {
    for (int c = 0; c < 3; c++) {
        roaring_bitmap_t *source = three_containers(true);
        roaring_bitmap_t *copy = roaring_bitmap_copy(source);
        roaring_bitmap_t *other = roaring_bitmap_from(7, 70000);
        if (c == 0) {
            roaring_bitmap_clear(copy);
        } else if (c == 1) {
            assert_true(roaring_bitmap_overwrite(copy, other));
        } else {
            roaring_bitmap_set_copy_on_write(other, true);
            assert_true(roaring_bitmap_overwrite(copy, other));
        }
        roaring_bitmap_add_range(copy, 0, 1 << 17);
        roaring_bitmap_add_range(copy, 262144, 262150);
        roaring_bitmap_remove(copy, 5);
        roaring_bitmap_remove(copy, 262146);
        roaring_bitmap_t *fresh = three_containers(false);
        assert_true(roaring_bitmap_equals(source, fresh));
        assert_bitmap_validate(source);
        assert_bitmap_validate(copy);
        uint64_t expected_card = (1 << 17) - 1 + 5;
        assert_true(roaring_bitmap_get_cardinality(copy) == expected_card);
        roaring_bitmap_free(fresh);
        roaring_bitmap_free(other);
        roaring_bitmap_free(copy);
        roaring_bitmap_free(source);
    }
}

/* ---- statistics ---- */

// Go TestStats, Java BitmapAnalyserTest
DEFINE_TEST(test_stats_per_container_type) {
    roaring_statistics_t stats;
    roaring_bitmap_t *r = roaring_bitmap_create();
    roaring_bitmap_statistics(r, &stats);
    assert_true(stats.n_containers == 0);
    assert_true(stats.n_array_containers == 0);
    assert_true(stats.n_run_containers == 0);
    assert_true(stats.n_bitset_containers == 0);
    assert_true(stats.cardinality == 0);
    roaring_bitmap_free(r);

    r = roaring_bitmap_create();
    for (uint32_t i = 0; i < 60000; i++) roaring_bitmap_add(r, i);
    roaring_bitmap_statistics(r, &stats);
    assert_true(stats.n_containers == 1);
    assert_true(stats.n_bitset_containers == 1);
    assert_true(stats.n_values_bitset_containers == 60000);
    assert_true(stats.n_bytes_bitset_containers == 8192);
    assert_true(stats.cardinality == 60000);
    assert_true(stats.min_value == 0);
    assert_true(stats.max_value == 59999);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from(2, 4);
    roaring_bitmap_statistics(r, &stats);
    assert_true(stats.n_containers == 1);
    assert_true(stats.n_array_containers == 1);
    assert_true(stats.n_values_array_containers == 2);
    assert_true(stats.n_bytes_array_containers == 4);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from_range(0, 1000, 1);
    roaring_bitmap_run_optimize(r);
    roaring_bitmap_statistics(r, &stats);
    assert_true(stats.n_containers == 1);
    assert_true(stats.n_run_containers == 1);
    assert_true(stats.n_values_run_containers == 1000);
    assert_true(stats.n_bytes_run_containers == 6);
    roaring_bitmap_free(r);

    r = roaring_bitmap_from(1, 3, 6, 26, 110, 1024);
    roaring_bitmap_add_range(r, 70000, 80000);
    for (uint32_t i = 0; i < 65536; i++) roaring_bitmap_add(r, (5 << 16) + i);
    roaring_bitmap_statistics(r, &stats);
    assert_true(stats.n_containers == 3);
    assert_true(stats.n_array_containers == 1);
    assert_true(stats.n_values_array_containers == 6);
    assert_true(stats.n_run_containers == 1);
    assert_true(stats.n_values_run_containers == 10000);
    assert_true(stats.n_bitset_containers == 1);
    assert_true(stats.n_values_bitset_containers == 65536);
    assert_true(stats.cardinality == 75542);
    roaring_bitmap_free(r);
}

int main() {
    tellmeall();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_or_inplace_few_containers),
        cmocka_unit_test(issue743),
        cmocka_unit_test(fuzz_deserializer),
        cmocka_unit_test(issue660),
        cmocka_unit_test(add_offset_each_container_type),
        cmocka_unit_test(andnot_run_containers_tail),
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
        cmocka_unit_test(overwrite_copy_on_write_with_plain),
        cmocka_unit_test(PyRoaringBitMap124),
        cmocka_unit_test(issue245),
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
        cmocka_unit_test(test_example_true),
        cmocka_unit_test(test_example_false),
        cmocka_unit_test(test_clear),
        cmocka_unit_test(can_copy_empty_true),
        cmocka_unit_test(can_copy_empty_false),
        cmocka_unit_test(test_intersect_small_run_bitset),
        cmocka_unit_test(test_intersect_array_container_pairs),
        cmocka_unit_test(test_intersect_shared_cow_arrays),
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
        cmocka_unit_test(test_serialize),
        cmocka_unit_test(test_portable_serialize),
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
        cmocka_unit_test(test_flip_sparse_array_container),
        cmocka_unit_test(test_word_range_to_bitset),
        cmocka_unit_test(select_test),
        cmocka_unit_test(test_subset),
        cmocka_unit_test(test_or_many_memory_leak),
        cmocka_unit_test(test_full_bitset_union_becomes_run),
        // cmocka_unit_test(test_run_to_bitset),
        // cmocka_unit_test(test_run_to_array),
        cmocka_unit_test(test_read_uint32_iterator_array),
        cmocka_unit_test(test_read_uint32_iterator_bitset),
        cmocka_unit_test(test_read_uint32_iterator_run),
        cmocka_unit_test(test_read_uint32_iterator_native),
        cmocka_unit_test(test_read_backward_uint32_iterator_array),
        cmocka_unit_test(test_read_backward_uint32_iterator_bitset),
        cmocka_unit_test(test_read_backward_uint32_iterator_run),
        cmocka_unit_test(test_read_backward_uint32_iterator_native),
        cmocka_unit_test(test_previous_iterator_array),
        cmocka_unit_test(test_previous_iterator_bitset),
        cmocka_unit_test(test_previous_iterator_run),
        cmocka_unit_test(test_previous_iterator_native),
        cmocka_unit_test(test_iterator_reuse),
        cmocka_unit_test(test_iterator_reuse_many),
        cmocka_unit_test(read_uint32_iterator_zero_count),
        cmocka_unit_test(test_uint32_iterator_skip_array),
        cmocka_unit_test(test_uint32_iterator_skip_bitset),
        cmocka_unit_test(test_uint32_iterator_skip_run),
        cmocka_unit_test(test_uint32_iterator_skip_native),
        cmocka_unit_test(test_uint32_iterator_skip_backward_array),
        cmocka_unit_test(test_uint32_iterator_skip_backward_bitset),
        cmocka_unit_test(test_uint32_iterator_skip_backward_run),
        cmocka_unit_test(test_uint32_iterator_skip_backward_native),
        cmocka_unit_test(test_read_ranges_iterator_array),
        cmocka_unit_test(test_read_ranges_iterator_bitset),
        cmocka_unit_test(test_read_ranges_iterator_run),
        cmocka_unit_test(test_read_ranges_iterator_native),
        cmocka_unit_test(test_read_prev_ranges_iterator_array),
        cmocka_unit_test(test_read_prev_ranges_iterator_bitset),
        cmocka_unit_test(test_read_prev_ranges_iterator_run),
        cmocka_unit_test(test_read_prev_ranges_iterator_native),
        cmocka_unit_test(test_read_ranges_empty_bitmap),
        cmocka_unit_test(test_read_ranges_cross_container),
        cmocka_unit_test(test_read_ranges_mid_range),
        cmocka_unit_test(test_read_ranges_interleaved),
        cmocka_unit_test(test_add_range),
        cmocka_unit_test(test_remove_range),
        cmocka_unit_test(test_remove_many),
        cmocka_unit_test(test_remove_many_copy_on_write),
        cmocka_unit_test(test_remove_many_copy_on_write_many_copies),
        cmocka_unit_test(test_inplace_mutators_copy_on_write),
        cmocka_unit_test(test_range_cardinality),
        cmocka_unit_test(test_range_end_beyond_universe),
        cmocka_unit_test(test_frozen_serialization),
        cmocka_unit_test(test_frozen_serialization_max_containers),
        cmocka_unit_test(test_frozen_serialization_empty),
        cmocka_unit_test(test_frozen_serialization_shared_containers),
#if ROARING_UNSAFE_FROZEN_TESTS
        cmocka_unit_test(test_portable_deserialize_frozen),
        cmocka_unit_test(test_portable_deserialize_frozen_unaligned),
#endif  // ROARING_UNSAFE_FROZEN_TESTS
        cmocka_unit_test(issue_15jan2024),
        cmocka_unit_test(test_intersect_with_range_edges),
        cmocka_unit_test(test_intersect_with_range_matches_cardinality),
        cmocka_unit_test(test_contains_range_agrees_with_range_cardinality),
        cmocka_unit_test(test_range_cardinality_layouts),
        cmocka_unit_test(test_run_optimize_recovers_space),
        cmocka_unit_test(test_issue467),
        cmocka_unit_test(add_offset_key_multiples),
        cmocka_unit_test(test_checked_add_remove_transitions),
        cmocka_unit_test(test_flip_karpenske),
        cmocka_unit_test(test_remove_range_issue377),
        cmocka_unit_test(test_equals_issue370),
        cmocka_unit_test(test_remove_run_compression_top_of_universe),
        cmocka_unit_test(test_rank_high_values),
        cmocka_unit_test(test_minimum_maximum_extremes),
        cmocka_unit_test(test_minimal_run_point_mutations),
        cmocka_unit_test(test_flip_to_empty_is_valid),
        cmocka_unit_test(test_inplace_ops_do_not_share_mutated_containers),
        cmocka_unit_test(test_results_do_not_share_containers_with_inputs),
        cmocka_unit_test(test_xor_many_drops_cancelled_containers),
        cmocka_unit_test(test_lazy_or_inplace_then_repair),
        cmocka_unit_test(test_issue330_wide_or_with_runs),
        cmocka_unit_test(test_issue178_sparse_or_many),
        cmocka_unit_test(test_subset_run_container_regressions),
        cmocka_unit_test(test_ops_emptying_or_splitting_containers),
        cmocka_unit_test(test_full_range_mask_with_mixed_data),
        cmocka_unit_test(test_self_aliasing_ops),
        cmocka_unit_test(test_andnot_inplace_bitset_to_array_then_serialize),
        cmocka_unit_test(test_or_inplace_copy_on_write_isolation),
        cmocka_unit_test(test_ops_with_empty_operands),
        cmocka_unit_test(test_wide_aggregations_zero_or_one_input),
        cmocka_unit_test(test_iterator_move_to_uint32_max),
        cmocka_unit_test(test_read_uint32_iterator_after_seek),
        cmocka_unit_test(test_read_uint32_iterator_empty_bitmap),
        cmocka_unit_test(test_uint32_iterator_copy_is_independent),
        cmocka_unit_test(test_reverse_iteration_many_containers),
        cmocka_unit_test(test_iterate_runs_ending_at_signed_and_unsigned_max),
        cmocka_unit_test(test_iterate_halts_early),
        cmocka_unit_test(test_read_ranges_at_uint32_max),
        cmocka_unit_test(test_read_ranges_multi_container_merges),
        cmocka_unit_test(test_copy_on_write_writes_after_index_shift),
        cmocka_unit_test(test_copy_on_write_clear_and_overwrite),
        cmocka_unit_test(test_stats_per_container_type),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
