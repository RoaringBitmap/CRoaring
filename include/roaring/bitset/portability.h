#ifndef CBITSET_PORTABILITY_H
#define CBITSET_PORTABILITY_H
#include <stdint.h>

// For compatibility with MSVC with the use of `restrict`
#if (__STDC_VERSION__ >= 199901L) || \
    (defined(__GNUC__) && defined(__STDC_VERSION__))
#define CBITSET_RESTRICT restrict
#else
#define CBITSET_RESTRICT
#endif  // (__STDC_VERSION__ >= 199901L) || (defined(__GNUC__) &&
        // defined(__STDC_VERSION__ ))

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
    if (input_num > 0xFFFFFFF) {
        _BitScanReverse(&index, (uint32_t)(input_num >> 32));
    } else {
        _BitScanReverse(&index, (uint32_t)(input_num));
        index += 32;
    }
#endif
    return 63 - index;
}

/* result might be undefined when input_num is zero */
static inline int __builtin_clz(int input_num) {
    unsigned long index;
    _BitScanReverse(&index, input_num);
    return 31 - index;
}

/* result might be undefined when input_num is zero */
static inline int __builtin_popcountll(unsigned long long input_num) {
#if defined(_M_ARM64) || defined(_M_ARM)
    input_num = input_num - ((input_num >> 1) & 0x5555555555555555);
    input_num = (input_num & 0x3333333333333333) +
                ((input_num >> 2) & 0x3333333333333333);
    input_num = ((input_num + (input_num >> 4)) & 0x0F0F0F0F0F0F0F0F);
    return (uint32_t)((input_num * (0x0101010101010101)) >> 56);
#elif defined(_WIN64)  // highly recommended!!!
    return (int)__popcnt64(input_num);
#else                  // if we must support 32-bit Windows
    return (int)(__popcnt((uint32_t)input_num) +
                 __popcnt((uint32_t)(input_num >> 32)));
#endif
}

static inline void __builtin_unreachable() { __assume(0); }
#endif
#endif
#endif
