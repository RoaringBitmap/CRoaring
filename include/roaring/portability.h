/*
 * portability.h
 *
 */

#ifndef INCLUDE_PORTABILITY_H_
#define INCLUDE_PORTABILITY_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif

#if !(defined(_POSIX_C_SOURCE)) || (_POSIX_C_SOURCE < 200809L)
#define _POSIX_C_SOURCE 200809L
#endif
#if !(defined(_XOPEN_SOURCE)) || (_XOPEN_SOURCE < 700)
#define _XOPEN_SOURCE 700
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>  // will provide posix_memalign with _POSIX_C_SOURCE as defined above
#if !(defined(__APPLE__)) && !(defined(__FreeBSD__))
#include <malloc.h>  // this should never be needed but there are some reports that it is needed.
#endif


#if defined(_MSC_VER) && !defined(__clang__) && !defined(_WIN64)
#pragma message( \
    "You appear to be attempting a 32-bit build under Visual Studio. We recommend a 64-bit build instead.")
#endif

#if defined(__SIZEOF_LONG_LONG__) && __SIZEOF_LONG_LONG__ != 8
#error This code assumes  64-bit long longs (by use of the GCC intrinsics). Your system is not currently supported.
#endif

#if defined(_MSC_VER)
#define __restrict__ __restrict
#endif

#ifndef DISABLE_X64  // some users may want to compile as if they did not have
                     // an x64 processor

///////////////////////
/// We support X64 hardware in the following manner:
///
/// if IS_X64 is defined then we have at least SSE and SSE2
/// (All Intel processors sold in the recent past have at least SSE and SSE2 support,
/// going back to the Pentium 4.)
///
/// if USESSE4 is defined then we assume at least SSE4.2, SSE4.1,
///                   SSSE3, SSE3... + IS_X64
/// if USEAVX is defined, then we assume AVX2, AVX + USESSE4
///
/// So if you have hardware that supports AVX but not AVX2, then "USEAVX"
/// won't be enabled.
/// If you have hardware that supports SSE4.1, but not SSE4.2, then USESSE4
/// won't be defined.
//////////////////////

// unless DISABLEAVX was defined, if we have __AVX2__, we enable AVX
#if (!defined(USEAVX)) && (!defined(DISABLEAVX)) && (defined(__AVX2__))
#define USEAVX
#endif

// if we have __SSE4_2__, we enable SSE4
#if (defined(__POPCNT__)) && (defined(__SSE4_2__))
#define USESSE4
#endif

#if defined(USEAVX) || defined(__x86_64__) || defined(_M_X64)
// we have an x64 processor
#define IS_X64
// we include the intrinsic header
#ifndef _MSC_VER
/* Non-Microsoft C/C++-compatible compiler */
#include <x86intrin.h>  // on some recent GCC, this will declare posix_memalign
#endif
#endif

#ifndef _MSC_VER
/* Non-Microsoft C/C++-compatible compiler, assumes that it supports inline
 * assembly */
#define ROARING_INLINE_ASM
#endif

#ifdef USEAVX
#define USESSE4             // if we have AVX, then we have SSE4
#define USE_BMI             // we assume that AVX2 and BMI go hand and hand
#define USEAVX2FORDECODING  // optimization
// vector operations should work on not just AVX
#define ROARING_VECTOR_OPERATIONS_ENABLED  // vector unions (optimization)
#endif

#endif  // DISABLE_X64

#ifdef _MSC_VER
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>

#ifndef __clang__  // if one compiles with MSVC *with* clang, then these
                   // intrinsics are defined!!!
// sadly there is no way to check whether we are missing these intrinsics
// specifically.

/* wrappers for Visual Studio built-ins that look like gcc built-ins */
/* result might be undefined when input_num is zero */
static inline int __builtin_ctzll(unsigned long long input_num) {
    unsigned long index;
#ifdef _WIN64  // highly recommended!!!
    _BitScanForward64(&index, input_num);
#else  // if we must support 32-bit Windows
    if ((uint32_t)input_num != 0) {
        _BitScanForward(&index, (uint32_t)input_num);
    } else {
        _BitScanForward(&index, (uint32_t)(input_num >> 32));
        index += 32;
    }
#endif
    return index;
}

/* result might be undefined when input_num is zero */
static inline int __builtin_clzll(unsigned long long input_num) {
    unsigned long index;
#ifdef _WIN64  // highly recommended!!!
    _BitScanReverse64(&index, input_num);
#else  // if we must support 32-bit Windows
    if (input_num > 0xFFFFFFFF) {
        _BitScanReverse(&index, (uint32_t)(input_num >> 32));
        index += 32;
    } else {
        _BitScanReverse(&index, (uint32_t)(input_num));
    }
#endif
    return 63 - index;
}

/* result might be undefined when input_num is zero */
static inline int __builtin_popcountll(unsigned long long input_num) {
#ifdef _WIN64  // highly recommended!!!
    return (int)__popcnt64(input_num);
#else  // if we must support 32-bit Windows
    return (int)(__popcnt((uint32_t)input_num) +
                 __popcnt((uint32_t)(input_num >> 32)));
#endif
}

/* Use #define so this is effective even under /Ob0 (no inline) */
#define __builtin_unreachable() __assume(0)
#endif

#endif

// without the following, we get lots of warnings about posix_memalign
#ifndef __cplusplus
extern int posix_memalign(void **__memptr, size_t __alignment, size_t __size);
#endif  //__cplusplus // C++ does not have a well defined signature

// portable version of  posix_memalign
static inline void *aligned_malloc(size_t alignment, size_t size) {
    void *p;
#ifdef _MSC_VER
    p = _aligned_malloc(size, alignment);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    p = __mingw_aligned_malloc(size, alignment);
#else
    // somehow, if this is used before including "x86intrin.h", it creates an
    // implicit defined warning.
    if (posix_memalign(&p, alignment, size) != 0) return NULL;
#endif
    return p;
}

static inline void aligned_free(void *memblock) {
#ifdef _MSC_VER
    _aligned_free(memblock);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    __mingw_aligned_free(memblock);
#else
    free(memblock);
#endif
}

#if defined(_MSC_VER)
#define ALIGNED(x) __declspec(align(x))
#else
#if defined(__GNUC__)
#define ALIGNED(x) __attribute__((aligned(x)))
#endif
#endif

#ifdef __GNUC__
#define WARN_UNUSED __attribute__((warn_unused_result))
#else
#define WARN_UNUSED
#endif

#define IS_BIG_ENDIAN (*(uint16_t *)"\0\xff" < 0x100)

static inline int hamming(uint64_t x) {
#ifdef USESSE4
    return (int) _mm_popcnt_u64(x);
#else
    // won't work under visual studio, but hopeful we have _mm_popcnt_u64 in
    // many cases
    return __builtin_popcountll(x);
#endif
}

#ifndef UINT64_C
#define UINT64_C(c) (c##ULL)
#endif

#ifndef UINT32_C
#define UINT32_C(c) (c##UL)
#endif

#endif /* INCLUDE_PORTABILITY_H_ */
