/**
 * Unit tests for the C++ roaring::Roaring64 class (ART-based 64-bit bitmap).
 */
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

#include <roaring/roaring64.h>  // C API for comparison
#include <roaring/roaring64.hh>
#include <roaring/roaring64map.hh>

#include "config.h"
#include "roaring64_checked.hh"
#include "test.h"

using roaring::Roaring64;
using roaring::Roaring64Map;

static_assert(std::is_nothrow_move_constructible<Roaring64>::value,
              "Expected Roaring64 to be nothrow move constructible");

DEFINE_TEST(test_cpp_r64_default_empty) {
    Roaring64 r;
    assert_true(r.isEmpty());
    assert_int_equal(r.cardinality(), 0);
}

DEFINE_TEST(test_cpp_r64_copy_and_move) {
    Roaring64 a;
    a.add(uint64_t(1) << 40);
    Roaring64 b(a);  // copy ctor
    assert_int_equal(b.cardinality(), 1);
    Roaring64 c(std::move(a));  // move ctor
    assert_int_equal(c.cardinality(), 1);
    // verify moved-from a is valid for reuse
    a = Roaring64{};
    a.add(99);
    assert_int_equal(a.cardinality(), 1);
    Roaring64 d;
    d = c;  // copy assign
    assert_int_equal(d.cardinality(), 1);
    Roaring64 e;
    e = std::move(c);  // move assign
    assert_int_equal(e.cardinality(), 1);
    // verify moved-from c is valid for reuse
    c = Roaring64{};
    c.add(42);
    assert_int_equal(c.cardinality(), 1);
    e.swap(d);
    assert_int_equal(e.cardinality(), 1);
}

DEFINE_TEST(test_cpp_r64_add_remove_contains_clear) {
    Roaring64 r;
    const uint64_t v = uint64_t(1) << 50;
    assert_false(r.contains(v));
    r.add(v);
    assert_true(r.contains(v));
    assert_int_equal(r.cardinality(), 1);
    r.remove(v);
    assert_false(r.contains(v));
    assert_true(r.isEmpty());
    r.add(1);
    r.add(2);
    r.clear();
    assert_true(r.isEmpty());
}

DEFINE_TEST(test_cpp_r64_add_remove_checked) {
    Roaring64 r;
    const uint64_t v = uint64_t(1) << 50;
    assert_true(r.addChecked(v));
    assert_false(r.addChecked(v));
    assert_true(r.contains(v));
    assert_true(r.removeChecked(v));
    assert_false(r.removeChecked(v));
    assert_true(r.isEmpty());
}

DEFINE_TEST(test_cpp_r64_add_remove_range) {
    const uint64_t K = uint64_t(1) << 32;

    Roaring64 r;
    r.addRange(K - 10, K + 10);  // spans two containers
    assert_int_equal(r.cardinality(), 20);
    assert_int_equal(r.minimum(), K - 10);
    assert_int_equal(r.maximum(), K + 9);

    r.addRangeClosed(K + 10, K + 19);
    assert_int_equal(r.cardinality(), 30);
    assert_int_equal(r.maximum(), K + 19);

    r.removeRange(K - 10, K);
    assert_int_equal(r.cardinality(), 20);
    assert_int_equal(r.minimum(), K);

    r.removeRangeClosed(K, K + 18);
    assert_true((r == Roaring64{K + 19}));

    // empty ranges are no-ops
    r.addRange(5, 5);
    r.addRangeClosed(6, 5);
    r.removeRange(K + 19, K + 19);
    r.removeRangeClosed(K + 20, K + 19);
    assert_true((r == Roaring64{K + 19}));

    // closed ranges can reach UINT64_MAX
    r.addRangeClosed(UINT64_MAX - 9, UINT64_MAX);
    assert_int_equal(r.cardinality(), 11);
    assert_true(r.contains(UINT64_MAX));
    r.removeRangeClosed(UINT64_MAX - 9, UINT64_MAX);
    assert_true((r == Roaring64{K + 19}));

    // full containers
    Roaring64 big;
    big.addRange(0, uint64_t(1) << 18);
    assert_int_equal(big.cardinality(), uint64_t(1) << 18);
    big.removeRange(1000, (uint64_t(1) << 18) - 1000);
    assert_int_equal(big.cardinality(), 2000);
    assert_true(big.contains(999));
    assert_false(big.contains(1000));
    assert_true(big.contains((uint64_t(1) << 18) - 1000));
}

DEFINE_TEST(test_cpp_r64_contains_range) {
    const uint64_t K = uint64_t(1) << 32;

    Roaring64 r;
    r.addRangeClosed(K - 10, K + 10);
    assert_true(r.containsRange(K - 10, K + 11));
    assert_false(r.containsRange(K - 11, K + 11));
    assert_false(r.containsRange(K - 10, K + 12));
    assert_true(r.containsRangeClosed(K - 10, K + 10));
    assert_false(r.containsRangeClosed(K - 10, K + 11));

    // empty ranges are always contained
    assert_true(r.containsRange(7, 7));
    assert_true(r.containsRangeClosed(8, 7));

    r.remove(K);
    assert_false(r.containsRangeClosed(K - 10, K + 10));
    assert_true(r.containsRange(K + 1, K + 11));

    r.addRangeClosed(UINT64_MAX - 4, UINT64_MAX);
    assert_true(r.containsRangeClosed(UINT64_MAX - 4, UINT64_MAX));
    assert_false(r.containsRangeClosed(UINT64_MAX - 5, UINT64_MAX));
}

DEFINE_TEST(test_cpp_r64_construction_helpers) {
    const uint64_t vals[] = {1, uint64_t(1) << 33, uint64_t(1) << 33, 7};
    Roaring64 r;
    r.addMany(4, vals);
    assert_int_equal(r.cardinality(), 3);

    Roaring64 il{1, 2, 3, 2};
    assert_int_equal(il.cardinality(), 3);

    Roaring64 bof =
        Roaring64::bitmapOf(3, uint64_t(10), uint64_t(20), uint64_t(30));
    assert_int_equal(bof.cardinality(), 3);
    assert_true(bof.contains(20));

    Roaring64 bol = Roaring64::bitmapOfList({10, 20, 20, uint64_t(1) << 40});
    assert_int_equal(bol.cardinality(), 3);
    assert_true(bol.contains(uint64_t(1) << 40));

    // ctor from a (n, data) array.
    const uint64_t data[] = {100, uint64_t(1) << 60, 100, 7};
    Roaring64 from_data(4, data);
    assert_int_equal(from_data.cardinality(), 3);
    assert_true(from_data.contains(uint64_t(1) << 60));
    assert_true(from_data.contains(7));
}

DEFINE_TEST(test_cpp_r64_minmax_equals) {
    // Empty bitmap returns sentinel values.
    Roaring64 empty;
    assert_true(empty.minimum() == UINT64_MAX);
    assert_true(empty.maximum() == 0);

    Roaring64 a{5, uint64_t(1) << 40, 7};
    assert_int_equal(a.minimum(), 5);
    assert_int_equal(a.maximum(), uint64_t(1) << 40);

    Roaring64 b{7, 5, uint64_t(1) << 40};
    assert_true(a == b);

    b.add(9);
    assert_false(a == b);
}

DEFINE_TEST(test_cpp_r64_set_ops) {
    const uint64_t K1 = uint64_t(1) << 16;
    const uint64_t K2 = uint64_t(1) << 32;

    Roaring64 a{1, 2, 7, K1 + 1, K1 + 5, K2 + 1};
    Roaring64 b{3, 4, 7, K1 + 5, K1 + 9, K2 + 2};
    //   a only: 1, 2, K1+1, K2+1
    //   b only: 3, 4, K1+9, K2+2
    //   both:   7, K1+5

    Roaring64 a_and_b = a & b;
    assert_true((a_and_b == Roaring64{7, K1 + 5}));

    Roaring64 a_or_b = a | b;
    assert_true((a_or_b == Roaring64{1, 2, 3, 4, 7, K1 + 1, K1 + 5, K1 + 9,
                                     K2 + 1, K2 + 2}));

    Roaring64 a_xor_b = a ^ b;
    assert_true(
        (a_xor_b == Roaring64{1, 2, 3, 4, K1 + 1, K1 + 9, K2 + 1, K2 + 2}));

    Roaring64 a_minus_b = a - b;
    assert_true((a_minus_b == Roaring64{1, 2, K1 + 1, K2 + 1}));

    Roaring64 c = a;
    c &= b;
    assert_true(c == a_and_b);
    c = a;
    c |= b;
    assert_true(c == a_or_b);
    c = a;
    c ^= b;
    assert_true(c == a_xor_b);
    c = a;
    c -= b;
    assert_true(c == a_minus_b);
}

DEFINE_TEST(test_cpp_r64_is_subset) {
    Roaring64 a{1, uint64_t(1) << 40};
    Roaring64 b{1, 7, uint64_t(1) << 40};
    Roaring64 empty;

    assert_true(a.isSubset(b));
    assert_true(a.isStrictSubset(b));
    assert_false(b.isSubset(a));
    assert_false(b.isStrictSubset(a));

    assert_true(a.isSubset(a));
    assert_false(a.isStrictSubset(a));

    assert_true(empty.isSubset(a));
    assert_true(empty.isStrictSubset(a));
    assert_true(empty.isSubset(empty));
    assert_false(empty.isStrictSubset(empty));

    a.add(uint64_t(1) << 50);
    assert_false(a.isSubset(b));
    assert_false(a.isStrictSubset(b));
}

DEFINE_TEST(test_cpp_r64_flip) {
    const uint64_t K = uint64_t(1) << 32;
    const uint64_t far = uint64_t(1) << 50;

    Roaring64 r{K - 2, K, K + 2, far};
    r.flip(K - 2, K + 2);
    assert_true((r == Roaring64{K - 1, K + 1, K + 2, far}));
    r.flipClosed(K - 2, K + 2);
    assert_true((r == Roaring64{K - 2, K, far}));

    // empty ranges are no-ops
    r.flip(9, 9);
    r.flipClosed(10, 9);
    assert_true((r == Roaring64{K - 2, K, far}));

    // closed ranges can reach UINT64_MAX
    Roaring64 e;
    e.flipClosed(UINT64_MAX - 9, UINT64_MAX);
    assert_int_equal(e.cardinality(), 10);
    assert_true(e.contains(UINT64_MAX));
    e.flipClosed(UINT64_MAX - 9, UINT64_MAX);
    assert_true(e.isEmpty());
}

DEFINE_TEST(test_cpp_r64_iteration) {
    Roaring64 r{uint64_t(1) << 40, 5, 7, 5};

    std::vector<uint64_t> seen;
    for (auto it = r.begin(); it != r.end(); ++it) {
        seen.push_back(*it);
    }
    // Iterates in ascending order, duplicates collapsed.
    std::vector<uint64_t> expected = {5, 7, uint64_t(1) << 40};
    // Also test iteration with postfix ++
    size_t postfix_count = 0;
    for (auto it = r.begin(); it != r.end(); it++) {
        ++postfix_count;
    }
    assert_int_equal(postfix_count, expected.size());
    assert_int_equal(seen.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert_int_equal(seen[i], expected[i]);
    }

    // Range-based for loop yields the same ascending results.
    std::vector<uint64_t> ranged;
    for (uint64_t x : r) {
        ranged.push_back(x);
    }
    assert_int_equal(ranged.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert_int_equal(ranged[i], expected[i]);
    }

    std::vector<uint64_t> arr(r.cardinality());
    r.toArray(arr.data());
    assert_int_equal(arr.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert_int_equal(arr[i], expected[i]);
    }

    // Empty bitmap: begin() == end().
    Roaring64 empty;
    assert_true(empty.begin() == empty.end());

    // Iterator copy assignment and move operations.
    Roaring64::const_iterator it1 = r.begin();  // at 5
    Roaring64::const_iterator it2 = r.end();
    it2 = it1;  // copy assignment
    assert_true(it2 == it1);
    assert_int_equal(*it2, uint64_t(5));

    Roaring64::const_iterator it3 = r.begin();
    ++it3;                                            // at 7
    Roaring64::const_iterator moved(std::move(it3));  // move construction
    assert_int_equal(*moved, uint64_t(7));

    Roaring64::const_iterator it4 = r.end();
    it4 = std::move(moved);  // move assignment
    assert_int_equal(*it4, uint64_t(7));
}

DEFINE_TEST(test_cpp_r64_bidirectional) {
    Roaring64 r{5, 7, 7, uint64_t(1) << 40, 5};

    assert_int_equal(*std::prev(r.end()), uint64_t(1) << 40);

    std::vector<uint64_t> backward;
    for (auto it = r.end(); it != r.begin();) {
        --it;
        backward.push_back(*it);
    }
    std::vector<uint64_t> expected = {uint64_t(1) << 40, 7, 5};
    assert_int_equal(backward.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert_int_equal(backward[i], expected[i]);
    }

    auto m = r.begin();
    ++m;
    ++m;
    --m;
    assert_int_equal(*m, uint64_t(7));

    auto p = r.end();
    --p;
    auto q = p--;
    assert_int_equal(*q, uint64_t(1) << 40);
    assert_int_equal(*p, uint64_t(7));

    // reverse over empty yields nothing
    Roaring64 empty;
    std::vector<uint64_t> rev_empty;
    for (auto it = empty.end(); it != empty.begin();) {
        --it;
        rev_empty.push_back(*it);
    }
    assert_true(rev_empty.empty());
}

DEFINE_TEST(test_cpp_r64_run_optimize) {
    Roaring64 r;
    const uint64_t sparse = uint64_t(1) << 40;
    r.add(sparse);
    assert_false(r.runOptimize());

    for (uint64_t i = 0; i < 30000; ++i) {
        r.add(i);
    }
    assert_true(r.runOptimize());

    assert_int_equal(r.cardinality(), 30001);
    assert_true(r.contains(0));
    assert_true(r.contains(29999));
    assert_false(r.contains(30000));
    assert_true(r.contains(sparse));
}

// Self-aliasing operators: r &= r and r |= r leave r unchanged; the binary
// operators on (r, r) give r or the empty set. (`r ^= r` and `r -= r` are not
// allowed by the C API, see roaring64_bitmap_xor_inplace.)
DEFINE_TEST(test_cpp_r64_self_aliasing) {
    const uint64_t K = uint64_t(1) << 32;
    Roaring64 r;
    r.addRange(1, 11);
    r.runOptimize();
    r.add(K + 3);
    r.add(UINT64_MAX);
    const Roaring64 orig = r;
    r &= r;
    assert_true(r == orig);
    r |= r;
    assert_true(r == orig);
    assert_true((r & r) == orig);
    assert_true((r | r) == orig);
    assert_true((r ^ r).isEmpty());
    assert_true((r - r).isEmpty());
    assert_true(r.isSubset(r));
    assert_false(r.isStrictSubset(r));
    assert_true(r == orig);
}

// runOptimize on a run container fragmented into length-1 runs returns false
// and shrinks the bitmap, while preserving equality.
DEFINE_TEST(test_cpp_r64_run_optimize_recovers_space) {
    Roaring64 r;
    r.addRange(0, 100);
    assert_true(r.runOptimize());
    for (uint64_t v = 200; v < 65536; v += 2) {
        r.add(v);
    }
    const Roaring64 expected = r;
    const size_t before = r.getSizeInBytes();
    assert_false(r.runOptimize());
    assert_true(r.getSizeInBytes() < before / 10);
    assert_true(r == expected);
    assert_int_equal(r.cardinality(), 100 + (65536 - 200) / 2);
}

DEFINE_TEST(test_cpp_r64_remove_run_compression) {
    Roaring64 r;
    for (uint64_t i = 0; i < 30000; ++i) {
        r.add(i);
    }
    assert_true(r.runOptimize());
    assert_true(r.removeRunCompression());
    assert_false(r.removeRunCompression());
    assert_true(r.runOptimize());

    assert_int_equal(r.cardinality(), 30000);
    assert_true(r.contains(0));
    assert_true(r.contains(29999));
    assert_false(r.contains(30000));
}

DEFINE_TEST(test_cpp_r64_shrink_to_fit) {
    Roaring64 r;
    for (uint64_t i = 0; i < 1000; ++i) {
        r.add(i << 32);  // one container per value
    }
    for (uint64_t i = 500; i < 1000; ++i) {
        r.remove(i << 32);
    }
    assert_true(r.shrinkToFit() > 0);  // slack from the removed containers

    assert_int_equal(r.cardinality(), 500);
    assert_true(r.contains(0));
    assert_true(r.contains(uint64_t(499) << 32));
    assert_false(r.contains(uint64_t(500) << 32));

    assert_int_equal(r.shrinkToFit(), 0);
}

DEFINE_TEST(test_cpp_r64_run_compression) {
    Roaring64 r;
    for (uint64_t i = 100; i <= 10000; i++) {
        r.add(i);
    }
    size_t size_origin = r.getSizeInBytes();
    bool has_run = r.runOptimize();
    size_t size_optimized = r.getSizeInBytes();
    assert_true(has_run);
    assert_true(size_origin > size_optimized);
    bool removed = r.removeRunCompression();
    assert_true(removed);
    size_t size_removed = r.getSizeInBytes();
    assert_true(size_removed > size_optimized);
}

DEFINE_TEST(test_cpp_r64_serialization) {
    Roaring64 r{1, 2, uint64_t(1) << 33, uint64_t(1) << 50,
                14000000000000000100ull};

    size_t expected = r.getSizeInBytes();
    std::vector<char> buf(expected);
    assert_int_equal(r.write(buf.data()), expected);

    assert_true(Roaring64::readSafe(buf.data(), buf.size()) == r);
    assert_true(Roaring64::read(buf.data()) == r);

    assert_int_equal(
        Roaring64::serializedSizeInBytesSafe(buf.data(), buf.size()), expected);
    // a truncated buffer is rejected
    assert_int_equal(
        Roaring64::serializedSizeInBytesSafe(buf.data(), buf.size() - 1), 0);
}

DEFINE_TEST(test_cpp_r64_serialization_empty) {
    Roaring64 empty;
    std::vector<char> buf(empty.getSizeInBytes());
    assert_int_equal(empty.write(buf.data()), buf.size());

    Roaring64 back = Roaring64::readSafe(buf.data(), buf.size());
    assert_true(back.isEmpty());
    assert_true(back == empty);
}

DEFINE_TEST(test_cpp_r64_serialization_containers) {
    Roaring64 r;
    r.add(uint64_t(1) << 50);  // array container
    for (uint64_t i = 0; i < 20000; i += 2) {
        r.add(i);  // bitset container
    }
    for (uint64_t i = 100000; i < 130000; ++i) {
        r.add(i);  // run container
    }
    assert_true(r.runOptimize());

    std::vector<char> buf(r.getSizeInBytes());
    assert_int_equal(r.write(buf.data()), buf.size());
    assert_true(Roaring64::readSafe(buf.data(), buf.size()) == r);
}

DEFINE_TEST(test_cpp_r64_serialization_matches_roaring64map) {
    // the portable format is shared, so the buffers are interchangeable
    const uint64_t vals[] = {1, 2, uint64_t(1) << 33, uint64_t(1) << 50,
                             14000000000000000100ull};
    Roaring64 r(5, vals);
    Roaring64Map m;
    for (uint64_t v : vals) {
        m.add(v);
    }

    std::vector<char> from_r(r.getSizeInBytes());
    r.write(from_r.data());
    std::vector<char> from_m(m.getSizeInBytes());
    m.write(from_m.data());

    assert_int_equal(from_r.size(), from_m.size());
    assert_true(memcmp(from_r.data(), from_m.data(), from_r.size()) == 0);

    Roaring64Map m_read = Roaring64Map::readSafe(from_r.data(), from_r.size());
    assert_true(m_read == m);

    Roaring64 r_read = Roaring64::readSafe(from_m.data(), from_m.size());
    assert_true(r_read == r);
}

// Returns true on success, false on exception. The files were written by
// Roaring64Map, which shares the portable format.
static bool deserialize64(const std::string& filename) {
    std::ifstream in(TEST_DATA_DIR + filename, std::ios::binary);
    std::vector<char> buf1(std::istreambuf_iterator<char>(in), {});
    Roaring64 r;
#if ROARING_EXCEPTIONS
    try {
        r = Roaring64::readSafe(buf1.data(), buf1.size());
    } catch (...) {
        return false;
    }
#else   // ROARING_EXCEPTIONS
    r = Roaring64::readSafe(buf1.data(), buf1.size());
#endif  // ROARING_EXCEPTIONS
    std::vector<char> buf2(r.getSizeInBytes());
    assert_int_equal(buf1.size(), buf2.size());
    assert_int_equal(r.write(buf2.data()), buf2.size());
    assert_memory_equal(buf1.data(), buf2.data(), buf1.size());
    return true;
}

DEFINE_TEST(test_cpp_r64_deserialize_empty) {
    assert_true(deserialize64("64mapempty.bin"));
}

DEFINE_TEST(test_cpp_r64_deserialize_32bit_vals) {
    assert_true(deserialize64("64map32bitvals.bin"));
}

DEFINE_TEST(test_cpp_r64_deserialize_spread_vals) {
    assert_true(deserialize64("64mapspreadvals.bin"));
}

DEFINE_TEST(test_cpp_r64_deserialize_high_vals) {
    assert_true(deserialize64("64maphighvals.bin"));
}

DEFINE_TEST(test_cpp_r64_deserialize_run_bitset_vals) {
    assert_true(deserialize64("64maprunbitsetvals.bin"));
}

#if ROARING_EXCEPTIONS
DEFINE_TEST(test_cpp_r64_deserialize_empty_input) {
    assert_false(deserialize64("64mapemptyinput.bin"));
}

DEFINE_TEST(test_cpp_r64_deserialize_size_too_small) {
    assert_false(deserialize64("64mapsizetoosmall.bin"));
}

DEFINE_TEST(test_cpp_r64_deserialize_invalid_size) {
    assert_false(deserialize64("64mapinvalidsize.bin"));
}

DEFINE_TEST(test_cpp_r64_deserialize_key_too_small) {
    assert_false(deserialize64("64mapkeytoosmall.bin"));
}
#endif

DEFINE_TEST(test_cpp_r64_random_vs_set) {
    doublechecked::Roaring64 r;
    std::vector<uint64_t> added;  // values that have been added

    uint64_t seed = 987654321;
    auto next = [&seed]() {
        seed = 6364136223846793005ULL * seed + 1442695040888963407ULL;
        return seed;
    };

    const uint64_t seedvals[] = {1, uint64_t(1) << 40, uint64_t(1) << 50,
                                 12345};
    r.addMany(4, seedvals);
    for (uint64_t v : seedvals) added.push_back(v);

    for (size_t i = 0; i < 5000; ++i) {
        if (!added.empty() && (next() & 3) == 0) {  // 25%
            // Remove an existing value.
            size_t idx = next() % added.size();
            uint64_t v = added[idx];
            r.remove(v);
            added[idx] = added.back();
            added.pop_back();
        } else {  // 75%
            // Add a new value.
            uint64_t high48 =
                next() & 0xFFFFFFFFFFFFULL;    // 48-bit container key
            uint64_t low16 = next() & 0xFFFF;  // 16-bit container offset
            uint64_t v = (high48 << 16) | low16;
            r.add(v);
            added.push_back(v);
        }
        // Periodically apply a post-processing step to the bitset
        switch (next() % 15) {
            case 0:
                r.removeRunCompression();
                break;
            case 1:
                r.runOptimize();
                break;
            case 2:
                r.shrinkToFit();
                break;
            default:
                break;
        }
        if ((i % 500) == 0) {
            (void)r.cardinality();
            if (!added.empty()) {
                (void)r.contains(added[next() % added.size()]);
            }
            (void)r.isEmpty();
            r.validate();
        }
    }
    r.validate();  // manually validate

    std::vector<char> buf(r.getSizeInBytes());
    assert_int_equal(r.write(buf.data()), buf.size());
    assert_true(Roaring64::readSafe(buf.data(), buf.size()) == r.plain);

    // exercise clear
    r.clear();
    assert_true(r.isEmpty());
    r.validate();
}

DEFINE_TEST(test_cpp_r64_random_range_ops_vs_set) {
    doublechecked::Roaring64 r;

    uint64_t seed = 123456789;
    auto next = [&seed]() {
        seed = 6364136223846793005ULL * seed + 1442695040888963407ULL;
        return seed;
    };

    // Ranges start near container and key boundaries, and near the top of the
    // 64-bit space, so that they overlap and span containers.
    const uint64_t bases[] = {0, (uint64_t(1) << 16) - 512,
                              (uint64_t(1) << 32) - 512,
                              (uint64_t(1) << 48) - 512, UINT64_MAX - 1024};
    const size_t num_bases = sizeof(bases) / sizeof(bases[0]);

    for (size_t i = 0; i < 1000; ++i) {
        uint64_t min = bases[next() % num_bases] + next() % 1024;
        // mostly short ranges, some long enough to fill bitset containers
        uint64_t width = (next() % 8 == 0) ? next() % 10000 : next() % 300;
        uint64_t max = (UINT64_MAX - min < width) ? UINT64_MAX : min + width;

        const bool check_subset = (i % 16) == 0;
        doublechecked::Roaring64 before;
        if (check_subset) {
            before = r;
        }

        switch (next() % 8) {
            case 0:
                r.addRange(min, max);
                break;
            case 1:
                r.addRangeClosed(min, max);
                break;
            case 2:
                r.removeRange(min, max);
                break;
            case 3:
                r.removeRangeClosed(min, max);
                break;
            case 4:
                r.flip(min, max);
                break;
            case 5:
                r.flipClosed(min, max);
                break;
            case 6:
                (void)r.addChecked(min);
                break;
            default:
                (void)r.removeChecked(min);
                break;
        }
        if (next() % 10 == 0) {
            r.runOptimize();
        }

        (void)r.containsRange(min, max);
        (void)r.containsRangeClosed(min, max);
        if (check_subset) {
            (void)before.isSubset(r);
            (void)r.isSubset(before);
            (void)before.isStrictSubset(r);
            (void)r.isStrictSubset(before);
            r.validate();
        }
    }
    r.validate();
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cpp_r64_default_empty),
        cmocka_unit_test(test_cpp_r64_copy_and_move),
        cmocka_unit_test(test_cpp_r64_add_remove_contains_clear),
        cmocka_unit_test(test_cpp_r64_add_remove_checked),
        cmocka_unit_test(test_cpp_r64_add_remove_range),
        cmocka_unit_test(test_cpp_r64_contains_range),
        cmocka_unit_test(test_cpp_r64_construction_helpers),
        cmocka_unit_test(test_cpp_r64_minmax_equals),
        cmocka_unit_test(test_cpp_r64_set_ops),
        cmocka_unit_test(test_cpp_r64_is_subset),
        cmocka_unit_test(test_cpp_r64_flip),
        cmocka_unit_test(test_cpp_r64_iteration),
        cmocka_unit_test(test_cpp_r64_bidirectional),
        cmocka_unit_test(test_cpp_r64_run_optimize),
        cmocka_unit_test(test_cpp_r64_remove_run_compression),
        cmocka_unit_test(test_cpp_r64_shrink_to_fit),
        cmocka_unit_test(test_cpp_r64_run_compression),
        cmocka_unit_test(test_cpp_r64_serialization),
        cmocka_unit_test(test_cpp_r64_serialization_empty),
        cmocka_unit_test(test_cpp_r64_serialization_containers),
        cmocka_unit_test(test_cpp_r64_serialization_matches_roaring64map),
        cmocka_unit_test(test_cpp_r64_deserialize_empty),
        cmocka_unit_test(test_cpp_r64_deserialize_32bit_vals),
        cmocka_unit_test(test_cpp_r64_deserialize_spread_vals),
        cmocka_unit_test(test_cpp_r64_deserialize_high_vals),
        cmocka_unit_test(test_cpp_r64_deserialize_run_bitset_vals),
#if ROARING_EXCEPTIONS
        cmocka_unit_test(test_cpp_r64_deserialize_empty_input),
        cmocka_unit_test(test_cpp_r64_deserialize_size_too_small),
        cmocka_unit_test(test_cpp_r64_deserialize_invalid_size),
        cmocka_unit_test(test_cpp_r64_deserialize_key_too_small),
#endif
        cmocka_unit_test(test_cpp_r64_random_vs_set),
        cmocka_unit_test(test_cpp_r64_random_range_ops_vs_set),
        cmocka_unit_test(test_cpp_r64_self_aliasing),
        cmocka_unit_test(test_cpp_r64_run_optimize_recovers_space),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
