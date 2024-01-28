/*
 * random.h
 *
 */

#ifndef BENCHMARKS_RANDOM_H_
#define BENCHMARKS_RANDOM_H_

#include <stdlib.h>

struct pcg_state_setseq_64 {  // Internals are *Private*.
    uint64_t state;           // RNG state.  All values are possible.
    uint64_t inc;             // Controls which RNG sequence (stream) is
                              // selected. Must *always* be odd.
};
typedef struct pcg_state_setseq_64 pcg32_random_t;

static pcg32_random_t pcg32_global = {0x853c49e6748fea9bULL,
                                      0xda3e39cb94b95bdbULL};

static inline uint32_t pcg32_random_r(pcg32_random_t *rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((0 - rot) & 31));
}

static inline uint32_t pcg32_random() { return pcg32_random_r(&pcg32_global); }

static inline uint32_t ranged_random(uint32_t range) {
    uint64_t random32bit, multiresult;
    uint32_t leftover;
    uint32_t threshold;
    random32bit = pcg32_random();
    if ((range & (range - 1)) == 0) {
        return random32bit & (range - 1);
    }
    if (range > 0x80000000) {  // if range > 1<<31
        while (random32bit >= range) {
            random32bit = pcg32_random();
        }
        return (uint32_t)random32bit;  // [0, range)
    }
    multiresult = random32bit * range;
    leftover = (uint32_t)multiresult;
    if (leftover < range) {
        threshold = 0xFFFFFFFF % range;
        while (leftover <= threshold) {
            random32bit = pcg32_random();
            multiresult = random32bit * range;
            leftover = (uint32_t)multiresult;
        }
    }
    return multiresult >> 32;  // [0, range)
}

// Fisher-Yates shuffle, shuffling an array of integers
static inline void shuffle_uint16(uint16_t *storage, uint32_t size) {
    uint32_t i;
    for (i = size; i > 1; i--) {
        uint32_t nextpos = ranged_random(i);
        uint16_t tmp = storage[i - 1];    // likely in cache
        uint16_t val = storage[nextpos];  // could be costly
        storage[i - 1] = val;
        storage[nextpos] = tmp;  // you might have to read this store later
    }
}

// Fisher-Yates shuffle, shuffling an array of integers
static inline void shuffle_uint32(uint32_t *storage, uint32_t size) {
    uint32_t i;
    for (i = size; i > 1; i--) {
        uint32_t nextpos = ranged_random(i);
        uint32_t tmp = storage[i - 1];    // likely in cache
        uint32_t val = storage[nextpos];  // could be costly
        storage[i - 1] = val;
        storage[nextpos] = tmp;  // you might have to read this store later
    }
}

#endif /* BENCHMARKS_RANDOM_H_ */
