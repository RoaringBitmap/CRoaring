#ifndef CROARING_MICROBENCHMARKS_BENCH_H
#define CROARING_MICROBENCHMARKS_BENCH_H
// clang-format off
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#if (!defined(_WIN32) && !defined(_WIN64) && !(__MINGW32__) && !(__MINGW64__))
#include <dirent.h>
#else
#include "toni_ronnko_dirent.h"
#endif

#include <benchmark/benchmark.h>
#include <roaring/roaring.h>
#include <roaring/roaring64.h>
#include <roaring64map.hh>

#include "performancecounters/event_counter.h"
// clang-format on

#if CROARING_IS_X64
#ifndef CROARING_COMPILER_SUPPORTS_AVX512
#error "CROARING_COMPILER_SUPPORTS_AVX512 needs to be defined."
#endif  // CROARING_COMPILER_SUPPORTS_AVX512
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
using roaring::Roaring64Map;

event_collector collector;
size_t N = 1000;
size_t bitmap_examples_bytes = 0;
size_t count = 0;
roaring_bitmap_t **bitmaps = NULL;
roaring64_bitmap_t **bitmaps64 = NULL;
Roaring64Map **bitmaps64cpp = NULL;
uint32_t *array_buffer;
uint64_t *array_buffer64;
uint32_t maxvalue = 0;
uint32_t maxcard = 0;

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
    char *answer = (char *)malloc(size + 1);
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
static uint32_t *read_integer_file(const char *filename, size_t *howmany) {
    char *buffer = read_file(filename);
    if (buffer == NULL) return NULL;

    size_t howmanyints = 1;
    size_t i1 = 0;
    for (; buffer[i1] != '\0'; i1++) {
        if (buffer[i1] == ',') ++howmanyints;
    }

    uint32_t *answer = (uint32_t *)malloc(howmanyints * sizeof(uint32_t));
    if (answer == NULL) return NULL;
    size_t pos = 0;
    for (size_t i = 0; (i < i1) && (buffer[i] != '\0'); i++) {
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

/**
 * Does the file filename ends with the given extension.
 */
static bool has_extension(const char *filename, const char *extension) {
    const char *ext = strrchr(filename, '.');
    return (ext && !strcmp(ext, extension));
}

/**
 * read all (count) integer files in a directory. Caller is responsible
 * for memory de-allocation. In case of error, a NULL is returned.
 */
static uint32_t **read_all_integer_files(const char *dirname,
                                         const char *extension,
                                         size_t **howmany, size_t *tcount) {
    struct dirent **entry_list;

    int c = scandir(dirname, &entry_list, 0, alphasort);
    if (c < 0) return NULL;
    size_t truec = 0;
    for (int i = 0; i < c; i++) {
        if (has_extension(entry_list[i]->d_name, extension)) ++truec;
    }
    *tcount = truec;
    *howmany = (size_t *)malloc(sizeof(size_t) * (*tcount));
    uint32_t **answer = (uint32_t **)malloc(sizeof(uint32_t *) * (*tcount));
    size_t dirlen = strlen(dirname);
    char *modifdirname = (char *)dirname;
    if (modifdirname[dirlen - 1] != '/') {
        modifdirname = (char *)malloc(dirlen + 2);
        strcpy(modifdirname, dirname);
        modifdirname[dirlen] = '/';
        modifdirname[dirlen + 1] = '\0';
        dirlen++;
    }
    for (size_t i = 0, pos = 0; i < (size_t)c;
         i++) { /* formerly looped while i < *tcount */
        if (!has_extension(entry_list[i]->d_name, extension)) continue;
        size_t filelen = strlen(entry_list[i]->d_name);
        char *fullpath = (char *)malloc(dirlen + filelen + 1);
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
/**
 * Once you have collected all the integers, build the bitmaps.
 */
static roaring_bitmap_t **create_all_bitmaps(size_t *howmany,
                                             uint32_t **numbers, size_t tcount,
                                             bool runoptimize,
                                             bool copy_on_write) {
    for (size_t i = 0; i < count; i++) {
        if (howmany[i] > 0) {
            if (maxvalue < numbers[i][howmany[i] - 1]) {
                maxvalue = numbers[i][howmany[i] - 1];
            }
        }
        if (maxcard < howmany[i]) {
            maxcard = howmany[i];
        }
    }
    if (numbers == NULL) return NULL;
    roaring_bitmap_t **answer =
        (roaring_bitmap_t **)malloc(sizeof(roaring_bitmap_t *) * tcount);
    bitmap_examples_bytes = 0;
    for (size_t i = 0; i < tcount; i++) {
        answer[i] = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
        if (runoptimize) roaring_bitmap_run_optimize(answer[i]);
        roaring_bitmap_shrink_to_fit(answer[i]);
        bitmap_examples_bytes += roaring_bitmap_size_in_bytes(answer[i]);
        roaring_bitmap_set_copy_on_write(answer[i], copy_on_write);
    }
    array_buffer = (uint32_t *)malloc(maxcard * sizeof(uint32_t));
    array_buffer64 = (uint64_t *)malloc(maxcard * sizeof(uint64_t));
    return answer;
}

static roaring64_bitmap_t **create_all_64bitmaps(size_t *howmany,
                                                 uint32_t **numbers,
                                                 size_t tcount,
                                                 bool runoptimize) {
    for (size_t i = 0; i < count; i++) {
        if (howmany[i] > 0) {
            if (maxvalue < numbers[i][howmany[i] - 1]) {
                maxvalue = numbers[i][howmany[i] - 1];
            }
        }
        if (maxcard < howmany[i]) {
            maxcard = howmany[i];
        }
    }
    if (numbers == NULL) return NULL;
    roaring64_bitmap_t **answer =
        (roaring64_bitmap_t **)malloc(sizeof(roaring64_bitmap_t *) * tcount);
    for (size_t i = 0; i < tcount; i++) {
        answer[i] = roaring64_bitmap_create();
        for (size_t j = 0; j < howmany[i]; ++j) {
            roaring64_bitmap_add(answer[i], numbers[i][j]);
        }
        if (runoptimize) roaring64_bitmap_run_optimize(answer[i]);
    }
    return answer;
}

static Roaring64Map **create_all_64bitmaps_cpp(size_t *howmany,
                                               uint32_t **numbers,
                                               size_t tcount,
                                               bool runoptimize) {
    for (size_t i = 0; i < count; i++) {
        if (howmany[i] > 0) {
            if (maxvalue < numbers[i][howmany[i] - 1]) {
                maxvalue = numbers[i][howmany[i] - 1];
            }
        }
        if (maxcard < howmany[i]) {
            maxcard = howmany[i];
        }
    }
    if (numbers == NULL) return NULL;
    Roaring64Map **answer =
        (Roaring64Map **)malloc(sizeof(Roaring64Map *) * tcount);
    for (size_t i = 0; i < tcount; i++) {
        answer[i] = new Roaring64Map();
        for (size_t j = 0; j < howmany[i]; ++j) {
            answer[i]->add(numbers[i][j]);
        }
        if (runoptimize) answer[i]->runOptimize();
    }
    return answer;
}

template <class func>
static void BasicBench(benchmark::State &state) {
    // volatile to prevent optimizations.
    volatile uint64_t marker = 0;
    for (auto _ : state) {
        marker = func::run();
    }
    if (collector.has_events()) {
        event_aggregate aggregate{};
        for (size_t i = 0; i < N; i++) {
            std::atomic_thread_fence(std::memory_order_acquire);
            collector.start();
            marker = func::run();
            std::atomic_thread_fence(std::memory_order_release);
            event_count allocate_count = collector.end();
            aggregate << allocate_count;
        }
        state.counters["cycles"] = aggregate.best.cycles();

        state.counters["instructions"] = aggregate.best.instructions();
        state.counters["GHz"] =
            aggregate.best.cycles() / aggregate.best.elapsed_ns();
    }
    (void)marker;
}

int load(const char *dirname) {
    const char *extension = ".txt";
    bool copy_on_write = false;
    bool runoptimize = true;
    size_t *howmany;

    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &howmany, &count);
    if (numbers == NULL) {
        printf(
            "I could not find or load any data file with extension %s in "
            "directory %s.\n",
            extension, dirname);
        return -1;
    }
    bitmaps =
        create_all_bitmaps(howmany, numbers, count, runoptimize, copy_on_write);
    bitmaps64 = create_all_64bitmaps(howmany, numbers, count, runoptimize);
    bitmaps64cpp =
        create_all_64bitmaps_cpp(howmany, numbers, count, runoptimize);

    for (size_t i = 0; i < count; ++i) {
        free(numbers[i]);
    }
    free(howmany);
    if (bitmaps == NULL) return -1;
    return count;
}
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif