// Container slots of 64-bit bitmaps. A roaring64_bitmap_t keeps its
// containers in an array; removing the only value of a container frees its
// slot, and adding a value to a new container takes the lowest free slot.
// Finding the next free slot must not cost a scan of the whole array.
//
// The bitmaps hold one value per container, so that every remove and every
// add frees or takes a slot. "RemoveAdd" removes a value and adds it back.
// "RemoveAddBatch" removes 64 values, then adds them back, so that the
// bitmap has several free slots at once. Both cycle through many values so
// that the branch predictor cannot learn them.
#include <algorithm>
#include <benchmark/benchmark.h>
#include <random>
#include <vector>

#include "roaring/roaring64.h"

namespace {

struct SlotBitmap {
    roaring64_bitmap_t *r;
    std::vector<uint64_t> values;  // present values, shuffled
};

SlotBitmap make_bitmap(uint64_t containers) {
    std::mt19937_64 gen(containers);
    SlotBitmap s;
    s.r = roaring64_bitmap_create();
    for (uint64_t i = 0; i < containers; ++i) {
        uint64_t v = (i << 16) | (gen() & 0xFFFF);
        roaring64_bitmap_add(s.r, v);
        s.values.push_back(v);
    }
    std::shuffle(s.values.begin(), s.values.end(), gen);
    s.values.resize(std::min<size_t>(s.values.size(), 4096));
    return s;
}

void RemoveAdd(benchmark::State &state) {
    SlotBitmap s = make_bitmap((uint64_t)state.range(0));
    size_t i = 0;
    for (auto _ : state) {
        uint64_t v = s.values[i];
        roaring64_bitmap_remove(s.r, v);
        roaring64_bitmap_add(s.r, v);
        i = (i + 1 == s.values.size()) ? 0 : i + 1;
    }
    state.SetItemsProcessed(state.iterations());
    roaring64_bitmap_free(s.r);
}

void RemoveAddBatch(benchmark::State &state) {
    SlotBitmap s = make_bitmap((uint64_t)state.range(0));
    const size_t batch = std::min<size_t>(64, s.values.size());
    size_t start = 0;
    for (auto _ : state) {
        for (size_t k = 0; k < batch; ++k) {
            roaring64_bitmap_remove(s.r,
                                    s.values[(start + k) % s.values.size()]);
        }
        for (size_t k = 0; k < batch; ++k) {
            roaring64_bitmap_add(s.r, s.values[(start + k) % s.values.size()]);
        }
        start = (start + batch) % s.values.size();
    }
    state.SetItemsProcessed(state.iterations() * (int64_t)batch);
    roaring64_bitmap_free(s.r);
}

}  // namespace

BENCHMARK(RemoveAdd)->RangeMultiplier(8)->Range(1 << 10, 1 << 19);
BENCHMARK(RemoveAddBatch)->RangeMultiplier(8)->Range(1 << 10, 1 << 19);

BENCHMARK_MAIN();
