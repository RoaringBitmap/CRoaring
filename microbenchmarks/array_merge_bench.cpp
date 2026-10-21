// Operations between two array containers of very different sizes, in the
// same 64K chunk: a small bitmap with a few values against one with up to 4096.
// See https://github.com/RoaringBitmap/CRoaring/issues/912
//
// Each benchmark cycles through many distinct pairs so that the branch
// predictor cannot learn a single input.
#include <algorithm>
#include <benchmark/benchmark.h>
#include <random>
#include <vector>

#include "roaring/roaring.h"

namespace {

constexpr size_t kPairs = 64;

std::vector<uint32_t> random_values(std::mt19937 &gen, size_t count) {
    std::vector<uint32_t> all(65536);
    for (uint32_t i = 0; i < all.size(); i++) all[i] = i;
    std::shuffle(all.begin(), all.end(), gen);
    all.resize(count);
    std::sort(all.begin(), all.end());
    return all;
}

roaring_bitmap_t *from_values(const std::vector<uint32_t> &values) {
    roaring_bitmap_t *r = roaring_bitmap_create();
    roaring_bitmap_add_many(r, values.size(), values.data());
    return r;
}

struct Pairs {
    std::vector<roaring_bitmap_t *> small, large;
    // If subset is true, the small bitmap holds values of the large one, so
    // that an in-place union leaves the large bitmap unchanged.
    Pairs(size_t small_card, size_t large_card, bool subset) {
        std::mt19937 gen(1234 + small_card * 7919 + large_card);
        for (size_t p = 0; p < kPairs; p++) {
            std::vector<uint32_t> lv = random_values(gen, large_card);
            std::vector<uint32_t> sv;
            if (subset) {
                sv = lv;
                std::shuffle(sv.begin(), sv.end(), gen);
                sv.resize(small_card);
                std::sort(sv.begin(), sv.end());
            } else {
                sv = random_values(gen, small_card);
            }
            large.push_back(from_values(lv));
            small.push_back(from_values(sv));
        }
    }
    ~Pairs() {
        for (roaring_bitmap_t *r : small) roaring_bitmap_free(r);
        for (roaring_bitmap_t *r : large) roaring_bitmap_free(r);
    }
};

template <roaring_bitmap_t *(*op)(const roaring_bitmap_t *,
                                  const roaring_bitmap_t *),
          bool small_first>
void BinaryOp(benchmark::State &state) {
    Pairs pairs((size_t)state.range(0), (size_t)state.range(1), false);
    size_t p = 0;
    for (auto _ : state) {
        roaring_bitmap_t *r = small_first ? op(pairs.small[p], pairs.large[p])
                                          : op(pairs.large[p], pairs.small[p]);
        benchmark::DoNotOptimize(r);
        roaring_bitmap_free(r);
        p = (p + 1) % kPairs;
    }
}

// large |= small, with small a subset of large: the scenario of issue 912,
// where many small bitmaps are merged one at a time into a larger one.
void OrInplace(benchmark::State &state) {
    Pairs pairs((size_t)state.range(0), (size_t)state.range(1), true);
    size_t p = 0;
    for (auto _ : state) {
        roaring_bitmap_or_inplace(pairs.large[p], pairs.small[p]);
        benchmark::DoNotOptimize(pairs.large[p]);
        p = (p + 1) % kPairs;
    }
}

// {small cardinality, large cardinality}. The unions stay below 4096 values,
// so the results are arrays too. The balanced pairs exercise the regular merge
// and guard against regressions there.
void Sizes(benchmark::internal::Benchmark *b) {
    for (int64_t large : {256, 2000, 3500}) {
        for (int64_t small : {1, 2, 3, 8, 32, 128}) {
            if (small < large) b->Args({small, large});
        }
    }
    b->Args({500, 2000});
    b->Args({2000, 2000});
}

}  // namespace

BENCHMARK(BinaryOp<roaring_bitmap_or, true>)->Name("Or")->Apply(Sizes);
BENCHMARK(OrInplace)->Apply(Sizes);
BENCHMARK(BinaryOp<roaring_bitmap_andnot, false>)
    ->Name("AndnotLargeMinusSmall")
    ->Apply(Sizes);
BENCHMARK(BinaryOp<roaring_bitmap_andnot, true>)
    ->Name("AndnotSmallMinusLarge")
    ->Apply(Sizes);
BENCHMARK(BinaryOp<roaring_bitmap_xor, true>)->Name("Xor")->Apply(Sizes);
BENCHMARK(BinaryOp<roaring_bitmap_and, true>)->Name("And")->Apply(Sizes);

BENCHMARK_MAIN();
