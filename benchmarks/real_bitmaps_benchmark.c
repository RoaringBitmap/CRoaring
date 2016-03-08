#define _GNU_SOURCE
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "benchmark.h"
#include "roaring.h"

/**
 * Read the content of a file to a char array. Caller is
 * responsible for memory de-allocation.
 * Returns NULL on error.
 *
 * (If the individual files are small, this function is
 * a good idea.)
 */
static char *read_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Could not open file %s\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = (size_t)ftell(fp);
    rewind(fp);
    char *answer = malloc(size + 1);
    if (!answer) {
        fclose(fp);
        return NULL;
    }
    if (fread(answer, size, 1, fp) != 1) {
        free(answer);
        return NULL;
    }
    answer[size] = '\0';
    fclose(fp);
    return answer;
}

/**
 * Given a file made of comma-separated integers,
 * read it all and generate an array of integers.
 * The caller is responsible for memory de-allocation.
 */
static uint32_t *read_integer_file(char *filename, size_t *howmany) {
    char *buffer = read_file(filename);
    if (buffer == NULL) return NULL;

    size_t howmanyints = 1;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == ',') ++howmanyints;
    }

    uint32_t *answer = malloc(howmanyints * sizeof(uint32_t));
    if (answer == NULL) return NULL;
    size_t pos = 0;
    for (int i = 0; buffer[i] != '\0'; i++) {
        uint32_t currentint;
        while ((buffer[i] < '0') || (buffer[i] > '9')) {
            i++;
            if (buffer[i] == '\0') goto END;
        }
        currentint = (uint32_t)(buffer[i] - '0');
        i++;
        for (; (buffer[i] >= '0') && (buffer[i] <= '9'); i++)
            currentint = currentint * 10 + (uint32_t)(buffer[i] - '0');
        answer[pos++] = currentint;
    }
END:
    if (pos != howmanyints) {
        printf("unexpected number of integers! %d %d \n", (int)pos,
               (int)howmanyints);
    }
    *howmany = pos;
    free(buffer);
    return answer;
}

static bool hasExtension(char *filename, char *extension) {
    char *ext = strrchr(filename, '.');
    return (ext && !strcmp(ext, extension));
}

/**
 * read all (count) integer files in a directory. Caller is responsible
 * for memory de-allocation. In case of error, a NULL is returned.
 */
static uint32_t **read_all_integer_files(char *dirname, char *extension,
                                         size_t **howmany, size_t *count) {
    struct dirent **entry_list;

    int c = scandir(dirname, &entry_list, 0, alphasort);
    if (c < 0) return NULL;
    size_t truec = 0;
    for (int i = 0; i < c; i++) {
        if (hasExtension(entry_list[i]->d_name, extension)) ++truec;
    }
    *count = truec;
    *howmany = malloc(sizeof(size_t) * (*count));
    uint32_t **answer = malloc(sizeof(uint32_t *) * (*count));
    size_t dirlen = strlen(dirname);
    char *modifdirname = dirname;
    if (modifdirname[dirlen - 1] != '/') {
        modifdirname = malloc(dirlen + 2);
        strcpy(modifdirname, dirname);
        modifdirname[dirlen] = '/';
        modifdirname[dirlen + 1] = '\0';
        dirlen++;
    }
    for (size_t i = 0, pos = 0; i < (size_t)c;
         i++) { /* formerly looped while i < *count */
        if (!hasExtension(entry_list[i]->d_name, extension)) continue;
        size_t filelen = strlen(entry_list[i]->d_name);
        char *fullpath = malloc(dirlen + filelen + 1);
        strcpy(fullpath, modifdirname);
        strcpy(fullpath + dirlen, entry_list[i]->d_name);
        answer[pos] = read_integer_file(fullpath, &((*howmany)[pos]));
        pos++;
        free(fullpath);
    }
    if (modifdirname != dirname) {
        free(modifdirname);
    }
    for (int i = 0; i < c; ++i) free(entry_list[i]);
    free(entry_list);
    return answer;
}

static roaring_bitmap_t **create_all_bitmaps(size_t *howmany,
                                             uint32_t **numbers, size_t count) {
    if (numbers == NULL) return NULL;
    printf("Constructing %d  bitmaps.\n", (int)count);
    roaring_bitmap_t **answer = malloc(sizeof(roaring_bitmap_t *) * count);
    for (size_t i = 0; i < count; i++) {
        printf(".");
        fflush(stdout);
        answer[i] = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
    }
    printf("\n");
    return answer;
}

static void printusage(char *command) {
    printf(
        " Try %s directory \n where directory could be "
        "benchmarks/realdata/census1881\n",
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
    char *extension = ".txt";
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
    if (numbers == NULL) return -1;

    uint64_t cycles_start = 0, cycles_final = 0;

    RDTSC_START(cycles_start);
    roaring_bitmap_t **bitmaps = create_all_bitmaps(howmany, numbers, count);
    RDTSC_FINAL(cycles_final);
    if (bitmaps == NULL) return -1;
    printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);

    printf("Creating %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count; i += 2) {
        roaring_bitmap_t *CI = roaring_bitmap_copy(
            bitmaps[i]);  // to test the inplace version we create a copy
        roaring_bitmap_free(CI);
    }
    RDTSC_FINAL(cycles_final);
    printf("Copying and freeing %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    uint64_t successive_and = 0;
    uint64_t successive_or = 0;
    // try ANDing and ORing together consecutive pairs
    for (int i = 0; i < (int)count - 1; ++i) {
        uint32_t c1 = roaring_bitmap_get_cardinality(bitmaps[i]);
        uint32_t c2 = roaring_bitmap_get_cardinality(bitmaps[i + 1]);
        RDTSC_START(cycles_start);
        roaring_bitmap_t *tempand =
            roaring_bitmap_and(bitmaps[i], bitmaps[i + 1]);
        RDTSC_FINAL(cycles_final);
        successive_and += cycles_final - cycles_start;

        uint32_t ci = roaring_bitmap_get_cardinality(tempand);
        roaring_bitmap_free(tempand);
        RDTSC_START(cycles_start);
        roaring_bitmap_t *tempor =
            roaring_bitmap_or(bitmaps[i], bitmaps[i + 1]);
        RDTSC_FINAL(cycles_final);
        successive_or += cycles_final - cycles_start;

        uint32_t co = roaring_bitmap_get_cardinality(tempor);
        roaring_bitmap_free(tempor);

        if (c1 + c2 != co + ci) {
            printf(KRED "cardinalities are wrong somehow\n");
            printf("c1 = %d, c2 = %d, co = %d, ci = %d\n", c1, c2, co, ci);
            return -1;
        }
    }
    printf(" %zu successive bitmaps intersections took %" PRIu64 " cycles\n",
           count - 1, successive_and);
    printf(" %zu successive bitmaps unions took %" PRIu64 " cycles\n",
           count - 1, successive_or);

    roaring_bitmap_t **copyofr = malloc(sizeof(roaring_bitmap_t *) * count);
    for (int i = 0; i < (int)count; i++) {
        copyofr[i] = roaring_bitmap_copy(bitmaps[i]);
    }
    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; i++) {
        roaring_bitmap_and_inplace(copyofr[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    printf(" %zu successive in-place bitmaps intersections took %" PRIu64
           " cycles\n",
           count - 1, cycles_final - cycles_start);

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
        roaring_bitmap_free(bitmaps[i]);
        bitmaps[i] = NULL;  // paranoid
    }
    free(bitmaps);
    free(howmany);
    free(numbers);

    return 0;
}
