#define _GNU_SOURCE
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <roaring/misc/configreport.h>
#include <roaring/roaring.h>

#include "config.h"

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
    roaring_bitmap_t **answer = malloc(sizeof(roaring_bitmap_t *) * count);
    for (size_t i = 0; i < count; i++) {
        answer[i] = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
    }
    return answer;
}

#define KRED "\x1B[31m"

int realdatacheck(char *dirname) {
    printf("[%s] %s %s\n", __FILE__, __func__, dirname);
    char *extension = ".txt";
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

    roaring_bitmap_t **bitmaps = create_all_bitmaps(howmany, numbers, count);

    if (bitmaps == NULL) return -1;
    printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);

    for (int i = 0; i < (int)count; i += 2) {
        roaring_bitmap_t *CI = roaring_bitmap_copy(
            bitmaps[i]);  // to test the inplace version we create a copy
        roaring_bitmap_free(CI);
    }

    // try ANDing and ORing together consecutive pairs
    for (int i = 0; i < (int)count - 1; ++i) {
        uint32_t c1 = roaring_bitmap_get_cardinality(bitmaps[i]);
        uint32_t c2 = roaring_bitmap_get_cardinality(bitmaps[i + 1]);
        roaring_bitmap_t *tempand =
            roaring_bitmap_and(bitmaps[i], bitmaps[i + 1]);
        uint32_t ci = roaring_bitmap_get_cardinality(tempand);
        size_t trueci = intersection_uint32_card(
            numbers[i], howmany[i], numbers[i + 1], howmany[i + 1]);
        if (ci != trueci) {
            printf(KRED "intersection cardinalities are wrong.\n");
            printf("c1 = %d, c2 = %d, ci = %d, trueci = %d\n", c1, c2, ci,
                   (int)trueci);
            return -1;
        }
        roaring_bitmap_free(tempand);
        roaring_bitmap_t *tempor =
            roaring_bitmap_or(bitmaps[i], bitmaps[i + 1]);
        uint32_t co = roaring_bitmap_get_cardinality(tempor);
        size_t trueco = union_uint32_card(numbers[i], howmany[i],
                                          numbers[i + 1], howmany[i + 1]);

        if (co != trueco) {
            printf(KRED "union cardinalities are wrong.\n");
            printf("c1 = %d, c2 = %d, co = %d, trueco = %d\n", c1, c2, co,
                   (int)trueco);
            return -1;
        }
        roaring_bitmap_free(tempor);

        if (c1 + c2 != co + ci) {
            printf(KRED "cardinalities are wrong somehow\n");
            printf("c1 = %d, c2 = %d, co = %d, ci = %d\n", c1, c2, co, ci);
            return -1;
        }
    }

    // then mangle them with inplace
    for (int i = 0; i < (int)count - 1; i += 2) {
        roaring_bitmap_t *CI = roaring_bitmap_copy(
            bitmaps[i]);  // to test the inplace version we create a copy
        roaring_bitmap_and_inplace(CI, bitmaps[i + 1]);
        uint32_t ci = roaring_bitmap_get_cardinality(CI);
        size_t trueci = intersection_uint32_card(
            numbers[i], howmany[i], numbers[i + 1], howmany[i + 1]);
        if (ci != trueci) {
            printf(KRED " there is a problem with in-place intersections\n");
            return -1;
        }

        roaring_bitmap_free(CI);
        roaring_bitmap_t *tempand =
            roaring_bitmap_and(bitmaps[i], bitmaps[i + 1]);
        if (ci != roaring_bitmap_get_cardinality(tempand)) {
            printf(KRED " there is a problem with in-place intersections\n");
            return -1;
        }
        roaring_bitmap_free(tempand);
    }
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

int main() {
    tellmeall();
    int r = 0;
    r = realdatacheck(BENCHMARK_DATA_DIR "census1881");
    if (r != 0) return -1;
    r = realdatacheck(BENCHMARK_DATA_DIR "census1881_srt");
    if (r != 0) return -1;
    r = realdatacheck(BENCHMARK_DATA_DIR "census-income");
    if (r != 0) return -1;
    r = realdatacheck(BENCHMARK_DATA_DIR "census-income_srt");
    if (r != 0) return -1;
    r = realdatacheck(BENCHMARK_DATA_DIR "uscensus2000");
    if (r != 0) return -1;
    r = realdatacheck(BENCHMARK_DATA_DIR "weather_sept_85");
    if (r != 0) return -1;
    r = realdatacheck(BENCHMARK_DATA_DIR "weather_sept_85_srt");
    if (r != 0) return -1;
    r = realdatacheck(BENCHMARK_DATA_DIR "wikileaks-noquotes");
    if (r != 0) return -1;
    r = realdatacheck(BENCHMARK_DATA_DIR "wikileaks-noquotes_srt");
    if (r != 0) return -1;

    return EXIT_SUCCESS;
}
