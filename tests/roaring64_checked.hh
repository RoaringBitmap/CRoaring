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

    void remove(uint64_t x) {
        plain.remove(x);
        check.erase(x);
    }

    bool contains(uint64_t x) const {
        bool ans = plain.contains(x);
        assert_true(ans == (check.count(x) == 1));
        return ans;
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

    // Manually validates the full contents against the reference set.
    void validate() const {
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
};

}  // namespace doublechecked

#endif  // INCLUDE_ROARING_64_CHECKED_HH_
