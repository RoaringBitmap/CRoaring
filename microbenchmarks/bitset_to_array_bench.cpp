// Conversion of a sparse bitset container (at most 4096 values) to an array
// container, as done by array_container_from_bitset. The conversion happens
// whenever an operation produces a bitset container that turns out to be
// sparse: unions (roaring_bitmap_repair_after_lazy, and so
// roaring_bitmap_or_many), intersections, differences, symmetric differences,
// negations and removals.
//
// The "Decode" benchmarks time the conversion alone, through the dispatching
// array_container_from_bitset and through each decoder forced: on x64 with
// AVX-512, the dispatch should never be slower than the faster decoder. The
// "OrMany" benchmarks time roaring_bitmap_or_many over many sparse bitmaps
// whose union is still sparse, so that each container goes through a bitset and
// is converted back to an array.
//
// Each benchmark cycles through many distinct inputs so that the branch
// predictor cannot learn them.
#include <algorithm>
#include <benchmark/benchmark.h>
#include <random>
#include <vector>

#include "roaring/bitset_util.h"
#include "roaring/containers/containers.h"
#include "roaring/roaring.h"

#if CROARING_IS_X64
#include "roaring/isadetection.h"
#endif

#ifdef __cplusplus
using namespace ::roaring::internal;
#endif

namespace {

constexpr size_t kInputs = 64;

// How the values are laid out within the 64K chunk.
enum Layout : int64_t {
    // Values drawn uniformly at random from [0, 65536).
    kUniform = 0,
    // Values drawn uniformly at random from the first 'card' * 4 values, so
    // that the set bits fill a quarter of the words they span, and the
    // remaining words are empty.
    kClustered = 1,
    // Runs of 16 consecutive values at random positions.
    kRuns = 2,
};

std::vector<uint16_t> make_values(std::mt19937 &gen, size_t card,
                                  Layout layout) {
    std::vector<uint16_t> out;
    if (layout == kRuns) {
        std::vector<bool> seen(65536 / 16, false);
        std::uniform_int_distribution<uint32_t> dist(0, 65536 / 16 - 1);
        while (out.size() < card) {
            uint32_t start = dist(gen);
            if (seen[start]) continue;
            seen[start] = true;
            for (uint32_t i = 0; i < 16 && out.size() < card; i++) {
                out.push_back((uint16_t)(start * 16 + i));
            }
        }
    } else {
        uint32_t universe = 65536;
        if (layout == kClustered) {
            universe = (uint32_t)std::min<size_t>(65536, card * 4);
        }
        std::vector<uint32_t> all(universe);
        for (uint32_t i = 0; i < universe; i++) all[i] = i;
        std::shuffle(all.begin(), all.end(), gen);
        for (size_t i = 0; i < card; i++) out.push_back((uint16_t)all[i]);
    }
    std::sort(out.begin(), out.end());
    return out;
}

struct Bitsets {
    std::vector<bitset_container_t *> inputs;
    Bitsets(size_t card, Layout layout) {
        std::mt19937 gen(4321 + card * 7919 + layout);
        for (size_t i = 0; i < kInputs; i++) {
            bitset_container_t *b = bitset_container_create();
            for (uint16_t v : make_values(gen, card, layout)) {
                bitset_container_set(b, v);
            }
            inputs.push_back(b);
        }
    }
    ~Bitsets() {
        for (bitset_container_t *b : inputs) bitset_container_free(b);
    }
};

// array_container_from_bitset, as used by the library.
void Decode(benchmark::State &state) {
    Bitsets bitsets((size_t)state.range(0), (Layout)state.range(1));
    size_t i = 0;
    for (auto _ : state) {
        array_container_t *a = array_container_from_bitset(bitsets.inputs[i]);
        benchmark::DoNotOptimize(a->array);
        array_container_free(a);
        i = (i + 1) % kInputs;
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

// The portable scalar decoder, forced.
void DecodeScalar(benchmark::State &state) {
    Bitsets bitsets((size_t)state.range(0), (Layout)state.range(1));
    std::vector<uint16_t> out(DEFAULT_MAX_SIZE + 64);
    size_t i = 0;
    for (auto _ : state) {
        const bitset_container_t *b = bitsets.inputs[i];
        size_t n = bitset_extract_setbits_uint16(
            b->words, BITSET_CONTAINER_SIZE_IN_WORDS, out.data(), 0);
        benchmark::DoNotOptimize(n);
        benchmark::DoNotOptimize(out.data());
        i = (i + 1) % kInputs;
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

#if CROARING_IS_X64 && CROARING_COMPILER_SUPPORTS_AVX512
// The AVX-512 decoder, forced.
void DecodeAvx512(benchmark::State &state) {
    if (!(croaring_hardware_support() & ROARING_SUPPORTS_AVX512)) {
        state.SkipWithError("AVX-512 is not supported");
        return;
    }
    Bitsets bitsets((size_t)state.range(0), (Layout)state.range(1));
    std::vector<uint16_t> out(DEFAULT_MAX_SIZE + 64);
    size_t i = 0;
    for (auto _ : state) {
        const bitset_container_t *b = bitsets.inputs[i];
        size_t n = bitset_extract_setbits_avx512_uint16(
            b->words, BITSET_CONTAINER_SIZE_IN_WORDS, out.data(),
            (size_t)b->cardinality, 0);
        benchmark::DoNotOptimize(n);
        benchmark::DoNotOptimize(out.data());
        i = (i + 1) % kInputs;
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
#endif

// {cardinality, layout}
void DecodeSizes(benchmark::internal::Benchmark *b) {
    for (int64_t layout : {kUniform, kClustered, kRuns}) {
        for (int64_t card :
             {16, 64, 128, 256, 512, 768, 1000, 1023, 1024, 2048, 4096}) {
            b->Args({card, layout});
        }
    }
    b->ArgNames({"card", "layout"});
    // The first benchmarks would otherwise run on a cold core.
    b->MinWarmUpTime(0.1);
}

// 'count' bitmaps, each with 'per_container' random values in each of
// kChunks chunks. Their union has about count * per_container values per
// chunk, at most 4096, so that each container of the union is an array
// container that was computed as a bitset first. There are enough chunks that
// the branch predictor cannot learn them all.
constexpr uint32_t kChunks = 1024;

struct ManyBitmaps {
    std::vector<roaring_bitmap_t *> bitmaps;
    ManyBitmaps(size_t count, size_t per_container) {
        std::mt19937 gen(99 + count * 31 + per_container);
        std::uniform_int_distribution<uint32_t> low(0, 65535);
        std::vector<uint32_t> values;
        for (size_t k = 0; k < count; k++) {
            values.clear();
            for (uint32_t chunk = 0; chunk < kChunks; chunk++) {
                for (size_t j = 0; j < per_container; j++) {
                    values.push_back((chunk << 16) | low(gen));
                }
            }
            roaring_bitmap_t *r = roaring_bitmap_create();
            roaring_bitmap_add_many(r, values.size(), values.data());
            bitmaps.push_back(r);
        }
    }
    ~ManyBitmaps() {
        for (roaring_bitmap_t *r : bitmaps) roaring_bitmap_free(r);
    }
};

// roaring_bitmap_or_many over 'count' bitmaps: the union is accumulated in
// bitset containers (lazily, with bitset conversion), and
// roaring_bitmap_repair_after_lazy converts each container back to an array.
void OrMany(benchmark::State &state) {
    ManyBitmaps many((size_t)state.range(0), (size_t)state.range(1));
    const roaring_bitmap_t **inputs =
        (const roaring_bitmap_t **)many.bitmaps.data();
    for (auto _ : state) {
        roaring_bitmap_t *r =
            roaring_bitmap_or_many(many.bitmaps.size(), inputs);
        benchmark::DoNotOptimize(r);
        roaring_bitmap_free(r);
    }
}

// {count, per_container}: union cardinalities per container of about 200,
// 500, 1000, 2000 and 4000.
void ManySizes(benchmark::internal::Benchmark *b) {
    b->Args({40, 5});
    b->Args({100, 5});
    b->Args({200, 5});
    b->Args({400, 5});
    b->Args({800, 5});
    b->ArgNames({"count", "per_container"});
}

}  // namespace

BENCHMARK(Decode)->Apply(DecodeSizes);
BENCHMARK(DecodeScalar)->Apply(DecodeSizes);
#if CROARING_IS_X64 && CROARING_COMPILER_SUPPORTS_AVX512
BENCHMARK(DecodeAvx512)->Apply(DecodeSizes);
#endif
BENCHMARK(OrMany)->Apply(ManySizes);

BENCHMARK_MAIN();
