
#ifndef UTIL_H
 
#define UTIL_H

int32_t binarySearch(uint16_t* source, int32_t n, uint16_t target);


int32_t advanceUntil(uint16_t * array, int32_t pos, int32_t length, uint16_t min);


/*
 * Set all bits in indexes [begin,end) to true.
 */
void bitset_set_range(uint64_t *bitmap, uint32_t start, uint32_t end);

#endif
