//
// roaring64_checked.hh
//
// PURPOSE:
//
// This file implements a class which maintains a `class Roaring64` bitset in
// sync with a C++ `std::set` of 64-bit integers.  It asserts if it ever
// notices a difference between the result the roaring bitset gives and the
// result that the set would give.
//
// The doublechecked class is a drop-in replacement for the plain C++ class.
// Hence any codebase that uses that class could act as a test...if it wished.
//
// USAGE:
//
// The checked class has the same name (Roaring64) in `namespace
// doublechecked`. So switching between versions could be done easily with a
// command-line
// `-D` setting for a #define, e.g.:
//
//     #ifdef ROARING_DOUBLECHECK_CPP
//         #include "roaring64_checked.hh"
//         using doublechecked::Roaring64;
//     #else
//         #include "roaring/roaring64.hh"
//     #endif

#ifndef INCLUDE_ROARING_64_CHECKED_HH_
#define INCLUDE_ROARING_64_CHECKED_HH_

#include <algorithm>
#include <cmath>
#include <iterator>
#include <set>
#include <vector>

#include <roaring/roaring64.hh>

#include "test.h"

namespace doublechecked {

class Roaring64 {
   public:
    roaring::Roaring64 plain;
    std::set<uint64_t> check;

    Roaring64() : plain() {}

    void add(uint64_t x) {
        plain.add(x);
        check.insert(x);
    }

    void addMany(size_t n_args, const uint64_t* vals) {
        plain.addMany(n_args, vals);
        for (size_t i = 0; i < n_args; ++i) check.insert(vals[i]);
    }

    bool addChecked(uint64_t x) {
        bool ans = plain.addChecked(x);
        assert_true(ans == check.insert(x).second);
        return ans;
    }

    void addRange(uint64_t min, uint64_t max) {
        plain.addRange(min, max);
        if (min < max) {
            forEachInRangeClosed(min, max - 1,
                                 [this](uint64_t v) { check.insert(v); });
        }
    }

    void addRangeClosed(uint64_t min, uint64_t max) {
        plain.addRangeClosed(min, max);
        forEachInRangeClosed(min, max, [this](uint64_t v) { check.insert(v); });
    }

    void remove(uint64_t x) {
        plain.remove(x);
        check.erase(x);
    }

    bool removeChecked(uint64_t x) {
        bool ans = plain.removeChecked(x);
        assert_true(ans == (check.erase(x) == 1));
        return ans;
    }

    void removeRange(uint64_t min, uint64_t max) {
        plain.removeRange(min, max);
        if (min < max) {
            check.erase(check.lower_bound(min), check.lower_bound(max));
        }
    }

    void removeRangeClosed(uint64_t min, uint64_t max) {
        plain.removeRangeClosed(min, max);
        if (min <= max) {
            check.erase(check.lower_bound(min), check.upper_bound(max));
        }
    }

    void flip(uint64_t min, uint64_t max) {
        plain.flip(min, max);
        if (min < max) {
            forEachInRangeClosed(min, max - 1,
                                 [this](uint64_t v) { flipInCheck(v); });
        }
    }

    void flipClosed(uint64_t min, uint64_t max) {
        plain.flipClosed(min, max);
        forEachInRangeClosed(min, max, [this](uint64_t v) { flipInCheck(v); });
    }

    bool contains(uint64_t x) const {
        bool ans = plain.contains(x);
        assert_true(ans == (check.count(x) == 1));
        return ans;
    }

    bool containsRange(uint64_t min, uint64_t max) const {
        bool ans = plain.containsRange(min, max);
        assert_true(ans == (min >= max || checkContainsClosed(min, max - 1)));
        return ans;
    }

    bool containsRangeClosed(uint64_t min, uint64_t max) const {
        bool ans = plain.containsRangeClosed(min, max);
        assert_true(ans == (min > max || checkContainsClosed(min, max)));
        return ans;
    }

    bool isSubset(const Roaring64& r) const {
        bool ans = plain.isSubset(r.plain);
        assert_true(ans == std::includes(r.check.begin(), r.check.end(),
                                         check.begin(), check.end()));
        return ans;
    }

    bool isStrictSubset(const Roaring64& r) const {
        bool ans = plain.isStrictSubset(r.plain);
        assert_true(ans == (check.size() < r.check.size() &&
                            std::includes(r.check.begin(), r.check.end(),
                                          check.begin(), check.end())));
        return ans;
    }

    uint64_t and_cardinality(const Roaring64& r) const {
        uint64_t ans = plain.and_cardinality(r.plain);
        assert_int_equal(ans, intersectionSize(r));
        return ans;
    }

    uint64_t or_cardinality(const Roaring64& r) const {
        uint64_t ans = plain.or_cardinality(r.plain);
        assert_int_equal(ans,
                         check.size() + r.check.size() - intersectionSize(r));
        return ans;
    }

    uint64_t andnot_cardinality(const Roaring64& r) const {
        uint64_t ans = plain.andnot_cardinality(r.plain);
        assert_int_equal(ans, check.size() - intersectionSize(r));
        return ans;
    }

    uint64_t xor_cardinality(const Roaring64& r) const {
        uint64_t ans = plain.xor_cardinality(r.plain);
        assert_int_equal(
            ans, check.size() + r.check.size() - 2 * intersectionSize(r));
        return ans;
    }

    bool intersect(const Roaring64& r) const {
        bool ans = plain.intersect(r.plain);
        assert_true(ans == (intersectionSize(r) > 0));
        return ans;
    }

    double jaccard_index(const Roaring64& r) const {
        double ans = plain.jaccard_index(r.plain);
        uint64_t inter = intersectionSize(r);
        uint64_t uni = check.size() + r.check.size() - inter;
        if (uni > 0) {  // undefined when both are empty
            double expected =
                static_cast<double>(inter) / static_cast<double>(uni);
            assert_true(std::fabs(ans - expected) < 1e-12);
        }
        return ans;
    }

    uint64_t rank(uint64_t x) const {
        uint64_t ans = plain.rank(x);
        assert_int_equal(ans,
                         std::distance(check.begin(), check.upper_bound(x)));
        return ans;
    }

    bool select(uint64_t rnk, uint64_t* element) const {
        bool ans = plain.select(rnk, element);
        assert_true(ans == (rnk < check.size()));
        if (ans) {
            auto it =
                std::next(check.begin(), static_cast<std::ptrdiff_t>(rnk));
            assert_int_equal(*element, *it);
        }
        return ans;
    }

    int64_t getIndex(uint64_t x) const {
        int64_t ans = plain.getIndex(x);
        auto it = check.find(x);
        assert_true(
            ans == (it == check.end() ? -1 : std::distance(check.begin(), it)));
        return ans;
    }

    void iterate(roaring::api::roaring_iterator64 iterator, void* ptr) const {
        std::vector<uint64_t> seen;
        plain.iterate(
            [](uint64_t value, void* p) {
                static_cast<std::vector<uint64_t>*>(p)->push_back(value);
                return true;
            },
            &seen);
        assert_int_equal(seen.size(), check.size());
        assert_true(std::equal(seen.begin(), seen.end(), check.begin()));
        plain.iterate(iterator, ptr);
    }

    bool internal_validate(const char** reason = nullptr) const {
        return plain.internal_validate(reason);
    }

    void clear() {
        plain.clear();
        check.clear();
    }

    uint64_t cardinality() const {
        uint64_t ans = plain.cardinality();
        assert_int_equal(ans, check.size());
        return ans;
    }

    bool isEmpty() const {
        bool ans = plain.isEmpty();
        assert_true(ans == check.empty());
        return ans;
    }

    bool removeRunCompression() { return plain.removeRunCompression(); }

    bool runOptimize() { return plain.runOptimize(); }

    size_t shrinkToFit() { return plain.shrinkToFit(); }

    size_t write(char* buf) const { return plain.write(buf); }

    size_t getSizeInBytes() const { return plain.getSizeInBytes(); }

    // Validates the internal structure, then manually validates the full
    // contents against the reference set.
    void validate() const {
        assert_true(plain.internal_validate());
        assert_int_equal(plain.cardinality(), check.size());
        std::vector<uint64_t> from_plain;
        for (auto it = plain.begin(); it != plain.end(); ++it) {
            from_plain.push_back(*it);
        }
        assert_int_equal(from_plain.size(), check.size());
        size_t i = 0;
        for (uint64_t v : check) {
            assert_int_equal(from_plain[i], v);
            ++i;
        }
    }

    // TODO: Add other Roaring64 methods to make this a complete drop-in

   private:
    // Calls f(v) for each v in [min, max], doing nothing if min > max.
    template <typename F>
    static void forEachInRangeClosed(uint64_t min, uint64_t max, F f) {
        if (min > max) {
            return;
        }
        for (uint64_t v = min;; ++v) {
            f(v);
            if (v == max) {
                break;
            }
        }
    }

    void flipInCheck(uint64_t v) {
        if (check.erase(v) == 0) {
            check.insert(v);
        }
    }

    uint64_t intersectionSize(const Roaring64& r) const {
        uint64_t count = 0;
        auto a = check.begin();
        auto b = r.check.begin();
        while (a != check.end() && b != r.check.end()) {
            if (*a < *b) {
                ++a;
            } else if (*b < *a) {
                ++b;
            } else {
                ++count;
                ++a;
                ++b;
            }
        }
        return count;
    }

    // Requires min <= max.
    bool checkContainsClosed(uint64_t min, uint64_t max) const {
        auto first = check.lower_bound(min);
        auto last = check.upper_bound(max);
        uint64_t count = static_cast<uint64_t>(std::distance(first, last));
        return count > 0 && count - 1 == max - min;
    }
};

}  // namespace doublechecked

#endif  // INCLUDE_ROARING_64_CHECKED_HH_
