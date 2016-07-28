#ifndef BITSET_UTIL_H
#define BITSET_UTIL_H

#include <stdint.h>

/*
 * Set all bits in indexes [begin,end) to true.
 */
static inline void bitset_set_range(uint64_t *bitmap, uint32_t start, uint32_t end) {
    if (start == end) return;
    uint32_t firstword = start / 64;
    uint32_t endword = (end - 1) / 64;
    if (firstword == endword) {
        bitmap[firstword] |= ((~UINT64_C(0)) << (start % 64)) &
                             ((~UINT64_C(0)) >> ((-end) % 64));
        return;
    }
    bitmap[firstword] |= (~UINT64_C(0)) << (start % 64);
    for (uint32_t i = firstword + 1; i < endword; i++) bitmap[i] = ~UINT64_C(0);
    bitmap[endword] |= (~UINT64_C(0)) >> ((-end) % 64);
}

/*
 * Set all bits in indexes [begin,begin+lenminusone] to true.
 */
static inline void bitset_set_lenrange(uint64_t *bitmap, uint32_t start, uint32_t lenminusone) {
    uint32_t firstword = start / 64;
    uint32_t endword = (start + lenminusone) / 64;
    if (firstword == endword) {
        bitmap[firstword] |= ((~UINT64_C(0)) >> ((63 - lenminusone)))
                             << (start % 64);
        return;
    }
    uint64_t temp = bitmap[endword];
    bitmap[firstword] |= (~UINT64_C(0)) << (start % 64);
    for (uint32_t i = firstword + 1; i < endword; i += 2)
        bitmap[i] = bitmap[i + 1] = ~UINT64_C(0);
    bitmap[endword] =
        temp | (~UINT64_C(0)) >> ((-start - lenminusone - 1) % 64);
}


/*
 * Flip all the bits in indexes [begin,end).
 */
static inline void bitset_flip_range(uint64_t *bitmap, uint32_t start, uint32_t end) {
    if (start == end) return;
    uint32_t firstword = start / 64;
    uint32_t endword = (end - 1) / 64;
    bitmap[firstword] ^= ~((~UINT64_C(0)) << (start % 64));
    for (uint32_t i = firstword; i < endword; i++) bitmap[i] = ~bitmap[i];
    bitmap[endword] ^= ((~UINT64_C(0)) >> ((-end) % 64));
}

/*
 * Set all bits in indexes [begin,end) to false.
 */
static inline void bitset_reset_range(uint64_t *bitmap, uint32_t start, uint32_t end) {
    if (start == end) return;
    uint32_t firstword = start / 64;
    uint32_t endword = (end - 1) / 64;
    if (firstword == endword) {
        bitmap[firstword] &= ~(((~UINT64_C(0)) << (start % 64)) &
                               ((~UINT64_C(0)) >> ((-end) % 64)));
        return;
    }
    bitmap[firstword] &= ~((~UINT64_C(0)) << (start % 64));
    for (uint32_t i = firstword + 1; i < endword; i++) bitmap[i] = UINT64_C(0);
    bitmap[endword] &= ~((~UINT64_C(0)) >> ((-end) % 64));
}

/*
 * Given a bitset containing "length" 64-bit words, write out the position
 * of all the set bits to "out", values start at "base".
 *
 * The "out" pointer should be sufficient to store the actual number of bits
 * set.
 *
 * Returns how many values were actually decoded.
 *
 * This function should only be expected to be faster than
 * bitset_extract_setbits
 * when the density of the bitset is high.
 *
 * This function uses AVX2 decoding.
 */
size_t bitset_extract_setbits_avx2(uint64_t *bitset, size_t length,
                                   uint32_t *out, size_t outcapacity,
                                   uint32_t base);

/*
 * Given a bitset containing "length" 64-bit words, write out the position
 * of all the set bits to "out", values start at "base".
 *
 * The "out" pointer should be sufficient to store the actual number of bits
 *set.
 *
 * Returns how many values were actually decoded.
 */
size_t bitset_extract_setbits(uint64_t *bitset, size_t length, uint32_t *out,
                              uint32_t base);

/*
 * Given a bitset containing "length" 64-bit words, write out the position
 * of all the set bits to "out" as 16-bit integers, values start at "base" (can
 *be set to zero)
 *
 * The "out" pointer should be sufficient to store the actual number of bits
 *set.
 *
 * Returns how many values were actually decoded.
 *
 * This function should only be expected to be faster than
 *bitset_extract_setbits_uint16
 * when the density of the bitset is high.
 *
 * This function uses SSE decoding.
 */
size_t bitset_extract_setbits_sse_uint16(const uint64_t *bitset, size_t length,
                                         uint16_t *out, size_t outcapacity,
                                         uint16_t base);

/*
 * Given a bitset containing "length" 64-bit words, write out the position
 * of all the set bits to "out",  values start at "base"
 * (can be set to zero)
 *
 * The "out" pointer should be sufficient to store the actual number of bits
 *set.
 *
 * Returns how many values were actually decoded.
 */
size_t bitset_extract_setbits_uint16(const uint64_t *bitset, size_t length,
                                     uint16_t *out, uint16_t base);

/*
 * Given two bitsets containing "length" 64-bit words, write out the position
 * of all the common set bits to "out", values start at "base"
 * (can be set to zero)
 *
 * The "out" pointer should be sufficient to store the actual number of bits
 * set.
 *
 * Returns how many values were actually decoded.
 */
size_t bitset_extract_intersection_setbits_uint16(const uint64_t *bitset1,
                                                  const uint64_t *bitset2,
                                                  size_t length, uint16_t *out,
                                                  uint16_t base);

/*
 * Given a bitset having cardinality card, set all bit values in the list (there
 * are length of them)
 * and return the updated cardinality. This evidently assumes that the bitset
 * already contained data.
 */
uint64_t bitset_set_list_withcard(void *bitset, uint64_t card,
                                  const uint16_t *list, uint64_t length);
/*
 * Given a bitset, set all bit values in the list (there
 * are length of them).
 */
void bitset_set_list(void *bitset, const uint16_t *list, uint64_t length);

/*
 * Given a bitset having cardinality card, unset all bit values in the list
 * (there are length of them)
 * and return the updated cardinality. This evidently assumes that the bitset
 * already contained data.
 */
uint64_t bitset_clear_list(void *bitset, uint64_t card, const uint16_t *list,
                           uint64_t length);

/*
 * Given a bitset having cardinality card, toggle all bit values in the list
 * (there are length of them)
 * and return the updated cardinality. This evidently assumes that the bitset
 * already contained data.
 */

uint64_t bitset_flip_list_withcard(void *bitset, uint64_t card,
                                   const uint16_t *list, uint64_t length);

void bitset_flip_list(void *bitset, const uint16_t *list, uint64_t length);

#endif


