#include <benchmark/benchmark.h>
#include <random>
#include <set>
#include <array>

#include "performancecounters/event_counter.h"
#include "roaring/roaring64.h"
#include "roaring/roaring64map.hh"

namespace roaring {

const auto kCountAndDensityRange = {
    benchmark::CreateRange(1000, 1000000, /*multi=*/10),
    benchmark::CreateRange(1, uint64_t{1} << 48,
                           /*multi=*/256)};

// Bitmasks with 20 bits set, spread out over: 20, 32, 48, 64 bits.
//
// These bitmasks make it so that the set size is bounded, and the hit rate is
// high, while also changing density at different bit orders. With 2^20 random
// elements inserted, the hit rate is ~63% due to the overlap in elements
// inserted.
constexpr std::array<uint64_t, 10> kBitmasks = {
    // 20 bit spread
    0x00000000000FFFFF,
    0x0000000FFFFF0000,
    0x000FFFFF00000000,
    0xFFFFF00000000000,
    // 32 bit spread
    0x000000005DBFC83E,
    0x00005DBFC83E0000,
    0x5DBFC83E00000000,
    // 48 bit spread
    0x0000493B189604B6,
    0x493B189604B60000,
    // 64 bit spread
    0x420C684950A2D088,
};

std::random_device rd;
std::mt19937 gen(rd());

uint64_t randUint64() {
    return std::uniform_int_distribution<uint64_t>(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max())(gen);
}

static void r64ContainsHit(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        roaring64_bitmap_add(r, val);
    }
    size_t i = 0;
    for (auto _ : state) {
        uint64_t val = i * step;
        i = (i + 1) % count;
        benchmark::DoNotOptimize(roaring64_bitmap_contains(r, val));
    }
    roaring64_bitmap_free(r);
}
BENCHMARK(r64ContainsHit)->ArgsProduct({kCountAndDensityRange});

static void cppContainsHit(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    Roaring64Map r;
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        r.add(val);
    }
    size_t i = 0;
    for (auto _ : state) {
        uint64_t val = i * step;
        i = (i + 1) % count;
        benchmark::DoNotOptimize(r.contains(val));
    }
}
BENCHMARK(cppContainsHit)->ArgsProduct({kCountAndDensityRange});

static void setContainsHit(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    std::set<uint64_t> set;
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        set.insert(val);
    }
    size_t i = 0;
    for (auto _ : state) {
        uint64_t val = i * step;
        i = (i + 1) % count;
        benchmark::DoNotOptimize(set.find(val) != set.end());
    }
}
BENCHMARK(setContainsHit)->ArgsProduct({kCountAndDensityRange});

static void r64ContainsMiss(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        roaring64_bitmap_add(r, val);
    }
    size_t i = 0;
    for (auto _ : state) {
        uint64_t val = (i + 1) * step - 1;
        i = (i + 1) % count;
        benchmark::DoNotOptimize(roaring64_bitmap_contains(r, val));
    }
    roaring64_bitmap_free(r);
}
BENCHMARK(r64ContainsMiss)->ArgsProduct({kCountAndDensityRange});

static void cppContainsMiss(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    Roaring64Map r;
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        r.add(val);
    }
    size_t i = 0;
    for (auto _ : state) {
        uint64_t val = (i + 1) * step - 1;
        i = (i + 1) % count;
        benchmark::DoNotOptimize(r.contains(val));
    }
}
BENCHMARK(cppContainsMiss)->ArgsProduct({kCountAndDensityRange});

static void setContainsMiss(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    std::set<uint64_t> set;
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        set.insert(val);
    }
    size_t i = 0;
    for (auto _ : state) {
        uint64_t val = (i + 1) * step - 1;
        i = (i + 1) % count;
        benchmark::DoNotOptimize(set.find(val) != set.end());
    }
}
BENCHMARK(setContainsMiss)->ArgsProduct({kCountAndDensityRange});

static void r64ContainsRandom(benchmark::State& state) {
    uint64_t bitmask = kBitmasks[state.range(0)];
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (size_t i = 0; i < (1 << 20); ++i) {
        uint64_t val = randUint64() & bitmask;
        roaring64_bitmap_add(r, val);
    }
    for (auto _ : state) {
        uint64_t val = randUint64() & bitmask;
        benchmark::DoNotOptimize(roaring64_bitmap_contains(r, val));
    }
    roaring64_bitmap_free(r);
}
BENCHMARK(r64ContainsRandom)->DenseRange(0, kBitmasks.size() - 1, 1);

static void cppContainsRandom(benchmark::State& state) {
    uint64_t bitmask = kBitmasks[state.range(0)];
    Roaring64Map r;
    for (size_t i = 0; i < (1 << 20); ++i) {
        uint64_t val = randUint64() & bitmask;
        r.add(val);
    }
    for (auto _ : state) {
        uint64_t val = randUint64() & bitmask;
        benchmark::DoNotOptimize(r.contains(val));
    }
}
BENCHMARK(cppContainsRandom)->DenseRange(0, kBitmasks.size() - 1, 1);

static void setContainsRandom(benchmark::State& state) {
    uint64_t bitmask = kBitmasks[state.range(0)];
    std::set<uint64_t> set;
    for (size_t i = 0; i < (1 << 20); ++i) {
        uint64_t val = randUint64() & bitmask;
        set.insert(val);
    }
    for (auto _ : state) {
        uint64_t val = randUint64() & bitmask;
        benchmark::DoNotOptimize(set.find(val) != set.end());
    }
}
BENCHMARK(setContainsRandom)->DenseRange(0, kBitmasks.size() - 1, 1);

static void r64Insert(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    for (auto _ : state) {
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        for (size_t i = 0; i < count; ++i) {
            uint64_t val = i * step;
            roaring64_bitmap_add(r, val);
        }
        roaring64_bitmap_free(r);
    }
    state.SetItemsProcessed(count);
}
BENCHMARK(r64Insert)->ArgsProduct({kCountAndDensityRange});

static void cppInsert(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    for (auto _ : state) {
        Roaring64Map r;
        for (size_t i = 0; i < count; ++i) {
            uint64_t val = i * step;
            r.add(val);
        }
    }
    state.SetItemsProcessed(count);
}
BENCHMARK(cppInsert)->ArgsProduct({kCountAndDensityRange});

static void setInsert(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    for (auto _ : state) {
        std::set<uint64_t> set;
        for (size_t i = 0; i < count; ++i) {
            uint64_t val = i * step;
            set.insert(val);
        }
    }
    state.SetItemsProcessed(count);
}
BENCHMARK(setInsert)->ArgsProduct({kCountAndDensityRange});

static void r64Remove(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    for (auto _ : state) {
        state.PauseTiming();
        roaring64_bitmap_t* r = roaring64_bitmap_create();
        for (size_t i = 0; i < count; ++i) {
            uint64_t val = i * step;
            roaring64_bitmap_add(r, val);
        }
        state.ResumeTiming();
        for (size_t i = 0; i < count; ++i) {
            uint64_t val = i * step;
            roaring64_bitmap_remove(r, val);
        }
        state.PauseTiming();
        roaring64_bitmap_free(r);
    }
    state.SetItemsProcessed(count);
}
BENCHMARK(r64Remove)->ArgsProduct({kCountAndDensityRange});

static void cppRemove(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    for (auto _ : state) {
        state.PauseTiming();
        Roaring64Map r;
        for (size_t i = 0; i < count; ++i) {
            uint64_t val = i * step;
            r.add(val);
        }
        state.ResumeTiming();
        for (size_t i = 0; i < count; ++i) {
            uint64_t val = i * step;
            r.remove(val);
        }
        state.PauseTiming();
    }
    state.SetItemsProcessed(count);
}
BENCHMARK(cppRemove)->ArgsProduct({kCountAndDensityRange});

static void setRemove(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    for (auto _ : state) {
        state.PauseTiming();
        std::set<uint64_t> set;
        for (size_t i = 0; i < count; ++i) {
            uint64_t val = i * step;
            set.insert(val);
        }
        state.ResumeTiming();
        for (size_t i = 0; i < count; ++i) {
            uint64_t val = i * step;
            set.erase(val);
        }
        state.PauseTiming();
    }
    state.SetItemsProcessed(count);
}
BENCHMARK(setRemove)->ArgsProduct({kCountAndDensityRange});

static void r64InsertRemoveRandom(benchmark::State& state) {
    uint64_t bitmask = kBitmasks[state.range(0)];
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (size_t i = 0; i < (1 << 20); ++i) {
        uint64_t val = randUint64() & bitmask;
        roaring64_bitmap_add(r, val);
    }
    for (auto _ : state) {
        uint64_t val1 = randUint64() & bitmask;
        uint64_t val2 = randUint64() & bitmask;
        roaring64_bitmap_add(r, val1);
        roaring64_bitmap_remove(r, val2);
    }
    roaring64_bitmap_free(r);
}
BENCHMARK(r64InsertRemoveRandom)->DenseRange(0, kBitmasks.size() - 1, 1);

static void cppInsertRemoveRandom(benchmark::State& state) {
    uint64_t bitmask = kBitmasks[state.range(0)];
    Roaring64Map r;
    for (size_t i = 0; i < (1 << 20); ++i) {
        uint64_t val = randUint64() & bitmask;
        r.add(val);
    }
    for (auto _ : state) {
        uint64_t val1 = randUint64() & bitmask;
        uint64_t val2 = randUint64() & bitmask;
        r.add(val1);
        r.remove(val2);
    }
}
BENCHMARK(cppInsertRemoveRandom)->DenseRange(0, kBitmasks.size() - 1, 1);

static void setInsertRemoveRandom(benchmark::State& state) {
    uint64_t bitmask = kBitmasks[state.range(0)];
    std::set<uint64_t> set;
    for (size_t i = 0; i < (1 << 20); ++i) {
        uint64_t val = randUint64() & bitmask;
        set.insert(val);
    }
    for (auto _ : state) {
        uint64_t val1 = randUint64() & bitmask;
        uint64_t val2 = randUint64() & bitmask;
        set.insert(val1);
        set.erase(val2);
    }
}
BENCHMARK(setInsertRemoveRandom)->DenseRange(0, kBitmasks.size() - 1, 1);

static void r64PortableSerialize(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        roaring64_bitmap_add(r, val);
    }
    size_t size = roaring64_bitmap_portable_size_in_bytes(r);
    std::vector<char> buf(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(
            roaring64_bitmap_portable_serialize(r, buf.data()));
    }
    state.SetItemsProcessed(count);
    state.SetBytesProcessed(size);
    roaring64_bitmap_free(r);
}
BENCHMARK(r64PortableSerialize)->ArgsProduct({kCountAndDensityRange});

static void r64FrozenSerialize(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    roaring64_bitmap_t* r = roaring64_bitmap_create();
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        roaring64_bitmap_add(r, val);
    }
    roaring64_bitmap_shrink_to_fit(r);
    size_t size = roaring64_bitmap_frozen_size_in_bytes(r);
    std::vector<char> buf(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(
            roaring64_bitmap_frozen_serialize(r, buf.data()));
    }
    state.SetItemsProcessed(count);
    state.SetBytesProcessed(size);
    roaring64_bitmap_free(r);
}
BENCHMARK(r64FrozenSerialize)->ArgsProduct({kCountAndDensityRange});

static void cppPortableSerialize(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    Roaring64Map r;
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        r.add(val);
    }
    size_t size = r.getSizeInBytes(/*portable=*/true);
    std::vector<char> buf(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(r.write(buf.data(), /*portable=*/true));
    }
    state.SetItemsProcessed(count);
    state.SetBytesProcessed(size);
}
BENCHMARK(cppPortableSerialize)->ArgsProduct({kCountAndDensityRange});

static void cppFrozenSerialize(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    Roaring64Map r;
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        r.add(val);
    }
    size_t size = r.getFrozenSizeInBytes();
    // TODO: there seems to be a bug in writeFrozen that causes writes beyond
    // getFrozenSizeInBytes()
    std::vector<char> buf(size * 2);
    for (auto _ : state) {
        r.writeFrozen(buf.data());
        benchmark::DoNotOptimize(buf);
    }
    state.SetItemsProcessed(count);
    state.SetBytesProcessed(size);
}
BENCHMARK(cppFrozenSerialize)->ArgsProduct({kCountAndDensityRange});

static void r64PortableDeserialize(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        roaring64_bitmap_add(r1, val);
    }
    size_t size = roaring64_bitmap_portable_size_in_bytes(r1);
    std::vector<char> buf(size);
    roaring64_bitmap_portable_serialize(r1, buf.data());
    roaring64_bitmap_free(r1);
    for (auto _ : state) {
        auto r2 = roaring64_bitmap_portable_deserialize_safe(buf.data(), size);
        benchmark::DoNotOptimize(r2);
        roaring64_bitmap_free(r2);
    }
    state.SetItemsProcessed(count);
    state.SetBytesProcessed(size);
}
BENCHMARK(r64PortableDeserialize)->ArgsProduct({kCountAndDensityRange});

static void r64FrozenDeserialize(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    roaring64_bitmap_t* r1 = roaring64_bitmap_create();
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        roaring64_bitmap_add(r1, val);
    }
    roaring64_bitmap_shrink_to_fit(r1);
    size_t size = roaring64_bitmap_frozen_size_in_bytes(r1);
    char* buf = (char*)aligned_alloc(64, size);
    roaring64_bitmap_frozen_serialize(r1, buf);
    roaring64_bitmap_free(r1);
    for (auto _ : state) {
        auto r2 = roaring64_bitmap_frozen_view(buf, size);
        benchmark::DoNotOptimize(r2);
        roaring64_bitmap_free(r2);
    }
    free(buf);
    state.SetItemsProcessed(count);
    state.SetBytesProcessed(size);
}
BENCHMARK(r64FrozenDeserialize)->ArgsProduct({kCountAndDensityRange});

static void cppPortableDeserialize(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    Roaring64Map r1;
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        r1.add(val);
    }
    size_t size = r1.getSizeInBytes(/*portable=*/true);
    std::vector<char> buf(size);
    r1.write(buf.data(), /*portable=*/true);
    for (auto _ : state) {
        auto r2 = Roaring64Map::read(buf.data(), /*portable=*/true);
        benchmark::DoNotOptimize(r2);
    }
    state.SetItemsProcessed(count);
    state.SetBytesProcessed(size);
}
BENCHMARK(cppPortableDeserialize)->ArgsProduct({kCountAndDensityRange});

static void cppFrozenDeserialize(benchmark::State& state) {
    size_t count = state.range(0);
    uint64_t step = state.range(1);
    Roaring64Map r1;
    for (size_t i = 0; i < count; ++i) {
        uint64_t val = i * step;
        r1.add(val);
    }
    size_t size = r1.getFrozenSizeInBytes();
    // TODO: there seems to be a bug in writeFrozen that causes writes beyond
    // getFrozenSizeInBytes()
    std::vector<char> buf(size * 2);
    r1.writeFrozen(buf.data());
    for (auto _ : state) {
        auto r2 = Roaring64Map::frozenView(buf.data());
        benchmark::DoNotOptimize(r2);
    }
    state.SetItemsProcessed(count);
    state.SetBytesProcessed(size);
}
BENCHMARK(cppFrozenDeserialize)->ArgsProduct({kCountAndDensityRange});

}  // namespace roaring

BENCHMARK_MAIN();
