/*
 * bitset.c
 *
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/bitset_util.h>
#include <roaring/containers/bitset.h>
#include <roaring/portability.h>
#include <roaring/utilasm.h>

extern inline int bitset_container_cardinality(const bitset_container_t *bitset);
extern inline bool bitset_container_nonzero_cardinality(bitset_container_t *bitset);
extern inline void bitset_container_set(bitset_container_t *bitset, uint16_t pos);
extern inline void bitset_container_unset(bitset_container_t *bitset, uint16_t pos);
extern inline bool bitset_container_get(const bitset_container_t *bitset,
                                        uint16_t pos);
extern inline int32_t bitset_container_serialized_size_in_bytes(void);
extern inline bool bitset_container_add(bitset_container_t *bitset, uint16_t pos);
extern inline bool bitset_container_remove(bitset_container_t *bitset, uint16_t pos);
extern inline bool bitset_container_contains(const bitset_container_t *bitset,
                                             uint16_t pos);

void bitset_container_clear(bitset_container_t *bitset) {
    memset(bitset->array, 0, sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
    bitset->cardinality = 0;
}

void bitset_container_set_all(bitset_container_t *bitset) {
    memset(bitset->array, INT64_C(-1),
           sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
    bitset->cardinality = (1 << 16);
}



/* Create a new bitset. Return NULL in case of failure. */
bitset_container_t *bitset_container_create(void) {
    bitset_container_t *bitset =
        (bitset_container_t *)malloc(sizeof(bitset_container_t));

    if (!bitset) {
        return NULL;
    }
    // sizeof(__m256i) == 32
    bitset->array = (uint64_t *)roaring_bitmap_aligned_malloc(
        32, sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
    if (!bitset->array) {
        free(bitset);
        return NULL;
    }
    bitset_container_clear(bitset);
    return bitset;
}

/* Copy one container into another. We assume that they are distinct. */
void bitset_container_copy(const bitset_container_t *source,
                           bitset_container_t *dest) {
    dest->cardinality = source->cardinality;
    memcpy(dest->array, source->array,
           sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
}

void bitset_container_add_from_range(bitset_container_t *bitset, uint32_t min,
                                     uint32_t max, uint16_t step) {
    if (step == 0) return;   // refuse to crash
    if ((64 % step) == 0) {  // step divides 64
        uint64_t mask = 0;   // construct the repeated mask
        for (uint32_t value = (min % step); value < 64; value += step) {
            mask |= ((uint64_t)1 << value);
        }
        uint32_t firstword = min / 64;
        uint32_t endword = (max - 1) / 64;
        bitset->cardinality = (max - min + step - 1) / step;
        if (firstword == endword) {
            bitset->array[firstword] |=
                mask & (((~UINT64_C(0)) << (min % 64)) &
                        ((~UINT64_C(0)) >> ((~max + 1) % 64)));
            return;
        }
        bitset->array[firstword] = mask & ((~UINT64_C(0)) << (min % 64));
        for (uint32_t i = firstword + 1; i < endword; i++)
            bitset->array[i] = mask;
        bitset->array[endword] = mask & ((~UINT64_C(0)) >> ((~max + 1) % 64));
    } else {
        for (uint32_t value = min; value < max; value += step) {
            bitset_container_add(bitset, value);
        }
    }
}

/* Free memory. */
void bitset_container_free(bitset_container_t *bitset) {
    if(bitset->array != NULL) {// Jon Strabala reports that some tools complain otherwise
      roaring_bitmap_aligned_free(bitset->array);
      bitset->array = NULL; // pedantic
    }
    free(bitset);
}

/* duplicate container. */
bitset_container_t *bitset_container_clone(const bitset_container_t *src) {
    bitset_container_t *bitset =
        (bitset_container_t *)malloc(sizeof(bitset_container_t));

    if (!bitset) {
        return NULL;
    }
    // sizeof(__m256i) == 32
    bitset->array = (uint64_t *)roaring_bitmap_aligned_malloc(
        32, sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
    if (!bitset->array) {
        free(bitset);
        return NULL;
    }
    bitset->cardinality = src->cardinality;
    memcpy(bitset->array, src->array,
           sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
    return bitset;
}

void bitset_container_set_range(bitset_container_t *bitset, uint32_t begin,
                                uint32_t end) {
    bitset_set_range(bitset->array, begin, end);
    bitset->cardinality =
        bitset_container_compute_cardinality(bitset);  // could be smarter
}


bool bitset_container_intersect(const bitset_container_t *src_1,
                                  const bitset_container_t *src_2) {
	// could vectorize, but this is probably already quite fast in practice
    const uint64_t * __restrict__ array_1 = src_1->array;
    const uint64_t * __restrict__ array_2 = src_2->array;
	for (int i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i ++) {
        if((array_1[i] & array_2[i]) != 0) return true;
    }
    return false;
}


#ifdef USEAVX
#ifndef WORDS_IN_AVX2_REG
#define WORDS_IN_AVX2_REG sizeof(__m256i) / sizeof(uint64_t)
#endif
/* Get the number of bits set (force computation) */
int bitset_container_compute_cardinality(const bitset_container_t *bitset) {
    return (int) avx2_harley_seal_popcount256(
        (const __m256i *)bitset->array,
        BITSET_CONTAINER_SIZE_IN_WORDS / (WORDS_IN_AVX2_REG));
}

#elif defined(USENEON)
int bitset_container_compute_cardinality(const bitset_container_t *bitset) {
    uint16x8_t n0 = vdupq_n_u16(0);
    uint16x8_t n1 = vdupq_n_u16(0);
    uint16x8_t n2 = vdupq_n_u16(0);
    uint16x8_t n3 = vdupq_n_u16(0);
    for (size_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i += 8) {
        uint64x2_t c0 = vld1q_u64(&bitset->array[i + 0]);
        n0 = vaddq_u16(n0, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c0))));
        uint64x2_t c1 = vld1q_u64(&bitset->array[i + 2]);
        n1 = vaddq_u16(n1, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c1))));
        uint64x2_t c2 = vld1q_u64(&bitset->array[i + 4]);
        n2 = vaddq_u16(n2, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c2))));
        uint64x2_t c3 = vld1q_u64(&bitset->array[i + 6]);
        n3 = vaddq_u16(n3, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c3))));
    }
    uint64x2_t n = vdupq_n_u64(0);
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n0)));
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n1)));
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n2)));
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n3)));
    return vgetq_lane_u64(n, 0) + vgetq_lane_u64(n, 1);
}

#else

/* Get the number of bits set (force computation) */
int bitset_container_compute_cardinality(const bitset_container_t *bitset) {
    const uint64_t *array = bitset->array;
    int32_t sum = 0;
    for (int i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i += 4) {
        sum += hamming(array[i]);
        sum += hamming(array[i + 1]);
        sum += hamming(array[i + 2]);
        sum += hamming(array[i + 3]);
    }
    return sum;
}

#endif

#ifdef USEAVX

#define BITSET_CONTAINER_FN_REPEAT 8
#ifndef WORDS_IN_AVX2_REG
#define WORDS_IN_AVX2_REG sizeof(__m256i) / sizeof(uint64_t)
#endif
#define LOOP_SIZE                    \
    BITSET_CONTAINER_SIZE_IN_WORDS / \
        ((WORDS_IN_AVX2_REG)*BITSET_CONTAINER_FN_REPEAT)

/* Computes a binary operation (eg union) on bitset1 and bitset2 and write the
   result to bitsetout */
// clang-format off
#define BITSET_CONTAINER_FN(opname, opsymbol, avx_intrinsic, neon_intrinsic)  \
int bitset_container_##opname##_nocard(const bitset_container_t *src_1, \
                                       const bitset_container_t *src_2, \
                                       bitset_container_t *dst) {       \
    const uint8_t * __restrict__ array_1 = (const uint8_t *)src_1->array; \
    const uint8_t * __restrict__ array_2 = (const uint8_t *)src_2->array; \
    /* not using the blocking optimization for some reason*/            \
    uint8_t *out = (uint8_t*)dst->array;                                \
    const int innerloop = 8;                                            \
    for (size_t i = 0;                                                  \
        i < BITSET_CONTAINER_SIZE_IN_WORDS / (WORDS_IN_AVX2_REG);       \
                                                         i+=innerloop) {\
        __m256i A1, A2, AO;                                             \
        A1 = _mm256_lddqu_si256((const __m256i *)(array_1));                  \
        A2 = _mm256_lddqu_si256((const __m256i *)(array_2));                  \
        AO = avx_intrinsic(A2, A1);                                     \
        _mm256_storeu_si256((__m256i *)out, AO);                        \
        A1 = _mm256_lddqu_si256((const __m256i *)(array_1 + 32));             \
        A2 = _mm256_lddqu_si256((const __m256i *)(array_2 + 32));             \
        AO = avx_intrinsic(A2, A1);                                     \
        _mm256_storeu_si256((__m256i *)(out+32), AO);                   \
        A1 = _mm256_lddqu_si256((const __m256i *)(array_1 + 64));             \
        A2 = _mm256_lddqu_si256((const __m256i *)(array_2 + 64));             \
        AO = avx_intrinsic(A2, A1);                                     \
        _mm256_storeu_si256((__m256i *)(out+64), AO);                   \
        A1 = _mm256_lddqu_si256((const __m256i *)(array_1 + 96));             \
        A2 = _mm256_lddqu_si256((const __m256i *)(array_2 + 96));             \
        AO = avx_intrinsic(A2, A1);                                     \
        _mm256_storeu_si256((__m256i *)(out+96), AO);                   \
        A1 = _mm256_lddqu_si256((const __m256i *)(array_1 + 128));            \
        A2 = _mm256_lddqu_si256((const __m256i *)(array_2 + 128));            \
        AO = avx_intrinsic(A2, A1);                                     \
        _mm256_storeu_si256((__m256i *)(out+128), AO);                  \
        A1 = _mm256_lddqu_si256((const __m256i *)(array_1 + 160));            \
        A2 = _mm256_lddqu_si256((const __m256i *)(array_2 + 160));            \
        AO = avx_intrinsic(A2, A1);                                     \
        _mm256_storeu_si256((__m256i *)(out+160), AO);                  \
        A1 = _mm256_lddqu_si256((const __m256i *)(array_1 + 192));            \
        A2 = _mm256_lddqu_si256((const __m256i *)(array_2 + 192));            \
        AO = avx_intrinsic(A2, A1);                                     \
        _mm256_storeu_si256((__m256i *)(out+192), AO);                  \
        A1 = _mm256_lddqu_si256((const __m256i *)(array_1 + 224));            \
        A2 = _mm256_lddqu_si256((const __m256i *)(array_2 + 224));            \
        AO = avx_intrinsic(A2, A1);                                     \
        _mm256_storeu_si256((__m256i *)(out+224), AO);                  \
        out+=256;                                                       \
        array_1 += 256;                                                 \
        array_2 += 256;                                                 \
    }                                                                   \
    dst->cardinality = BITSET_UNKNOWN_CARDINALITY;                      \
    return dst->cardinality;                                            \
}                                                                       \
/* next, a version that updates cardinality*/                           \
int bitset_container_##opname(const bitset_container_t *src_1,          \
                              const bitset_container_t *src_2,          \
                              bitset_container_t *dst) {                \
    const __m256i * __restrict__ array_1 = (const __m256i *) src_1->array; \
    const __m256i * __restrict__ array_2 = (const __m256i *) src_2->array; \
    __m256i *out = (__m256i *) dst->array;                              \
    dst->cardinality = (int32_t)avx2_harley_seal_popcount256andstore_##opname(array_2,\
    		array_1, out,BITSET_CONTAINER_SIZE_IN_WORDS / (WORDS_IN_AVX2_REG));\
    return dst->cardinality;                                            \
}                                                                       \
/* next, a version that just computes the cardinality*/                 \
int bitset_container_##opname##_justcard(const bitset_container_t *src_1, \
                              const bitset_container_t *src_2) {        \
    const __m256i * __restrict__ data1 = (const __m256i *) src_1->array; \
    const __m256i * __restrict__ data2 = (const __m256i *) src_2->array; \
    return (int)avx2_harley_seal_popcount256_##opname(data2,                \
    		data1, BITSET_CONTAINER_SIZE_IN_WORDS / (WORDS_IN_AVX2_REG));\
}

#elif defined(USENEON)

#define BITSET_CONTAINER_FN(opname, opsymbol, avx_intrinsic, neon_intrinsic)  \
int bitset_container_##opname(const bitset_container_t *src_1,                \
                              const bitset_container_t *src_2,                \
                              bitset_container_t *dst) {                      \
    const uint64_t * __restrict__ array_1 = src_1->array;                     \
    const uint64_t * __restrict__ array_2 = src_2->array;                     \
    uint64_t *out = dst->array;                                               \
    uint16x8_t n0 = vdupq_n_u16(0);                                           \
    uint16x8_t n1 = vdupq_n_u16(0);                                           \
    uint16x8_t n2 = vdupq_n_u16(0);                                           \
    uint16x8_t n3 = vdupq_n_u16(0);                                           \
    for (size_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i += 8) {          \
        uint64x2_t c0 = neon_intrinsic(vld1q_u64(&array_1[i + 0]),            \
                                       vld1q_u64(&array_2[i + 0]));           \
        n0 = vaddq_u16(n0, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c0))));   \
        vst1q_u64(&out[i + 0], c0);                                           \
        uint64x2_t c1 = neon_intrinsic(vld1q_u64(&array_1[i + 2]),            \
                                       vld1q_u64(&array_2[i + 2]));           \
        n1 = vaddq_u16(n1, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c1))));   \
        vst1q_u64(&out[i + 2], c1);                                           \
        uint64x2_t c2 = neon_intrinsic(vld1q_u64(&array_1[i + 4]),            \
                                       vld1q_u64(&array_2[i + 4]));           \
        n2 = vaddq_u16(n2, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c2))));   \
        vst1q_u64(&out[i + 4], c2);                                           \
        uint64x2_t c3 = neon_intrinsic(vld1q_u64(&array_1[i + 6]),            \
                                       vld1q_u64(&array_2[i + 6]));           \
        n3 = vaddq_u16(n3, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c3))));   \
        vst1q_u64(&out[i + 6], c3);                                           \
    }                                                                         \
    uint64x2_t n = vdupq_n_u64(0);                                            \
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n0)));                           \
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n1)));                           \
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n2)));                           \
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n3)));                           \
    dst->cardinality = vgetq_lane_u64(n, 0) + vgetq_lane_u64(n, 1);           \
    return dst->cardinality;                                                  \
}                                                                             \
int bitset_container_##opname##_nocard(const bitset_container_t *src_1,       \
                                       const bitset_container_t *src_2,       \
                                             bitset_container_t *dst) {       \
    const uint64_t * __restrict__ array_1 = src_1->array;                     \
    const uint64_t * __restrict__ array_2 = src_2->array;                     \
    uint64_t *out = dst->array;                                               \
    for (size_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i += 8) {          \
        vst1q_u64(&out[i + 0], neon_intrinsic(vld1q_u64(&array_1[i + 0]),     \
                                              vld1q_u64(&array_2[i + 0])));   \
        vst1q_u64(&out[i + 2], neon_intrinsic(vld1q_u64(&array_1[i + 2]),     \
                                              vld1q_u64(&array_2[i + 2])));   \
        vst1q_u64(&out[i + 4], neon_intrinsic(vld1q_u64(&array_1[i + 4]),     \
                                              vld1q_u64(&array_2[i + 4])));   \
        vst1q_u64(&out[i + 6], neon_intrinsic(vld1q_u64(&array_1[i + 6]),     \
                                              vld1q_u64(&array_2[i + 6])));   \
    }                                                                         \
    dst->cardinality = BITSET_UNKNOWN_CARDINALITY;                            \
    return dst->cardinality;                                                  \
}                                                                             \
int bitset_container_##opname##_justcard(const bitset_container_t *src_1,     \
                                         const bitset_container_t *src_2) {   \
    const uint64_t * __restrict__ array_1 = src_1->array;                     \
    const uint64_t * __restrict__ array_2 = src_2->array;                     \
    uint16x8_t n0 = vdupq_n_u16(0);                                           \
    uint16x8_t n1 = vdupq_n_u16(0);                                           \
    uint16x8_t n2 = vdupq_n_u16(0);                                           \
    uint16x8_t n3 = vdupq_n_u16(0);                                           \
    for (size_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i += 8) {          \
        uint64x2_t c0 = neon_intrinsic(vld1q_u64(&array_1[i + 0]),            \
                                       vld1q_u64(&array_2[i + 0]));           \
        n0 = vaddq_u16(n0, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c0))));   \
        uint64x2_t c1 = neon_intrinsic(vld1q_u64(&array_1[i + 2]),            \
                                       vld1q_u64(&array_2[i + 2]));           \
        n1 = vaddq_u16(n1, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c1))));   \
        uint64x2_t c2 = neon_intrinsic(vld1q_u64(&array_1[i + 4]),            \
                                       vld1q_u64(&array_2[i + 4]));           \
        n2 = vaddq_u16(n2, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c2))));   \
        uint64x2_t c3 = neon_intrinsic(vld1q_u64(&array_1[i + 6]),            \
                                       vld1q_u64(&array_2[i + 6]));           \
        n3 = vaddq_u16(n3, vpaddlq_u8(vcntq_u8(vreinterpretq_u8_u64(c3))));   \
    }                                                                         \
    uint64x2_t n = vdupq_n_u64(0);                                            \
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n0)));                           \
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n1)));                           \
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n2)));                           \
    n = vaddq_u64(n, vpaddlq_u32(vpaddlq_u16(n3)));                           \
    return vgetq_lane_u64(n, 0) + vgetq_lane_u64(n, 1);                       \
}

#else /* not USEAVX  */

#define BITSET_CONTAINER_FN(opname, opsymbol, avx_intrinsic, neon_intrinsic)  \
int bitset_container_##opname(const bitset_container_t *src_1,            \
                              const bitset_container_t *src_2,            \
                              bitset_container_t *dst) {                  \
    const uint64_t * __restrict__ array_1 = src_1->array;                 \
    const uint64_t * __restrict__ array_2 = src_2->array;                 \
    uint64_t *out = dst->array;                                           \
    int32_t sum = 0;                                                      \
    for (size_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i += 2) {      \
        const uint64_t word_1 = (array_1[i])opsymbol(array_2[i]),         \
                       word_2 = (array_1[i + 1])opsymbol(array_2[i + 1]); \
        out[i] = word_1;                                                  \
        out[i + 1] = word_2;                                              \
        sum += hamming(word_1);                                    \
        sum += hamming(word_2);                                    \
    }                                                                     \
    dst->cardinality = sum;                                               \
    return dst->cardinality;                                              \
}                                                                         \
int bitset_container_##opname##_nocard(const bitset_container_t *src_1,   \
                                       const bitset_container_t *src_2,   \
                                       bitset_container_t *dst) {         \
    const uint64_t * __restrict__ array_1 = src_1->array;                 \
    const uint64_t * __restrict__ array_2 = src_2->array;                 \
    uint64_t *out = dst->array;                                           \
    for (size_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i++) {         \
        out[i] = (array_1[i])opsymbol(array_2[i]);                        \
    }                                                                     \
    dst->cardinality = BITSET_UNKNOWN_CARDINALITY;                        \
    return dst->cardinality;                                              \
}                                                                         \
int bitset_container_##opname##_justcard(const bitset_container_t *src_1, \
                              const bitset_container_t *src_2) {          \
    const uint64_t * __restrict__ array_1 = src_1->array;                 \
    const uint64_t * __restrict__ array_2 = src_2->array;                 \
    int32_t sum = 0;                                                      \
    for (size_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i += 2) {      \
        const uint64_t word_1 = (array_1[i])opsymbol(array_2[i]),         \
                       word_2 = (array_1[i + 1])opsymbol(array_2[i + 1]); \
        sum += hamming(word_1);                                    \
        sum += hamming(word_2);                                    \
    }                                                                     \
    return sum;                                                           \
}

#endif

// we duplicate the function because other containers use the "or" term, makes API more consistent
BITSET_CONTAINER_FN(or,    |, _mm256_or_si256, vorrq_u64)
BITSET_CONTAINER_FN(union, |, _mm256_or_si256, vorrq_u64)

// we duplicate the function because other containers use the "intersection" term, makes API more consistent
BITSET_CONTAINER_FN(and,          &, _mm256_and_si256, vandq_u64)
BITSET_CONTAINER_FN(intersection, &, _mm256_and_si256, vandq_u64)

BITSET_CONTAINER_FN(xor,    ^,  _mm256_xor_si256,    veorq_u64)
BITSET_CONTAINER_FN(andnot, &~, _mm256_andnot_si256, vbicq_u64)
// clang-format On



int bitset_container_to_uint32_array( void *vout, const bitset_container_t *cont, uint32_t base) {
#ifdef USEAVX2FORDECODING
	if(cont->cardinality >= 8192)// heuristic
		return (int) bitset_extract_setbits_avx2(cont->array, BITSET_CONTAINER_SIZE_IN_WORDS, vout,cont->cardinality,base);
	else
		return (int) bitset_extract_setbits(cont->array, BITSET_CONTAINER_SIZE_IN_WORDS, vout,base);
#else
	return (int) bitset_extract_setbits(cont->array, BITSET_CONTAINER_SIZE_IN_WORDS, vout,base);
#endif
}

/*
 * Print this container using printf (useful for debugging).
 */
void bitset_container_printf(const bitset_container_t * v) {
	printf("{");
	uint32_t base = 0;
	bool iamfirst = true;// TODO: rework so that this is not necessary yet still readable
	for (int i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; ++i) {
		uint64_t w = v->array[i];
		while (w != 0) {
			uint64_t t = w & (~w + 1);
			int r = __builtin_ctzll(w);
			if(iamfirst) {// predicted to be false
				printf("%u",base + r);
				iamfirst = false;
			} else {
				printf(",%u",base + r);
			}
			w ^= t;
		}
		base += 64;
	}
	printf("}");
}


/*
 * Print this container using printf as a comma-separated list of 32-bit integers starting at base.
 */
void bitset_container_printf_as_uint32_array(const bitset_container_t * v, uint32_t base) {
	bool iamfirst = true;// TODO: rework so that this is not necessary yet still readable
	for (int i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; ++i) {
		uint64_t w = v->array[i];
		while (w != 0) {
			uint64_t t = w & (~w + 1);
			int r = __builtin_ctzll(w);
			if(iamfirst) {// predicted to be false
				printf("%u", r + base);
				iamfirst = false;
			} else {
				printf(",%u",r + base);
			}
			w ^= t;
		}
		base += 64;
	}
}


// TODO: use the fast lower bound, also
int bitset_container_number_of_runs(bitset_container_t *b) {
  int num_runs = 0;
  uint64_t next_word = b->array[0];

  for (int i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS-1; ++i) {
    uint64_t word = next_word;
    next_word = b->array[i+1];
    num_runs += hamming((~word) & (word << 1)) + ( (word >> 63) & ~next_word);
  }

  uint64_t word = next_word;
  num_runs += hamming((~word) & (word << 1));
  if((word & 0x8000000000000000ULL) != 0)
    num_runs++;
  return num_runs;
}

int32_t bitset_container_serialize(const bitset_container_t *container, char *buf) {
  int32_t l = sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS;
  memcpy(buf, container->array, l);
  return(l);
}



int32_t bitset_container_write(const bitset_container_t *container,
                                  char *buf) {
	memcpy(buf, container->array, BITSET_CONTAINER_SIZE_IN_WORDS * sizeof(uint64_t));
	return bitset_container_size_in_bytes(container);
}


int32_t bitset_container_read(int32_t cardinality, bitset_container_t *container,
		const char *buf)  {
	container->cardinality = cardinality;
	memcpy(container->array, buf, BITSET_CONTAINER_SIZE_IN_WORDS * sizeof(uint64_t));
	return bitset_container_size_in_bytes(container);
}

uint32_t bitset_container_serialization_len() {
  return(sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
}

void* bitset_container_deserialize(const char *buf, size_t buf_len) {
  bitset_container_t *ptr;
  size_t l = sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS;

  if(l != buf_len)
    return(NULL);

  if((ptr = (bitset_container_t *)malloc(sizeof(bitset_container_t))) != NULL) {
    memcpy(ptr, buf, sizeof(bitset_container_t));
    // sizeof(__m256i) == 32
    ptr->array = (uint64_t *) roaring_bitmap_aligned_malloc(32, l);
    if (! ptr->array) {
        free(ptr);
        return NULL;
    }
    memcpy(ptr->array, buf, l);
    ptr->cardinality = bitset_container_compute_cardinality(ptr);
  }

  return((void*)ptr);
}

bool bitset_container_iterate(const bitset_container_t *cont, uint32_t base, roaring_iterator iterator, void *ptr) {
  for (int32_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; ++i ) {
    uint64_t w = cont->array[i];
    while (w != 0) {
      uint64_t t = w & (~w + 1);
      int r = __builtin_ctzll(w);
      if(!iterator(r + base, ptr)) return false;
      w ^= t;
    }
    base += 64;
  }
  return true;
}

bool bitset_container_iterate64(const bitset_container_t *cont, uint32_t base, roaring_iterator64 iterator, uint64_t high_bits, void *ptr) {
  for (int32_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; ++i ) {
    uint64_t w = cont->array[i];
    while (w != 0) {
      uint64_t t = w & (~w + 1);
      int r = __builtin_ctzll(w);
      if(!iterator(high_bits | (uint64_t)(r + base), ptr)) return false;
      w ^= t;
    }
    base += 64;
  }
  return true;
}


bool bitset_container_equals(const bitset_container_t *container1, const bitset_container_t *container2) {
	if((container1->cardinality != BITSET_UNKNOWN_CARDINALITY) && (container2->cardinality != BITSET_UNKNOWN_CARDINALITY)) {
		if(container1->cardinality != container2->cardinality) {
			return false;
		}
    if (container1->cardinality == INT32_C(0x10000)) {
        return true;
    }
	}
#ifdef USEAVX
  const __m256i *ptr1 = (const __m256i*)container1->array;
  const __m256i *ptr2 = (const __m256i*)container2->array;
  for (size_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS*sizeof(uint64_t)/32; i++) {
      __m256i r1 = _mm256_load_si256(ptr1+i);
      __m256i r2 = _mm256_load_si256(ptr2+i);
      int mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(r1, r2));
      if ((uint32_t)mask != UINT32_MAX) {
          return false;
      }
  }
#else
  return memcmp(container1->array,
                container2->array,
                BITSET_CONTAINER_SIZE_IN_WORDS*sizeof(uint64_t)) == 0;
#endif
	return true;
}

bool bitset_container_is_subset(const bitset_container_t *container1,
                          const bitset_container_t *container2) {
    if((container1->cardinality != BITSET_UNKNOWN_CARDINALITY) && (container2->cardinality != BITSET_UNKNOWN_CARDINALITY)) {
        if(container1->cardinality > container2->cardinality) {
            return false;
        }
    }
    for(int32_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; ++i ) {
		if((container1->array[i] & container2->array[i]) != container1->array[i]) {
			return false;
		}
	}
	return true;
}

bool bitset_container_select(const bitset_container_t *container, uint32_t *start_rank, uint32_t rank, uint32_t *element) {
    int card = bitset_container_cardinality(container);
    if(rank >= *start_rank + card) {
        *start_rank += card;
        return false;
    }
    const uint64_t *array = container->array;
    int32_t size;
    for (int i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; i += 1) {
        size = hamming(array[i]);
        if(rank <= *start_rank + size) {
            uint64_t w = container->array[i];
            uint16_t base = i*64;
            while (w != 0) {
                uint64_t t = w & (~w + 1);
                int r = __builtin_ctzll(w);
                if(*start_rank == rank) {
                    *element = r+base;
                    return true;
                }
                w ^= t;
                *start_rank += 1;
            }
        }
        else
            *start_rank += size;
    }
    assert(false);
    __builtin_unreachable();
}


/* Returns the smallest value (assumes not empty) */
uint16_t bitset_container_minimum(const bitset_container_t *container) {
  for (int32_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; ++i ) {
    uint64_t w = container->array[i];
    if (w != 0) {
      int r = __builtin_ctzll(w);
      return r + i * 64;
    }
  }
  return UINT16_MAX;
}

/* Returns the largest value (assumes not empty) */
uint16_t bitset_container_maximum(const bitset_container_t *container) {
  for (int32_t i = BITSET_CONTAINER_SIZE_IN_WORDS - 1; i > 0; --i ) {
    uint64_t w = container->array[i];
    if (w != 0) {
      int r = __builtin_clzll(w);
      return i * 64 + 63  - r;
    }
  }
  return 0;
}

/* Returns the number of values equal or smaller than x */
int bitset_container_rank(const bitset_container_t *container, uint16_t x) {
  // credit: aqrit
  int sum = 0;
  int i = 0;
  for (int end = x / 64; i < end; i++){
    sum += hamming(container->array[i]);
  }
  uint64_t lastword = container->array[i];
  uint64_t lastpos = UINT64_C(1) << (x % 64);
  uint64_t mask = lastpos + lastpos - 1; // smear right
  sum += hamming(lastword & mask);
  return sum;
}

/* Returns the index of the first value equal or larger than x, or -1 */
int bitset_container_index_equalorlarger(const bitset_container_t *container, uint16_t x) {
  uint32_t x32 = x;
  uint32_t k = x32 / 64;
  uint64_t word = container->array[k];
  const int diff = x32 - k * 64; // in [0,64)
  word = (word >> diff) << diff; // a mask is faster, but we don't care
  while(word == 0) {
    k++;
    if(k == BITSET_CONTAINER_SIZE_IN_WORDS) return -1;
    word = container->array[k];
  }
  return k * 64 + __builtin_ctzll(word);
}
