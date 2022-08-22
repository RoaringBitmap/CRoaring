#include <roaring/portability.h>
#include <roaring/containers/run.h>
#include <roaring/misc/configreport.h>
#include "benchmark.h"
#include "random.h"

enum { TESTSIZE = 2048 };

#if defined(CROARING_IS_X64) && !(defined(_MSC_VER) && !defined(__clang__))
// flushes the array from cache
void run_cache_flush(run_container_t* B) {
    const int32_t CACHELINESIZE =
        computecacheline();  // 64 bytes per cache line
    for (int32_t k = 0; k < B->n_runs * 2;
         k += CACHELINESIZE / (int32_t)sizeof(uint16_t)) {
        __builtin_ia32_clflush(B->runs + k);
    }
}
#else
void run_cache_flush(run_container_t* B) { (void)B; }
#endif

// tries to put array in cache
void run_cache_prefetch(run_container_t* B) {
#if !CROARING_REGULAR_VISUAL_STUDIO
#ifdef CROARING_IS_X64
    const int32_t CACHELINESIZE =
        computecacheline();  // 64 bytes per cache line
#else
    const int32_t CACHELINESIZE = 64;
#endif
    for (int32_t k = 0; k < B->n_runs * 2;
         k += CACHELINESIZE / (int32_t)sizeof(uint16_t)) {
        __builtin_prefetch(B->runs + k);
    }
#endif // !CROARING_REGULAR_VISUAL_STUDIO
}

int add_test(run_container_t* B) {
    int x;
    for (x = 0; x < (1 << 16); x += 3) {
        run_container_add(B, (uint16_t)x);
    }
    return 0;
}

int remove_test(run_container_t* B) {
    int x;
    for (x = 0; x < (1 << 16); x += 3) {
        run_container_remove(B, (uint16_t)x);
    }
    return 0;
}

int contains_test(run_container_t* B) {
    int card = 0;
    int x;
    for (x = 0; x < (1 << 16); x++) {
        card += run_container_contains(B, (uint16_t)x);
    }
    return card;
}

int union_test(run_container_t* B1, run_container_t* B2, run_container_t* BO) {
    run_container_union(B1, B2, BO);
    return run_container_cardinality(BO);
}

int intersection_test(run_container_t* B1, run_container_t* B2,
                      run_container_t* BO) {
    run_container_intersection(B1, B2, BO);
    return run_container_cardinality(BO);
}

int main() {
    int repeat = 500;
    int size = TESTSIZE;
    tellmeall();
    printf("run container benchmarks\n");
    run_container_t* B = run_container_create();
    BEST_TIME(add_test(B), 0, repeat, size);
    int answer = contains_test(B);
    size = 1 << 16;
    BEST_TIME(contains_test(B), answer, repeat, size);
    size = (1 << 16) / 3;
    BEST_TIME(remove_test(B), 0, repeat, size);
    run_container_free(B);

    for (int howmany = 32; howmany <= (1 << 16); howmany *= 8) {
        run_container_t* Bt = run_container_create();
        for (int j = 0; j < howmany; ++j) {
            run_container_add(Bt, (uint16_t)pcg32_random());
        }
        size_t nbrtestvalues = 1024;
        uint16_t* testvalues = malloc(nbrtestvalues * sizeof(uint16_t));
        printf("\n number of values in container = %d\n",
               run_container_cardinality(Bt));
        int card = run_container_cardinality(Bt);
        uint32_t* out = malloc(sizeof(uint32_t) * (unsigned long)card);
        BEST_TIME(run_container_to_uint32_array(out, Bt, 1234), card, repeat,
                  card);
        free(out);
        BEST_TIME_PRE_ARRAY(Bt, run_container_contains, run_cache_prefetch,
                            testvalues, nbrtestvalues);
        BEST_TIME_PRE_ARRAY(Bt, run_container_contains, run_cache_flush,
                            testvalues, nbrtestvalues);
        free(testvalues);
        run_container_free(Bt);
    }
    printf("\n");

    run_container_t* B1 = run_container_create();
    for (int x = 0; x < 1 << 16; x += 3) {
        run_container_add(B1, (uint16_t)x);
    }
    run_container_t* B2 = run_container_create();
    for (int x = 0; x < 1 << 16; x += 5) {
        run_container_add(B2, (uint16_t)x);
    }
    int32_t inputsize;
    inputsize = B1->n_runs + B2->n_runs;
    run_container_t* BO = run_container_create();
    printf("\nUnion and intersections...\n");
    printf("\nNote:\n");
    printf(
        "union times are expressed in cycles per number of input elements "
        "(both runs)\n");
    printf(
        "intersection times are expressed in cycles per number of output "
        "elements\n\n");
    printf("==intersection and union test 1 \n");
    printf("input 1 cardinality = %d, input 2 cardinality = %d \n",
           run_container_cardinality(B1), run_container_cardinality(B2));
    answer = union_test(B1, B2, BO);
    printf("union cardinality = %d \n", answer);
    printf("B1 card = %d B2 card = %d \n", run_container_cardinality(B1),
           run_container_cardinality(B2));
    inputsize = B1->n_runs + B2->n_runs;
    BEST_TIME(union_test(B1, B2, BO), answer, repeat, inputsize);
    answer = intersection_test(B1, B2, BO);
    printf("intersection cardinality = %d \n", answer);
    BEST_TIME(intersection_test(B1, B2, BO), answer, repeat, answer);
    printf("==intersection and union test 2 \n");
    run_container_clear(B1);
    run_container_clear(B2);
    for (int x = 0; x < (1 << 16); x += 64) {
        int length = x % 11;
        for (int y = 0; y < length; ++y)
            run_container_add(B1, (uint16_t)(x + y));
    }
    for (int x = 1; x < 1 << 16; x += x) {
        run_container_add(B2, (uint16_t)x);
    }
    printf("input 1 cardinality = %d, input 2 cardinality = %d \n",
           run_container_cardinality(B1), run_container_cardinality(B2));
    answer = union_test(B1, B2, BO);
    printf("union cardinality = %d \n", answer);
    printf("B1 card = %d B2 card = %d \n", run_container_cardinality(B1),
           run_container_cardinality(B2));
    inputsize = B1->n_runs + B2->n_runs;

    BEST_TIME(union_test(B1, B2, BO), answer, repeat, inputsize);
    answer = intersection_test(B1, B2, BO);
    printf("intersection cardinality = %d \n", answer);
    BEST_TIME(intersection_test(B1, B2, BO), answer, repeat, answer);

    run_container_free(B1);
    run_container_free(B2);
    run_container_free(BO);
    return 0;
}
