#include "bench.h"

#include <random>
#include <vector>

// Synthetic dataset for cold vs. warm contains() benchmarks.
// 10,000 bitmaps over the universe [0, 2^18). Each bitmap is built by
// splitting the universe into 4 blocks of 2^16, drawing the per-block
// cardinality from a Poisson distribution with mean = density * 2^16,
// then placing the values uniformly within each block.
static constexpr size_t kSyntheticCount = 10000;
static constexpr uint32_t kSyntheticUniverse = 1u << 18;
static constexpr uint32_t kSyntheticBlockSize = 1u << 16;
static constexpr uint32_t kSyntheticBlockCount =
    kSyntheticUniverse / kSyntheticBlockSize;
static constexpr size_t kWarmRepeats = 1000;
// Warm probes the same kWarmBitmaps bitmaps kWarmRepeats times each, so the
// total contains() count matches the cold benchmark (kSyntheticCount probes).
static constexpr size_t kWarmBitmaps = kSyntheticCount / kWarmRepeats;

static roaring_bitmap_t **synth_bitmaps_low = nullptr;
static roaring_bitmap_t **synth_bitmaps_mod = nullptr;
static roaring_bitmap_t **synth_bitmaps_high = nullptr;
static uint32_t *synth_queries_cold = nullptr;
static uint32_t *synth_queries_warm = nullptr;

static roaring_bitmap_t **build_synthetic_bitmaps(double density,
                                                  uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::poisson_distribution<int> poisson(density * kSyntheticBlockSize);
    std::uniform_int_distribution<uint32_t> within_block(
        0, kSyntheticBlockSize - 1);
    auto **out = (roaring_bitmap_t **)malloc(sizeof(roaring_bitmap_t *) *
                                             kSyntheticCount);
    for (size_t i = 0; i < kSyntheticCount; ++i) {
        out[i] = roaring_bitmap_create();
        for (uint32_t b = 0; b < kSyntheticBlockCount; ++b) {
            int n = poisson(rng);
            if (n < 0) n = 0;
            if (n > (int)kSyntheticBlockSize) n = (int)kSyntheticBlockSize;
            uint32_t base = b * kSyntheticBlockSize;
            for (int k = 0; k < n; ++k) {
                roaring_bitmap_add(out[i], base + within_block(rng));
            }
        }
        roaring_bitmap_run_optimize(out[i]);
        roaring_bitmap_shrink_to_fit(out[i]);
    }
    return out;
}

static void load_synthetic() {
    synth_bitmaps_low = build_synthetic_bitmaps(0.001, 0xC0FFEE0001ULL);
    synth_bitmaps_mod = build_synthetic_bitmaps(0.01, 0xC0FFEE0002ULL);
    synth_bitmaps_high = build_synthetic_bitmaps(0.1, 0xC0FFEE0003ULL);

    std::mt19937_64 rng(0xDEADBEEFULL);
    std::uniform_int_distribution<uint32_t> dist(0, kSyntheticUniverse - 1);
    synth_queries_cold = (uint32_t *)malloc(sizeof(uint32_t) * kSyntheticCount);
    for (size_t i = 0; i < kSyntheticCount; ++i) {
        synth_queries_cold[i] = dist(rng);
    }
    // Single small pool reused for every bitmap in the warm benchmarks: keeps
    // the query array in L1 so we measure bitmap-cache residency, not the
    // bandwidth of streaming a large query buffer.
    synth_queries_warm = (uint32_t *)malloc(sizeof(uint32_t) * kWarmRepeats);
    for (size_t i = 0; i < kWarmRepeats; ++i) {
        synth_queries_warm[i] = dist(rng);
    }
}

static void free_synthetic() {
    for (size_t i = 0; i < kSyntheticCount; ++i) {
        roaring_bitmap_free(synth_bitmaps_low[i]);
        roaring_bitmap_free(synth_bitmaps_mod[i]);
        roaring_bitmap_free(synth_bitmaps_high[i]);
    }
    free(synth_bitmaps_low);
    free(synth_bitmaps_mod);
    free(synth_bitmaps_high);
    free(synth_queries_cold);
    free(synth_queries_warm);
}

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

// Wraps BasicBench and reports an "ns/query" column normalized by the
// number of contains() calls each iteration performs.
template <class func, size_t QueriesPerIter>
static void BasicBenchPerQuery(benchmark::State &state) {
    BasicBench<func>(state);
    state.counters["ns/query"] =
        benchmark::Counter(double(QueriesPerIter),
                           benchmark::Counter::kIsIterationInvariantRate |
                               benchmark::Counter::kInvert,
                           benchmark::Counter::OneK::kIs1000);
}

// Cold contains: walk all kSyntheticCount bitmaps, one random query each.
// Each new bitmap evicts the previous from cache.
struct contains_cold_low {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < kSyntheticCount; ++i) {
            marker += roaring_bitmap_contains(synth_bitmaps_low[i],
                                              synth_queries_cold[i]);
        }
        return marker;
    }
};
auto ContainsColdLow = BasicBenchPerQuery<contains_cold_low, kSyntheticCount>;
BENCHMARK(ContainsColdLow);

struct contains_cold_mod {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < kSyntheticCount; ++i) {
            marker += roaring_bitmap_contains(synth_bitmaps_mod[i],
                                              synth_queries_cold[i]);
        }
        return marker;
    }
};
auto ContainsColdMod = BasicBenchPerQuery<contains_cold_mod, kSyntheticCount>;
BENCHMARK(ContainsColdMod);

struct contains_cold_high {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < kSyntheticCount; ++i) {
            marker += roaring_bitmap_contains(synth_bitmaps_high[i],
                                              synth_queries_cold[i]);
        }
        return marker;
    }
};
auto ContainsColdHigh = BasicBenchPerQuery<contains_cold_high, kSyntheticCount>;
BENCHMARK(ContainsColdHigh);

// Warm contains: kWarmRepeats random queries against the same bitmap before
// moving on, so the bitmap is cache-resident for all but the first probe.
struct contains_warm_low {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < kWarmBitmaps; ++i) {
            roaring_bitmap_t *b = synth_bitmaps_low[i];
            for (size_t r = 0; r < kWarmRepeats; ++r) {
                marker += roaring_bitmap_contains(b, synth_queries_warm[r]);
            }
        }
        return marker;
    }
};
auto ContainsWarmLow =
    BasicBenchPerQuery<contains_warm_low, kWarmBitmaps * kWarmRepeats>;
BENCHMARK(ContainsWarmLow);

struct contains_warm_mod {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < kWarmBitmaps; ++i) {
            roaring_bitmap_t *b = synth_bitmaps_mod[i];
            for (size_t r = 0; r < kWarmRepeats; ++r) {
                marker += roaring_bitmap_contains(b, synth_queries_warm[r]);
            }
        }
        return marker;
    }
};
auto ContainsWarmMod =
    BasicBenchPerQuery<contains_warm_mod, kWarmBitmaps * kWarmRepeats>;
BENCHMARK(ContainsWarmMod);

struct contains_warm_high {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < kWarmBitmaps; ++i) {
            roaring_bitmap_t *b = synth_bitmaps_high[i];
            for (size_t r = 0; r < kWarmRepeats; ++r) {
                marker += roaring_bitmap_contains(b, synth_queries_warm[r]);
            }
        }
        return marker;
    }
};
auto ContainsWarmHigh =
    BasicBenchPerQuery<contains_warm_high, kWarmBitmaps * kWarmRepeats>;
BENCHMARK(ContainsWarmHigh);

// Note that input data matters: census1881 produces mostly array containers.
template <uint64_t offset>
struct add_offset {
    static uint64_t run() {
        uint64_t marker = 0;
        for (size_t i = 0; i < count; ++i) {
            roaring_bitmap_t *tmp =
                roaring_bitmap_add_offset(bitmaps[i], offset);
            marker += roaring_bitmap_get_cardinality(tmp);
            roaring_bitmap_free(tmp);
        }
        return marker;
    }
};
auto AddOffset1 = BasicBench<add_offset<1>>;
BENCHMARK(AddOffset1);

// prime number close to the half of the container capacity:
// inhibits fast code paths and also stresses OR
auto AddOffset32771 = BasicBench<add_offset<32771>>;
BENCHMARK(AddOffset32771);

// containers are cpoied verbatim, without tearing
auto AddOffset65536 = BasicBench<add_offset<65536>>;
BENCHMARK(AddOffset65536);

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
    load_synthetic();
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
    free_synthetic();
}
