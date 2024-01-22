#include "bench.h"

#include <vector>

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

struct successive_intersection64 {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            roaring64_bitmap_t *tempand =
                roaring64_bitmap_and(bitmaps64[i], bitmaps64[i + 1]);
            marker += roaring64_bitmap_get_cardinality(tempand);
            roaring64_bitmap_free(tempand);
        }
        return marker;
    }
};
auto SuccessiveIntersection64 = BasicBench<successive_intersection64>;
BENCHMARK(SuccessiveIntersection64);

struct successive_intersection_cardinality {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            marker +=
                roaring_bitmap_and_cardinality(bitmaps[i], bitmaps[i + 1]);
        }
        return marker;
    }
};
auto SuccessiveIntersectionCardinality =
    BasicBench<successive_intersection_cardinality>;
BENCHMARK(SuccessiveIntersectionCardinality);

struct successive_intersection_cardinality64 {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            marker += roaring64_bitmap_and_cardinality(bitmaps64[i],
                                                       bitmaps64[i + 1]);
        }
        return marker;
    }
};
auto SuccessiveIntersectionCardinality64 =
    BasicBench<successive_intersection_cardinality64>;
BENCHMARK(SuccessiveIntersectionCardinality64);

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

struct successive_union_cardinality64 {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            marker +=
                roaring64_bitmap_or_cardinality(bitmaps64[i], bitmaps64[i + 1]);
        }
        return marker;
    }
};
auto SuccessiveUnionCardinality64 = BasicBench<successive_union_cardinality64>;
BENCHMARK(SuccessiveUnionCardinality64);

struct successive_difference_cardinality {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            marker +=
                roaring_bitmap_andnot_cardinality(bitmaps[i], bitmaps[i + 1]);
        }
        return marker;
    }
};
auto SuccessiveDifferenceCardinality =
    BasicBench<successive_difference_cardinality>;
BENCHMARK(SuccessiveDifferenceCardinality);

struct successive_difference_cardinality64 {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            marker += roaring64_bitmap_andnot_cardinality(bitmaps64[i],
                                                          bitmaps64[i + 1]);
        }
        return marker;
    }
};
auto SuccessiveDifferenceCardinality64 =
    BasicBench<successive_difference_cardinality64>;
BENCHMARK(SuccessiveDifferenceCardinality64);

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

struct successive_union64 {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            roaring64_bitmap_t *tempunion =
                roaring64_bitmap_or(bitmaps64[i], bitmaps64[i + 1]);
            marker += roaring64_bitmap_get_cardinality(tempunion);
            roaring64_bitmap_free(tempunion);
        }
        return marker;
    }
};
auto SuccessiveUnion64 = BasicBench<successive_union64>;
BENCHMARK(SuccessiveUnion64);

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

struct random_access64 {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            marker += roaring64_bitmap_contains(bitmaps64[i], maxvalue / 4);
            marker += roaring64_bitmap_contains(bitmaps64[i], maxvalue / 2);
            marker += roaring64_bitmap_contains(bitmaps64[i], 3 * maxvalue / 4);
        }
        return marker;
    }
};
auto RandomAccess64 = BasicBench<random_access64>;
BENCHMARK(RandomAccess64);

struct random_access64_cpp {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            marker += bitmaps64cpp[i]->contains(maxvalue / 4);
            marker += bitmaps64cpp[i]->contains(maxvalue / 2);
            marker += bitmaps64cpp[i]->contains(3 * maxvalue / 4);
        }
        return marker;
    }
};
auto RandomAccess64Cpp = BasicBench<random_access64_cpp>;
BENCHMARK(RandomAccess64Cpp);

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

struct to_array64 {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            roaring64_bitmap_to_uint64_array(bitmaps64[i], array_buffer64);
            marker += array_buffer[0];
        }
        return marker;
    }
};
auto ToArray64 = BasicBench<to_array64>;
BENCHMARK(ToArray64);

struct iterate_all {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            roaring_bitmap_t *r = bitmaps[i];
            roaring_uint32_iterator_t j;
            roaring_iterator_init(r, &j);
            while (j.has_value) {
                marker++;
                roaring_uint32_iterator_advance(&j);
            }
        }
        return marker;
    }
};
auto IterateAll = BasicBench<iterate_all>;
BENCHMARK(IterateAll);

struct iterate_all64 {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            roaring64_bitmap_t *r = bitmaps64[i];
            roaring64_iterator_t *it = roaring64_iterator_create(r);
            while (roaring64_iterator_has_value(it)) {
                marker++;
                roaring64_iterator_advance(it);
            }
            roaring64_iterator_free(it);
        }
        return marker;
    }
};
auto IterateAll64 = BasicBench<iterate_all64>;
BENCHMARK(IterateAll64);

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

struct compute_cardinality64 {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            marker += roaring64_bitmap_get_cardinality(bitmaps64[i]);
        }
        return marker;
    }
};

auto ComputeCardinality64 = BasicBench<compute_cardinality64>;
BENCHMARK(ComputeCardinality64);

struct rank_many_slow {
    static uint64_t run() {
        std::vector<uint64_t> ranks(5);
        for (size_t i = 0; i < count; ++i) {
            ranks[0] = roaring_bitmap_rank(bitmaps[i], maxvalue / 5);
            ranks[1] = roaring_bitmap_rank(bitmaps[i], 2 * maxvalue / 5);
            ranks[2] = roaring_bitmap_rank(bitmaps[i], 3 * maxvalue / 5);
            ranks[3] = roaring_bitmap_rank(bitmaps[i], 4 * maxvalue / 5);
            ranks[4] = roaring_bitmap_rank(bitmaps[i], maxvalue);
        }
        return ranks[0];
    }
};
auto RankManySlow = BasicBench<rank_many_slow>;
BENCHMARK(RankManySlow);

struct rank_many {
    static uint64_t run() {
        std::vector<uint64_t> ranks(5);
        std::vector<uint32_t> input{maxvalue / 5, 2 * maxvalue / 5,
                                    3 * maxvalue / 5, 4 * maxvalue / 5,
                                    maxvalue};
        for (size_t i = 0; i < count; ++i) {
            roaring_bitmap_rank_many(bitmaps[i], input.data(),
                                     input.data() + input.size(), ranks.data());
        }
        return ranks[0];
    }
};
auto RankMany = BasicBench<rank_many>;
BENCHMARK(RankMany);

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
    benchmark::AddCustomContext(
        "AVX-512 hardware",
        (support & roaring::internal::ROARING_SUPPORTS_AVX512) ? "yes" : "no");
#endif  // CROARING_COMPILER_SUPPORTS_AVX512
    benchmark::AddCustomContext(
        "AVX-2 hardware",
        (support & roaring::internal::ROARING_SUPPORTS_AVX2) ? "yes" : "no");
#endif  // CROARING_IS_X64
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
