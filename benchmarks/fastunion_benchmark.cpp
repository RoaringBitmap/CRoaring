#include <iostream>
#include <stdio.h>

#include <roaring/roaring.h>

#include "benchmark.h"
#include "roaring64map.hh"

using roaring::Roaring64Map;

namespace {
const uint32_t num_iterations = 10;

const uint32_t num_bitmaps = 100;
const uint32_t num_outer_slots = 1000;
const uint32_t num_inner_values = 2000;

/**
 * Creates the input maps for the test. This method creates 'num_bitmaps' maps,
 * each of which contains 'num_outer_slots' 32-bit Roarings, each of which
 * contains 'num_inner_values' bits. The inner bits are separated by
 * 'num_bitmaps' and their starting offset is offset by 1 from one bitmap to the
 * next. The intent is that in the result of the union, all the bits in a given
 * 32 bit Roaring slot will end up densely packed together, which seemed like an
 * interesting thing to do.
 */
std::vector<Roaring64Map> makeMaps() {
    std::vector<Roaring64Map> result;
    for (uint32_t bm_index = 0; bm_index != num_bitmaps; ++bm_index) {
        Roaring64Map roaring;

        for (uint32_t slot = 0; slot != num_outer_slots; ++slot) {
            auto value = (uint64_t(slot) << 32) + bm_index + 0x98765432;
            for (uint32_t inner_index = 0; inner_index != num_inner_values;
                 ++inner_index) {
                roaring.add(value);
                value += num_bitmaps;
            }
        }
        result.push_back(std::move(roaring));
    }
    return result;
}

Roaring64Map legacy_fastunion(size_t n, const Roaring64Map **inputs) {
    Roaring64Map ans;
    // not particularly fast
    for (size_t lcv = 0; lcv < n; ++lcv) {
        ans |= *(inputs[lcv]);
    }
    return ans;
}

void benchmarkLegacyFastUnion() {
    std::cout << "*** Legacy fastunion ***\n";
    auto maps = makeMaps();

    // Need pointers to the above
    std::vector<const Roaring64Map *> result_ptrs;
    for (auto &map : maps) {
        result_ptrs.push_back(&map);
    }

    for (uint32_t iter = 0; iter < num_iterations; ++iter) {
        uint64_t cycles_start, cycles_final;
        RDTSC_START(cycles_start);
        auto result = legacy_fastunion(result_ptrs.size(), result_ptrs.data());
        RDTSC_FINAL(cycles_final);

        auto num_cycles = cycles_final - cycles_start;
        uint64_t cycles_per_map = num_cycles / maps.size();
        std::cout << "Iteration " << iter << ": " << cycles_per_map
                  << " per map\n";
    }
}

void benchmarkNewFastUnion() {
    std::cout << "*** New fastunion() ***\n";
    auto maps = makeMaps();

    // Need pointers to the above
    std::vector<const Roaring64Map *> result_ptrs;
    for (auto &map : maps) {
        result_ptrs.push_back(&map);
    }

    for (uint32_t iter = 0; iter < num_iterations; ++iter) {
        uint64_t cycles_start, cycles_final;
        RDTSC_START(cycles_start);
        auto result =
            Roaring64Map::fastunion(result_ptrs.size(), result_ptrs.data());
        RDTSC_FINAL(cycles_final);

        auto num_cycles = cycles_final - cycles_start;
        uint64_t cycles_per_map = num_cycles / maps.size();
        std::cout << "Iteration " << iter << ": " << cycles_per_map
                  << " per map\n";
    }
}
}  // namespace

int main() {
    benchmarkLegacyFastUnion();
    benchmarkNewFastUnion();
}
