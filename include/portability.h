/*
 * portability.h
 *
 */

#ifndef INCLUDE_PORTABILITY_H_
#define INCLUDE_PORTABILITY_H_

#include <stdint.h>

#if __SIZEOF_LONG_LONG__ != 8
#error This code assumes  64-bit long longs (by use of the GCC intrinsics). Your system is not currently supported.
#endif

#if defined(USEAVX) && ((!defined(__AVX2__)) || (!defined(__BMI2__)))
#undef USEAVX
#endif

#if defined(USEAVX) || defined(__x86_64__) || defined(_M_X64)
#define IS_X64
#include <x86intrin.h>
#endif

#if defined(USEAVX)
#define USE_BMI //we assume that AVX2 and BMI go hand and hand
#endif

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
  return __builtin_popcountll(x);
#endif
}

#ifndef UINT64_C
#define UINT64_C(c) (c ## ULL)
#endif

#ifndef UINT32_C
#define UINT32_C(c) (c ## UL)
#endif

#endif /* INCLUDE_PORTABILITY_H_ */
