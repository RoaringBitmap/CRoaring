#define _GNU_SOURCE
#include <roaring/roaring.h>

#include "benchmark.h"
#include "numbersfromtextfiles.h"

#define STARTBEST(numberoftests)                                          \
    {                                                                     \
        uint64_t min_diff = -1;                                           \
        uint64_t boguscyclesstart = 0;                                    \
        uint64_t boguscyclesend = 0;                                      \
        for (int bogustest = 0; bogustest < numberoftests; bogustest++) { \
            uint64_t cycles_diff = 0;                                     \
            RDTSC_START(boguscyclesstart);

#define ENDBEST(outputvar)                              \
    RDTSC_FINAL(boguscyclesend);                        \
    cycles_diff = (boguscyclesend - boguscyclesstart);  \
    if (cycles_diff < min_diff) min_diff = cycles_diff; \
    }                                                   \
    outputvar = min_diff;                               \
    }
/**
 * Once you have collected all the integers, build the bitmaps.
 */
static roaring_bitmap_t **create_all_bitmaps(size_t *howmany,
                                             uint32_t **numbers, size_t count,
                                             bool runoptimize,
                                             bool copy_on_write) {
    if (numbers == NULL) return NULL;
    printf("Constructing %d  bitmaps.\n", (int)count);
    roaring_bitmap_t **answer = malloc(sizeof(roaring_bitmap_t *) * count);
    for (size_t i = 0; i < count; i++) {
        printf(".");
        fflush(stdout);
        answer[i] = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
        if (runoptimize) roaring_bitmap_run_optimize(answer[i]);
        roaring_bitmap_shrink_to_fit(answer[i]);
        roaring_bitmap_set_copy_on_write(answer[i], copy_on_write);
    }
    printf("\n");
    return answer;
}

static void printusage(char *command) {
    printf(
        " Try %s directory \n where directory could be "
        "benchmarks/realdata/weather_sept_85\n",
        command);
    ;
}

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

int main(int argc, char **argv) {
    int c;
    const char *extension = ".txt";
    bool copy_on_write = false;
    bool runoptimize = true;
    while ((c = getopt(argc, argv, "e:h")) != -1) switch (c) {
            case 'e':
                extension = optarg;
                break;
            case 'h':
                printusage(argv[0]);
                return 0;
            default:
                abort();
        }
    if (optind >= argc) {
        printusage(argv[0]);
        return -1;
    }
    char *dirname = argv[optind];
    size_t count;

    size_t *howmany = NULL;
    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &howmany, &count);
    if (numbers == NULL) {
        printf(
            "I could not find or load any data file with extension %s in "
            "directory %s.\n",
            extension, dirname);
        return -1;
    }

    uint64_t cycles_start = 0, cycles_final = 0;

    RDTSC_START(cycles_start);
    roaring_bitmap_t **bitmaps =
        create_all_bitmaps(howmany, numbers, count, runoptimize, copy_on_write);
    RDTSC_FINAL(cycles_final);
    if (bitmaps == NULL) return -1;
    printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);

    printf("Creating %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);
    if (count == 0) return -1;
    uint32_t maxvalue = roaring_bitmap_maximum(bitmaps[0]);
    for (int i = 1; i < (int)count; i++) {
        uint32_t thismax = roaring_bitmap_maximum(bitmaps[0]);
        if (thismax > maxvalue) maxvalue = thismax;
    }
    const int quartile_test_repetitions = 1000;

    uint64_t quartcount;
    uint64_t cycles;
    STARTBEST(quartile_test_repetitions)
    quartcount = 0;
    for (size_t i = 0; i < count; ++i) {
        quartcount += roaring_bitmap_contains(bitmaps[i], maxvalue / 4);
        quartcount += roaring_bitmap_contains(bitmaps[i], maxvalue / 2);
        quartcount += roaring_bitmap_contains(bitmaps[i], 3 * maxvalue / 4);
    }
    ENDBEST(cycles)

    printf("Quartile queries on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles);

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
        roaring_bitmap_free(bitmaps[i]);
        bitmaps[i] = NULL;  // paranoid
    }
    free(bitmaps);
    free(howmany);
    free(numbers);

    return (int)quartcount;
}
