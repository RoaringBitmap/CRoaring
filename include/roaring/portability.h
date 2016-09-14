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

#include <stdlib.h> // will provide posix_memalign with _POSIX_C_SOURCE as defined above
#include <stdint.h>
#include <stdbool.h>
#ifndef __APPLE__
#include <malloc.h> // this should never be needed but there are some reports that it is needed.
#endif

#if __SIZEOF_LONG_LONG__ != 8
#error This code assumes  64-bit long longs (by use of the GCC intrinsics). Your system is not currently supported.
#endif


#if defined(_MSC_VER)
#define __restrict__ __restrict
#endif

#ifndef DISABLE_X64 // some users may want to compile as if they did not have an x64 processor

// unless DISABLEAVX was defined, if we have AVX2 and BMI2, we enable AVX
#if (!defined(USEAVX)) && (!defined(DISABLEAVX)) && (defined(__AVX2__)) && \
    (defined(__BMI2__))
#define USEAVX
#endif

// if USEAVX was somehow defined and we lack either AVX2 or BMI2, we disable it
#if defined(USEAVX) && ((!defined(__AVX2__)) || (!defined(__BMI2__)))
#undef USEAVX
#endif

#if defined(USEAVX) || defined(__x86_64__) || defined(_M_X64)
// we have an x64 processor
#define IS_X64
// we include the intrinsic header
#ifdef _MSC_VER
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
#else
/* Pretty much anything else. */
#include <x86intrin.h> // on some recent GCC, this will declare posix_memalign
#endif
#endif

// if we have AVX, then we use BMI optimizations
#ifdef USEAVX
#define USE_BMI  // we assume that AVX2 and BMI go hand and hand
#define USEAVX2FORDECODING            // optimization
#define ROARING_VECTOR_UNION_ENABLED  // vector unions (optimization)
#endif

#endif // DISABLE_X64

// without the following, we get lots of warnings about posix_memalign
#ifndef __cplusplus
extern int posix_memalign(void **__memptr, size_t __alignment, size_t __size);
#endif //__cplusplus // C++ does not have a well defined signature

// portable version of  posix_memalign
static inline void * aligned_malloc(size_t alignment, size_t size) {
	void *p;
#ifdef _MSC_VER
	p = _aligned_malloc(size, alignment);
#elif defined(__MINGW32__) || defined(__MINGW64__)
	p = __mingw_aligned_malloc(size, alignment);
#else
        // somehow, if this is used before including "x86intrin.h", it creates an implicit defined warning.
	if (posix_memalign(&p, alignment, size) != 0)
		return NULL;
#endif
	return p;
}

static inline void aligned_free(void *  memblock) {
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
#if defined(IS_X64) && defined(__POPCNT__)
    return _mm_popcnt_u64(x);
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
