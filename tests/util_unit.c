/*
 * util_unit.c
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/bitset_util.h>
#include <roaring/isadetection.h>
#include <roaring/misc/configreport.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
using namespace roaring::internal;
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

int main() {
    tellmeall();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(setandextract_uint16),
        cmocka_unit_test(setandextract_uint32),
#if CROARING_IS_X64 && CROARING_COMPILER_SUPPORTS_AVX512
        cmocka_unit_test(avx512_extract_matches_scalar),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
