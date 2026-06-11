// This file is the "How to use the library? / The C++ API" example from the
// project README.md. It is part of the test suite so that the documented
// example is guaranteed to compile and run. Keep it in sync with the README.
//
// We undefine NDEBUG so that the assert() checks below are always active,
// even when the test suite is built in release mode.
#undef NDEBUG

#include <roaring/roaring.hh>
#include <roaring/roaring64map.hh>

#include <cassert>
#include <iostream>

using namespace roaring;

int main() {
    // --- 32-bit bitmaps ---
    Roaring r;
    r.add(1);
    r.add(100);
    r.add(1000);
    r.addRange(10, 20);  // adds the half-open range [10, 20)

    assert(r.contains(100));
    assert(!r.contains(50));
    std::cout << "32-bit cardinality = " << r.cardinality() << std::endl;

    // Construct a bitmap directly from a list of values.
    Roaring other = Roaring::bitmapOfList({100, 1000, 5000});

    // Operators return new bitmaps; their memory is managed for you.
    Roaring intersection = r & other;
    assert(intersection.cardinality() == 2);  // {100, 1000}

    // Range-based iteration visits the values in sorted (increasing) order.
    uint64_t sum = 0;
    for (uint32_t value : r) {
        sum += value;
    }
    std::cout << "sum of values = " << sum << std::endl;

    // --- 64-bit bitmaps ---
    Roaring64Map big;
    big.add(uint64_t(1));
    big.add(uint64_t(0xFFFFFFFFFFULL));  // a value beyond 32 bits
    assert(big.contains(uint64_t(0xFFFFFFFFFFULL)));
    std::cout << "64-bit cardinality = " << big.cardinality() << std::endl;

    return EXIT_SUCCESS;
}
