#include <roaring/portability.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/convert.h>
#include <roaring/misc/configreport.h>

#include "benchmark.h"
#include "random.h"

#define DIV_CEIL_64K(denom) (((1 << 16) + ((denom) - 1)) / (denom))

const int repeat = 500;


#if defined(CROARING_IS_X64) && !(defined(_MSC_VER) && !defined(__clang__))
// flushes the array of words from cache
void bitset_cache_flush(bitset_container_t* B) {
    const int32_t CACHELINESIZE =
        computecacheline();  // 64 bytes per cache line
    for (int32_t k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS;
         k += CACHELINESIZE / (int32_t)sizeof(uint64_t)) {
        __builtin_ia32_clflush(B->words + k);
    }
}
#else
void bitset_cache_flush(bitset_container_t* B) { (void)B; }

#endif

// tries to put array of words in cache
void bitset_cache_prefetch(bitset_container_t* B) {
#if !CROARING_REGULAR_VISUAL_STUDIO
#ifdef CROARING_IS_X64
    const int32_t CACHELINESIZE =
        computecacheline();  // 64 bytes per cache line
#else
    const int32_t CACHELINESIZE = 64;
#endif
    for (int32_t k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS;
         k += CACHELINESIZE / (int32_t)sizeof(uint64_t)) {
        __builtin_prefetch(B->words + k);
    }
#endif // !CROARING_REGULAR_VISUAL_STUDIO
}

// used to benchmark array_container_from_bitset
int get_cardinality_through_conversion_to_array(bitset_container_t* B) {
    array_container_t* conv = array_container_from_bitset(B);
    int card = conv->cardinality;
    array_container_free(conv);
    return card;
}

int extract_test(bitset_container_t* B) {
    int card = bitset_container_cardinality(B);
    uint32_t* out = malloc(sizeof(uint32_t) * (unsigned)card);
    bitset_container_to_uint32_array(out, B, 1234);
    free(out);
    return card;
}

int set_test(bitset_container_t* B) {
    int x;
    for (x = 0; x < 1 << 16; x += 3) {
        bitset_container_set(B, (uint16_t)x);
    }
    return 0;
}

int unset_test(bitset_container_t* B) {
    int x;
    for (x = 0; x < 1 << 16; x += 3) {
        bitset_container_remove(B, (uint16_t)x);
    }
    return 0;
}

int get_test(bitset_container_t* B) {
    int card = 0;
    int x;
    for (x = 0; x < 1 << 16; x++) {
        card += bitset_container_get(B, (uint16_t)x);
    }
    return card;
}

void benchmark_logical_operations() {
    printf("\nLogical operations (time units per single operation):\n");
    bitset_container_t* B1 = bitset_container_create();
    for (int x = 0; x < 1 << 16; x += 3) {
        bitset_container_set(B1, (uint16_t)x);
    }
    bitset_container_t* B2 = bitset_container_create();
    for (int x = 0; x < 1 << 16; x += 5) {
        bitset_container_set(B2, (uint16_t)x);
    }

    bitset_container_t* BO = bitset_container_create();

    const int and_cardinality = DIV_CEIL_64K(3*5);
    BEST_TIME(bitset_container_and_nocard(B1, B2, BO), BITSET_UNKNOWN_CARDINALITY, repeat, 1);
    BEST_TIME(bitset_container_and(B1, B2, BO), and_cardinality, repeat, 1);
    BEST_TIME(bitset_container_and_justcard(B1, B2), and_cardinality, repeat, 1);
    BEST_TIME(bitset_container_compute_cardinality(BO), and_cardinality, repeat, 1);

    const int or_cardinality = DIV_CEIL_64K(3) + DIV_CEIL_64K(5) - DIV_CEIL_64K(3*5);
    BEST_TIME(bitset_container_or_nocard(B1, B2, BO), BITSET_UNKNOWN_CARDINALITY, repeat, 1);
    BEST_TIME(bitset_container_or(B1, B2, BO), or_cardinality, repeat, 1);
    BEST_TIME(bitset_container_or_justcard(B1, B2), or_cardinality, repeat, 1);
    BEST_TIME(bitset_container_compute_cardinality(BO), or_cardinality, repeat, 1);

    bitset_container_free(BO);
    bitset_container_free(B1);
    bitset_container_free(B2);
    printf("\n");
}


int main() {
    int size = (1 << 16) / 3;
    tellmeall();
    printf("bitset container benchmarks\n");
    bitset_container_t* B = bitset_container_create();
    BEST_TIME(set_test(B), 0, repeat, size);
    int answer = get_test(B);
    size = 1 << 16;
    BEST_TIME(get_test(B), answer, repeat, size);
    BEST_TIME(bitset_container_cardinality(B), answer, repeat, 1);
    BEST_TIME(bitset_container_compute_cardinality(B), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);

    size = (1 << 16) / 3;
    BEST_TIME(unset_test(B), 0, repeat, size);
    bitset_container_free(B);

    for (int howmany = 4096; howmany <= (1 << 16); howmany *= 2) {
        bitset_container_t* Bt = bitset_container_create();
        while (bitset_container_cardinality(Bt) < howmany) {
            bitset_container_set(Bt, (uint16_t)pcg32_random());
        }
        size_t nbrtestvalues = 1024;
        uint16_t* testvalues = malloc(nbrtestvalues * sizeof(uint16_t));
        printf("\n number of values in container = %d\n",
               bitset_container_cardinality(Bt));
        int card = bitset_container_cardinality(Bt);
        uint32_t* out = malloc(sizeof(uint32_t) * (unsigned)card + 32);
        BEST_TIME(bitset_container_to_uint32_array(out, Bt, 1234), card, repeat,
                  card);
        free(out);
        BEST_TIME_PRE_ARRAY(Bt, bitset_container_get, bitset_cache_prefetch,
                            testvalues, nbrtestvalues);
        BEST_TIME_PRE_ARRAY(Bt, bitset_container_get, bitset_cache_flush,
                            testvalues, nbrtestvalues);
        free(testvalues);
        bitset_container_free(Bt);
    }
    printf("\n");

    benchmark_logical_operations();

    // next we are going to benchmark conversion from bitset to array (an
    // important step)
    bitset_container_t* B1 = bitset_container_create();
    for (int k = 0; k < 4096; ++k) {
        bitset_container_set(B1, (uint16_t)ranged_random(1 << 16));
    }
    answer = get_cardinality_through_conversion_to_array(B1);
    BEST_TIME(get_cardinality_through_conversion_to_array(B1), answer, repeat,
              BITSET_CONTAINER_SIZE_IN_WORDS);

    bitset_container_free(B1);
    return 0;
}
