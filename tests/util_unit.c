/*
 * util_unit.c
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/array_util.h>
#include <roaring/bitset_util.h>
#include <roaring/isadetection.h>
#include <roaring/misc/configreport.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
using namespace roaring::internal;
using namespace roaring::misc;
#endif

#include "test.h"

DEFINE_TEST(setandextract_uint16) {
    const unsigned int bitset_size = 1 << 16;
    const unsigned int bitset_size_in_words =
        bitset_size / (sizeof(uint64_t) * 8);

    for (unsigned int offset = 1; offset < bitset_size; offset++) {
        const unsigned int valsize = bitset_size / offset;
        uint16_t* vals = (uint16_t*)malloc(valsize * sizeof(uint16_t));
        uint64_t* bitset =
            (uint64_t*)calloc(bitset_size_in_words, sizeof(uint64_t));
        for (unsigned int k = 0; k < valsize; ++k) {
            vals[k] = (uint16_t)(k * offset);
        }

        bitset_set_list(bitset, vals, valsize);
        uint16_t* newvals = (uint16_t*)malloc(valsize * sizeof(uint16_t));
        bitset_extract_setbits_uint16(bitset, bitset_size_in_words, newvals, 0);

        for (unsigned int k = 0; k < valsize; ++k) {
            assert_int_equal(newvals[k], vals[k]);
        }

        free(vals);
        free(newvals);
        free(bitset);
    }
}

DEFINE_TEST(setandextract_uint32) {
    const unsigned int bitset_size = 1 << 16;
    const unsigned int bitset_size_in_words =
        bitset_size / (sizeof(uint64_t) * 8);

    for (unsigned int offset = 1; offset < bitset_size; offset++) {
        const unsigned int valsize = bitset_size / offset;
        uint16_t* vals = (uint16_t*)malloc(valsize * sizeof(uint16_t));
        uint64_t* bitset =
            (uint64_t*)calloc(bitset_size_in_words, sizeof(uint64_t));

        for (unsigned int k = 0; k < valsize; ++k) {
            vals[k] = (uint16_t)(k * offset);
        }

        bitset_set_list(bitset, vals, valsize);
        uint32_t* newvals = (uint32_t*)malloc(valsize * sizeof(uint32_t));
        bitset_extract_setbits(bitset, bitset_size_in_words, newvals, 0);

        for (unsigned int k = 0; k < valsize; ++k) {
            assert_int_equal(newvals[k], vals[k]);
        }

        free(vals);
        free(newvals);
        free(bitset);
    }
}

#if CROARING_IS_X64 && CROARING_COMPILER_SUPPORTS_AVX512
// Checks the AVX-512 (VBMI2) decoders against the scalar ones. The kernels
// write with masks derived from the popcount of each word, so the interesting
// cases are the block boundaries (16/32/48/64 values) and an output buffer
// sized exactly to the cardinality, with no padding.
DEFINE_TEST(avx512_extract_matches_scalar) {
    if ((croaring_hardware_support() & ROARING_SUPPORTS_AVX512) == 0) {
        return;  // no VBMI2 here
    }
    const size_t length = (1 << 16) / 64;
    uint64_t* words = (uint64_t*)malloc(length * sizeof(uint64_t));
    assert_non_null(words);

    // A deterministic set of densities, from very sparse to almost full.
    for (unsigned int step = 1; step <= 64; step++) {
        size_t cardinality = 0;
        for (size_t i = 0; i < length; ++i) {
            uint64_t w = 0;
            for (unsigned int b = (unsigned int)(i % step); b < 64; b += step) {
                w |= UINT64_C(1) << b;
            }
            words[i] = w;
            cardinality += (size_t)roaring_hamming(w);
        }

        const uint32_t base32 = 1u << 20;
        const uint16_t base16 = 3;
        // Exactly the cardinality, plus a few truncated capacities.
        size_t capacities[] = {cardinality, cardinality / 2, 64, 63, 16, 1, 0};
        for (size_t k = 0; k < sizeof(capacities) / sizeof(capacities[0]);
             k++) {
            size_t capacity = capacities[k];

            uint32_t* got32 = (uint32_t*)malloc((capacity + 1) * 4);
            uint32_t* want32 = (uint32_t*)malloc((capacity + 1) * 4);
            uint16_t* got16 = (uint16_t*)malloc((capacity + 1) * 2);
            uint16_t* want16 = (uint16_t*)malloc((capacity + 1) * 2);
            assert_non_null(got32);
            assert_non_null(want32);
            assert_non_null(got16);
            assert_non_null(want16);

            // Scalar reference, truncated to the same capacity.
            size_t n = 0;
            uint32_t base = base32;
            for (size_t i = 0; (i < length) && (n < capacity); ++i) {
                uint64_t w = words[i];
                while ((w != 0) && (n < capacity)) {
                    int r = roaring_trailing_zeroes(w);
                    want32[n] = (uint32_t)(r + base);
                    want16[n] = (uint16_t)(r + base16 + i * 64);
                    n++;
                    w &= (w - 1);
                }
                base += 64;
            }

            size_t n32 = bitset_extract_setbits_avx512(words, length, got32,
                                                       capacity, base32);
            size_t n16 = bitset_extract_setbits_avx512_uint16(
                words, length, got16, capacity, base16);
            assert_int_equal(n32, n);
            assert_int_equal(n16, n);
            for (size_t j = 0; j < n; ++j) {
                assert_int_equal(got32[j], want32[j]);
                assert_int_equal(got16[j], want16[j]);
            }

            free(got32);
            free(want32);
            free(got16);
            free(want16);
        }
    }
    free(words);
}
#endif  // CROARING_IS_X64 && CROARING_COMPILER_SUPPORTS_AVX512

static uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

// Checks the branchless intersection decoders against the plain loop, on
// random bitsets with densities from empty to full, so that every choice of
// unrolling (and the fallback for words with many bits) is exercised. The
// output buffer holds exactly the cardinality: a sanitizer catches any write
// past it.
DEFINE_TEST(intersection_extract_matches_scalar) {
    const size_t length = (1 << 16) / 64;
    uint64_t* words1 = (uint64_t*)malloc(length * sizeof(uint64_t));
    uint64_t* words2 = (uint64_t*)malloc(length * sizeof(uint64_t));
    uint16_t* want = (uint16_t*)malloc((1 << 16) * sizeof(uint16_t));
    assert_non_null(words1);
    assert_non_null(words2);
    assert_non_null(want);
    uint64_t rng = 1234;
    // Each bit is set with probability density / 64 in both inputs.
    for (unsigned int density = 0; density <= 64; density++) {
        for (unsigned int trial = 0; trial < 4; trial++) {
            for (size_t i = 0; i < length; ++i) {
                uint64_t w1 = 0, w2 = 0;
                for (unsigned int b = 0; b < 64; b++) {
                    if ((splitmix64(&rng) & 63) < density)
                        w1 |= UINT64_C(1) << b;
                    if ((splitmix64(&rng) & 63) < density)
                        w2 |= UINT64_C(1) << b;
                }
                words1[i] = w1;
                words2[i] = w2;
            }
            const uint16_t base = (uint16_t)(trial * 7);
            size_t card = bitset_extract_intersection_setbits_uint16(
                words1, words2, length, want, base);
            uint16_t* got =
                (uint16_t*)malloc((card > 0 ? card : 1) * sizeof(uint16_t));
            assert_non_null(got);
            size_t n = bitset_extract_intersection_setbits_uint16_with_card(
                words1, words2, length, got, card, base);
            assert_int_equal(n, card);
            for (size_t j = 0; j < card; ++j) {
                assert_int_equal(got[j], want[j]);
            }
#if CROARING_IS_X64 && CROARING_COMPILER_SUPPORTS_AVX512
            if (croaring_hardware_support() & ROARING_SUPPORTS_AVX512) {
                n = bitset_extract_intersection_setbits_avx512_uint16(
                    words1, words2, length, got, card, base);
                assert_int_equal(n, card);
                for (size_t j = 0; j < card; ++j) {
                    assert_int_equal(got[j], want[j]);
                }
            }
#endif
            free(got);
        }
    }
    free(words1);
    free(words2);
    free(want);
}

// Fills "out" with a sorted set of distinct values following one of several
// patterns, from random to strongly clustered. Returns its size.
static size_t make_sorted_set(uint64_t* state, int pattern, uint16_t* out) {
    size_t n = 0;
    const uint32_t start = (uint32_t)(splitmix64(state) % 2000);
    const uint32_t density = 1 + (uint32_t)(splitmix64(state) % 64);
    for (uint32_t v = start; v < (1 << 16) && n < 4096; v++) {
        bool take;
        switch (pattern) {
            case 0:  // random
                take = (splitmix64(state) & 63) < density;
                break;
            case 1:  // blocks of 64
                take = ((v / 64) % 2) == (density % 2);
                break;
            case 2:  // one run
                take = v < start + 100 * density;
                break;
            default:  // every third value
                take = v % 3 == 0;
                break;
        }
        if (take) out[n++] = (uint16_t)v;
    }
    return n;
}

// Compares the scalar array merges with simple reference implementations,
// including the in-place layouts that the containers use.
DEFINE_TEST(scalar_merges_match_reference) {
    uint16_t* a = (uint16_t*)malloc(4096 * sizeof(uint16_t));
    uint16_t* b = (uint16_t*)malloc(4096 * sizeof(uint16_t));
    uint16_t* want = (uint16_t*)malloc(8192 * sizeof(uint16_t));
    uint16_t* got = (uint16_t*)malloc(8192 * sizeof(uint16_t));
    bool* in_a = (bool*)malloc(1 << 16);
    bool* in_b = (bool*)malloc(1 << 16);
    assert_non_null(a);
    assert_non_null(b);
    assert_non_null(want);
    assert_non_null(got);
    assert_non_null(in_a);
    assert_non_null(in_b);
    uint64_t rng = 99;
    for (int trial = 0; trial < 2000; trial++) {
        size_t na = make_sorted_set(&rng, trial % 4, a);
        size_t nb = make_sorted_set(&rng, (trial / 4) % 4, b);
        if (trial % 50 == 0) na = 0;
        if (trial % 70 == 1) nb = trial % 3;
        memset(in_a, 0, 1 << 16);
        memset(in_b, 0, 1 << 16);
        for (size_t i = 0; i < na; i++) in_a[a[i]] = true;
        for (size_t i = 0; i < nb; i++) in_b[b[i]] = true;
        for (int op = 0; op < 4; op++) {
            size_t nwant = 0;
            for (uint32_t v = 0; v < (1 << 16); v++) {
                bool keep = op == 0   ? (in_a[v] && in_b[v])
                            : op == 1 ? (in_a[v] || in_b[v])
                            : op == 2 ? (in_a[v] != in_b[v])
                                      : (in_a[v] && !in_b[v]);
                if (keep) want[nwant++] = (uint16_t)v;
            }
            size_t n;
            if (op == 0) {
                n = (size_t)intersect_uint16(a, na, b, nb, got);
                assert_int_equal(n, nwant);
                assert_true(memcmp(got, want, n * sizeof(uint16_t)) == 0);
                assert_int_equal(intersect_uint16_cardinality(a, na, b, nb),
                                 nwant);
                // In place, in either input.
                memcpy(got, a, na * sizeof(uint16_t));
                n = (size_t)intersect_uint16(got, na, b, nb, got);
                assert_int_equal(n, nwant);
                assert_true(memcmp(got, want, n * sizeof(uint16_t)) == 0);
                memcpy(got, b, nb * sizeof(uint16_t));
                n = (size_t)intersect_uint16(a, na, got, nb, got);
                assert_int_equal(n, nwant);
                assert_true(memcmp(got, want, n * sizeof(uint16_t)) == 0);
            } else if (op == 1) {
                n = union_uint16(a, na, b, nb, got);
                assert_int_equal(n, nwant);
                assert_true(memcmp(got, want, n * sizeof(uint16_t)) == 0);
                // In place: a stored nb values into the output buffer.
                memcpy(got + nb, a, na * sizeof(uint16_t));
                n = union_uint16(got + nb, na, b, nb, got);
                assert_int_equal(n, nwant);
                assert_true(memcmp(got, want, n * sizeof(uint16_t)) == 0);
            } else if (op == 2) {
                n = (size_t)xor_uint16(a, (int32_t)na, b, (int32_t)nb, got);
                assert_int_equal(n, nwant);
                assert_true(memcmp(got, want, n * sizeof(uint16_t)) == 0);
            } else {
                n = (size_t)difference_uint16(a, (int)na, b, (int)nb, got);
                assert_int_equal(n, nwant);
                assert_true(memcmp(got, want, n * sizeof(uint16_t)) == 0);
                memcpy(got, a, na * sizeof(uint16_t));
                n = (size_t)difference_uint16(got, (int)na, b, (int)nb, got);
                assert_int_equal(n, nwant);
                assert_true(memcmp(got, want, n * sizeof(uint16_t)) == 0);
            }
        }
    }
    free(a);
    free(b);
    free(want);
    free(got);
    free(in_a);
    free(in_b);
}

int main() {
    tellmeall();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(setandextract_uint16),
        cmocka_unit_test(setandextract_uint32),
#if CROARING_IS_X64 && CROARING_COMPILER_SUPPORTS_AVX512
        cmocka_unit_test(avx512_extract_matches_scalar),
#endif
        cmocka_unit_test(intersection_extract_matches_scalar),
        cmocka_unit_test(scalar_merges_match_reference),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
