#include "bench.h"


struct successive_intersection {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            roaring_bitmap_t *tempand =
                roaring_bitmap_and(bitmaps[i], bitmaps[i + 1]);
            marker += roaring_bitmap_get_cardinality(tempand);
            roaring_bitmap_free(tempand);
        }
        return marker;
    }
};
auto SuccessiveIntersection = BasicBench<successive_intersection>;
BENCHMARK(SuccessiveIntersection);


struct successive_intersection_cardinality {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            marker += roaring_bitmap_and_cardinality(bitmaps[i], bitmaps[i + 1]);
        }
        return marker;
    }
};
auto SuccessiveIntersectionCardinality = BasicBench<successive_intersection_cardinality>;
BENCHMARK(SuccessiveIntersectionCardinality);


struct successive_union_cardinality {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            marker += roaring_bitmap_or_cardinality(bitmaps[i], bitmaps[i + 1]);
        }
        return marker;
    }
};
auto SuccessiveUnionCardinality = BasicBench<successive_union_cardinality>;
BENCHMARK(SuccessiveUnionCardinality);

struct successive_difference_cardinality {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            marker += roaring_bitmap_andnot_cardinality(bitmaps[i], bitmaps[i + 1]);
        }
        return marker;
    }
};
auto SuccessiveDifferenceCardinality = BasicBench<successive_difference_cardinality>;
BENCHMARK(SuccessiveDifferenceCardinality);

struct successive_union {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            roaring_bitmap_t *tempand =
                roaring_bitmap_or(bitmaps[i], bitmaps[i + 1]);
            marker += roaring_bitmap_get_cardinality(tempand);
            roaring_bitmap_free(tempand);
        }
        return marker;
    }
};
auto SuccessiveUnion = BasicBench<successive_union>;
BENCHMARK(SuccessiveUnion);

struct many_union {
    static uint64_t run() {
        uint64_t marker = 0;
        roaring_bitmap_t *totalorbitmap =
            roaring_bitmap_or_many(count, (const roaring_bitmap_t **)bitmaps);
        marker = roaring_bitmap_get_cardinality(totalorbitmap);
        roaring_bitmap_free(totalorbitmap);
        return marker;
    }
};
auto TotalUnion = BasicBench<many_union>;
BENCHMARK(TotalUnion);

struct many_union_heap {
    static uint64_t run() {
        uint64_t marker = 0;
        roaring_bitmap_t *totalorbitmap = roaring_bitmap_or_many_heap(
            count, (const roaring_bitmap_t **)bitmaps);
        marker = roaring_bitmap_get_cardinality(totalorbitmap);
        roaring_bitmap_free(totalorbitmap);
        return marker;
    }
};
auto TotalUnionHeap = BasicBench<many_union_heap>;
BENCHMARK(TotalUnionHeap);

struct random_access {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            marker += roaring_bitmap_contains(bitmaps[i], maxvalue / 4);
            marker += roaring_bitmap_contains(bitmaps[i], maxvalue / 2);
            marker += roaring_bitmap_contains(bitmaps[i], 3 * maxvalue / 4);
        }
        return marker;
    }
};
auto RandomAccess = BasicBench<random_access>;
BENCHMARK(RandomAccess);

struct to_array {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            roaring_bitmap_to_uint32_array(bitmaps[i], array_buffer);
            marker += array_buffer[0];
        }
        return marker;
    }
};
auto ToArray = BasicBench<to_array>;
BENCHMARK(ToArray);

struct iterate_all {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            roaring_bitmap_t *r = bitmaps[i];
            roaring_uint32_iterator_t j;
            roaring_init_iterator(r, &j);
            while (j.has_value) {
                marker++;
                roaring_advance_uint32_iterator(&j);
            }
        }
        return marker;
    }
};
auto IterateAll = BasicBench<iterate_all>;
BENCHMARK(IterateAll);


struct compute_cardinality {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            marker += roaring_bitmap_get_cardinality(bitmaps[i]);
        }
        return marker;
    }
};

auto ComputeCardinality = BasicBench<compute_cardinality>;
BENCHMARK(ComputeCardinality);

int main(int argc, char **argv) {
    const char *dir_name;
    if ((argc == 1) || (argc > 1 && argv[1][0] == '-')) {
        benchmark::AddCustomContext(
            "benchmarking other files",
            "You may pass is a data directory as a parameter.");
        dir_name = BENCHMARK_DATA_DIR "census1881";
    } else {
        dir_name = argv[1];
    }
    int number_loaded = load(dir_name);
#if (__APPLE__ && __aarch64__) || defined(__linux__)
    if (!collector.has_events()) {
        benchmark::AddCustomContext("performance counters",
                                    "No privileged access (sudo may help).");
    }
#else
    if (!collector.has_events()) {
        benchmark::AddCustomContext("performance counters",
                                    "Unsupported system.");
    }
#endif

#if CROARING_IS_X64
    benchmark::AddCustomContext("x64", "detected");
    int support = roaring::internal::croaring_hardware_support();
#if CROARING_COMPILER_SUPPORTS_AVX512
    benchmark::AddCustomContext("AVX-512", "supported by compiler");
    benchmark::AddCustomContext("AVX-512 hardware", ( support & roaring::internal::ROARING_SUPPORTS_AVX512 ) ? "yes" : "no");
#endif // CROARING_COMPILER_SUPPORTS_AVX512
    benchmark::AddCustomContext("AVX-2 hardware", ( support & roaring::internal::ROARING_SUPPORTS_AVX2 ) ? "yes" : "no");
#endif // CROARING_IS_X64
    benchmark::AddCustomContext("data source", dir_name);

    benchmark::AddCustomContext("number of bitmaps", std::to_string(count));

    benchmark::AddCustomContext(
        "In RAM volume in MiB (estimated)",
        std::to_string(bitmap_examples_bytes / (1024 * 1024.0)));
    if (number_loaded == -1) {
        return EXIT_FAILURE;
    }
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    for (size_t i = 0; i < count; ++i) {
        roaring_bitmap_free(bitmaps[i]);
    }
    free(array_buffer);
}