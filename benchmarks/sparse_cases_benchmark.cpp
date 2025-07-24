#include <iostream>
#include <random>

#include <roaring/roaring.h>
#include <roaring/roaring64map.hh>

#include "benchmark.h"

// the same key will add to the same bitmap
static std::default_random_engine e;

void run_bench(size_t batch_size, int loop_count, size_t max) {
    using namespace roaring;
    using roaring::Roaring;
    std::uniform_int_distribution<uint64_t> dist(0, max);
    uint64_t cycles_start, cycles_final;
    RDTSC_START(cycles_start);
    for (int j = 0; j < loop_count; ++j) {
        Roaring64Map bitmap_64;
        for (size_t i = 0; i < batch_size; i++) {
            bitmap_64.add(dist(e));
        }
    }

    RDTSC_FINAL(cycles_final);
    std::cout << "batch_size=" << batch_size << ", max=" << max << " costs:"
              << (cycles_final - cycles_start) * 1.0 / batch_size / loop_count
              << std::endl;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    run_bench(100, 10, 100);
    run_bench(100, 10, 1000);
    run_bench(100, 10, 1000000);
    run_bench(100, 10, 100000000);
    run_bench(100, 10, 10000000000);

    run_bench(100000, 10, 1000);
    run_bench(100000, 10, 100000);
    run_bench(100000, 10, 1000000000);
    run_bench(100000, 10, 100000000000);

    run_bench(100000000, 1, 100000000);
    run_bench(100000000, 1, 500000000);
    run_bench(100000000, 1, 5000000000);

    return 0;
}
