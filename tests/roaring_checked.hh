//
// roaring_checked.hh
//
// PURPOSE:
//
// This file implements a class which maintains a `class Roaring` bitset in
// sync with a C++ `std::set` of 32-bit integers.  It asserts if it ever
// notices a difference between the result the roaring bitset gives and the
// result that the set would give.
//
// The doublechecked class is a drop-in replacement for the plain C++ class.
// Hence any codebase that uses that class could act as a test...if it wished.
//
// USAGE:
//
// The checked class has the same name (Roaring) in `namespace doublechecked`.
// So switching between versions could be done easily with a command-line
// `-D` setting for a #define, e.g.:
//
//     #ifdef ROARING_DOUBLECHECK_CPP
//         #include "roaring_checked.hh"
//         using doublechecked::Roaring;
//     #else
//         #include "roaring.hh"
//     #endif
//
// NOTES:
//
// * The algorithms available in `<algorithm>` don't generally support in-place
//   operations, and many (like `std::set_intersection`) weren't added until
//   C++17.  For these two reasons, many are re-implemented here.
//

#ifndef INCLUDE_ROARING_CHECKED_HH_
#define INCLUDE_ROARING_CHECKED_HH_

#include <stdarg.h>

#include <algorithm>
#include <new>
#include <stdexcept>
#include <string>

#include <set>  // sorted set, typically a red-black tree implementation
#include <assert.h>

#define ROARING_CPP_NAMESPACE unchecked  // can't be overridden if global
#include "roaring.hh"  // contains Roaring unchecked class

namespace doublechecked {  // put the checked class in its own namespace

class Roaring {
  public:  // members public to allow tests access to them
    roaring::Roaring plain;  // ordinary Roaring bitset wrapper class
    std::set<uint32_t> check;  // contents kept in sync with `plain`

  public:
    Roaring() : plain() {
    }

    Roaring(size_t n, const uint32_t *data) : plain (n, data) {
        for (size_t i = 0; i < n; ++i)
            check.insert(data[i]);
    }

    Roaring(const Roaring &r) {
        plain = r.plain;
        check = r.check;
    }

    Roaring(Roaring &&r) noexcept {
        plain = std::move(r.plain);
        check = std::move(r.check);
    }

    Roaring(roaring::api::roaring_bitmap_t *s) noexcept : plain (s) {
        for (auto value : plain)
            check.insert(value);
    }

    // This constructor is unique to doublecheck::Roaring(), for making a
    // doublechecked version from an unchecked version.  Note that this alone
    // is somewhat toothless for checking...e.g. running an operation and then
    // accepting that all the values in it were correct doesn't do much.  So
    // the results of such constructions should be validated another way.
    //
    Roaring(roaring::Roaring &&other_plain) {
        plain = std::move(other_plain);
        for (auto value : plain)
            check.insert(value);
    }

    // Note: This does not call `::Roaring::bitmapOf()` because variadics can't
    // forward their parameters.  But this is all the code does, so it's fine.
    //
    static Roaring bitmapOf(size_t n, ...) {
        doublechecked::Roaring ans;
        va_list vl;
        va_start(vl, n);
        for (size_t i = 0; i < n; i++) {
            ans.add(va_arg(vl, uint32_t));
        }
        va_end(vl);
        return ans;
    }

    void add(uint32_t x) {
        plain.add(x);
        check.insert(x);
    }

    bool addChecked(uint32_t x) {
        bool ans = plain.addChecked(x);
        bool was_in_set = check.insert(x).second;  // insert -> pair<iter,bool>
        assert(ans == was_in_set);
        (void)was_in_set;  // unused besides assert
        return ans;
    }

    void addRange(const uint64_t x, const uint64_t y) {
        if (x != y) {  // repeat add_range_closed() cast and bounding logic
            addRangeClosed(x, y - 1);
        }
    }

    void addRangeClosed(uint32_t min, uint32_t max) {
        plain.addRangeClosed(min, max);
        if (min <= max) {
            for (uint32_t val = max; val != min - 1; --val)
                check.insert(val);
        }
    }

    void addMany(size_t n_args, const uint32_t *vals) {
        plain.addMany(n_args, vals);
        for (size_t i = 0; i < n_args; ++i)
            check.insert(vals[i]);
    }

    void remove(uint32_t x) {
        plain.remove(x);
        check.erase(x);
    }

    bool removeChecked(uint32_t x) {
        bool ans = plain.removeChecked(x);
        size_t num_removed = check.erase(x);
        assert(ans == (num_removed == 1));
        (void)num_removed;  // unused besides assert
        return ans;
    }

    void removeRange(const uint64_t x, const uint64_t y) {
        if (x != y) {  // repeat remove_range_closed() cast and bounding logic
            removeRangeClosed(x, y - 1);
        }
    }

    void removeRangeClosed(uint32_t min, uint32_t max) {
        plain.removeRangeClosed(min, max);
        if (min <= max) {
            check.erase(check.lower_bound(min), check.upper_bound(max));
        }
    }

    uint32_t maximum() const {
        uint32_t ans = plain.maximum();
        assert(check.empty() ? ans == 0 : ans == *check.rbegin());
        return ans;
    }

    uint32_t minimum() const {
        uint32_t ans = plain.minimum();
        assert(check.empty() ? ans == UINT32_MAX : ans == *check.begin());
        return ans;
    }

    bool contains(uint32_t x) const {
        bool ans = plain.contains(x);
        assert(ans == (check.find(x) != check.end()));
        return ans;
    }

    bool containsRange(const uint64_t x, const uint64_t y) const {
        bool ans = plain.containsRange(x, y);

        auto it = check.find(x);
        if (x >= y)
            assert(ans == true);  // roaring says true for this
        else if (it == check.end())
            assert(ans == false);  // start of range not in set
        else {
            uint64_t last = x;  // iterate up to y so long as values sequential
            while (++it != check.end() && last + 1 == *it && *it < y)
                last = *it;
            assert(ans == (last == y - 1));
        }

        return ans;
    }

    // This method is exclusive to `doublechecked::Roaring`
    //
    bool does_std_set_match_roaring() const {
        auto it_check = check.begin();
        auto it_check_end = check.end();
        auto it_plain = plain.begin();
        auto it_plain_end = plain.end();

        for (; it_check != it_check_end; ++it_check, ++it_plain) {
            if (it_plain == it_plain_end)
                return false;
            if (*it_check != *it_plain)
                return false;
        }
        return it_plain == plain.end();  // should have visited all values
    }

    ~Roaring() {
        assert(does_std_set_match_roaring());  // always check on destructor
    }

    Roaring &operator=(const Roaring &r) {
        plain = r.plain;
        check = r.check;
        return *this;
    }

    Roaring &operator=(Roaring &&r) noexcept {
        plain = std::move(r.plain);
        check = std::move(r.check);
        return *this;
    }

    Roaring &operator&=(const Roaring &r) {
        plain &= r.plain;

        auto it = check.begin();
        auto r_it = r.check.begin();
        while (it != check.end() && r_it != r.check.end()) {
            if (*it < *r_it) { it = check.erase(it); }
            else if (*r_it < *it) { ++r_it; }
            else { ++it; ++r_it; }  // overlapped
        }
        check.erase(it, check.end());  // erase rest of check not in r.check

        return *this;
    }

    Roaring &operator-=(const Roaring &r) {
        plain -= r.plain;

        for (auto value : r.check)
            check.erase(value);  // Note std::remove() is not for ordered sets

        return *this;
    }

    Roaring &operator|=(const Roaring &r) {
        plain |= r.plain;

        check.insert(r.check.begin(), r.check.end());  // won't add duplicates

        return *this;
    }

    Roaring &operator^=(const Roaring &r) {
        plain ^= r.plain;

        auto it = check.begin();
        auto it_end = check.end();
        auto r_it = r.check.begin();
        auto r_it_end = r.check.end();
        if (it == it_end) { check = r.check; }  // this empty
        else if (r_it == r_it_end) { }  // r empty
        else if (*it > *r.check.rbegin() || *r_it > *check.rbegin()) {
            check.insert(r.check.begin(), r.check.end());  // obvious disjoint
        } else while (r_it != r_it_end) {  // may overlap
            if (it == it_end) { check.insert(*r_it); ++r_it; }
            else if (*it == *r_it) {  // remove overlapping value
                it = check.erase(it);  // returns *following* iterator
                ++r_it;
            }
            else if (*it < *r_it) { ++it; }  // keep value from this
            else { check.insert(*r_it); ++r_it; }  // add value from r
        }

        return *this;
    }

    void swap(Roaring &r) {
        std::swap(r.plain, plain);
        std::swap(r.check, check);
    }

    uint64_t cardinality() const {
        uint64_t ans = plain.cardinality();
        assert(ans == check.size());
        return ans;
    }

    bool isEmpty() const {
        bool ans = plain.isEmpty();
        assert(ans == check.empty());
        return ans;
    }

    bool isSubset(const Roaring &r) const {  // is `this` subset of `r`?
        bool ans = plain.isSubset(r.plain);
        assert(ans == std::includes(
            r.check.begin(), r.check.end(),  // containing range
            check.begin(), check.end()  // range to test for containment
        ));
        return ans;
    }

    bool isStrictSubset(const Roaring &r) const {  // is `this` subset of `r`?
        bool ans = plain.isStrictSubset(r.plain);
        assert(ans == (std::includes(
            r.check.begin(), r.check.end(),  // containing range
            check.begin(), check.end()  // range to test for containment
        ) && r.check.size() > check.size()));
        return ans;
    }

    void toUint32Array(uint32_t *ans) const {
        plain.toUint32Array(ans);
        // TBD: doublecheck
    }

    void rangeUint32Array(uint32_t *ans, size_t offset, size_t limit) const {
        plain.rangeUint32Array(ans, offset, limit);
        // TBD: doublecheck
    }

    bool operator==(const Roaring &r) const {
        bool ans = (plain == r.plain);
        assert(ans == (check == r.check));
        return ans;
    }

    void flip(uint64_t range_start, uint64_t range_end) {
        plain.flip(range_start, range_end);

        if (range_start < range_end) {
            if (range_end >= UINT64_C(0x100000000))
                range_end = UINT64_C(0x100000000);
            auto hint = check.lower_bound(range_start);  // *hint stays as >= i
            auto it_end = check.end();
            for (uint64_t i = range_start; i < range_end; ++i) {
                if (hint == it_end || *hint > i)  // i not present, so add
                    check.insert(hint, i);  // leave hint past i
                else  // *hint == i, must adjust hint and erase
                    hint = check.erase(hint);  // returns *following* iterator
            }
        }
    }

    bool removeRunCompression() {
        return plain.removeRunCompression();
    }

    bool runOptimize() {
        return plain.runOptimize();
    }

    size_t shrinkToFit() {
        return plain.shrinkToFit();
    }

    void iterate(roaring::api::roaring_iterator iterator, void *ptr) const {
        plain.iterate(iterator, ptr);
        assert(does_std_set_match_roaring());  // checks equivalent iteration
    }

    bool select(uint32_t rnk, uint32_t *element) const {
        bool ans = plain.select(rnk, element);

        auto it = check.begin();
        auto it_end = check.end();
        for (uint32_t i = 0; it != it_end && i < rnk; ++i)
            ++it;
        assert(ans == (it != it_end) && (ans ? *it == *element : true));

        return ans;
    }

    uint64_t and_cardinality(const Roaring &r) const {
        uint64_t ans = plain.and_cardinality(r.plain);

        auto it = check.begin();
        auto it_end = check.end();
        auto r_it = r.check.begin();
        auto r_it_end = r.check.end();
        if (it == it_end || r_it == r_it_end) {
            assert(ans == 0);  // if either is empty then no intersection
        } else if (*it > *r.check.rbegin() || *r_it > *check.rbegin()) {
            assert(ans == 0);  // obvious disjoint
        } else {  // may overlap
            uint64_t count = 0;
            while (it != it_end && r_it != r_it_end) {
                if (*it == *r_it) { ++count; ++it; ++r_it; }  // count overlap
                else if (*it < *r_it) { ++it; }
                else { ++r_it; }
            }
            assert(ans == count);
        }

        return ans;
    }

    bool intersect(const Roaring &r) const {
    	  bool ans = plain.intersect(r.plain);

        auto it = check.begin();
        auto it_end = check.end();
        auto r_it = r.check.begin();
        auto r_it_end = r.check.end();
        if (it == it_end || r_it == r_it_end) {
            assert(ans == false);  // if either are empty, no intersection
        } else if (*it > *r.check.rbegin() || *r_it > *check.rbegin()) {
            assert(ans == false);  // obvious disjoint
        } else while (it != it_end && r_it != r_it_end) {  // may overlap
            if (*it == *r_it) { assert(ans == true); goto done; }  // overlap
            else if (*it < *r_it) { ++it; }
            else { ++r_it; }
        }
        assert(ans == false);

      done:  // (could use lambda vs goto, but debug step in lambdas is poor)
         return ans;
    }

    double jaccard_index(const Roaring &r) const {
        double ans = plain.jaccard_index(r.plain);
        // TBD: doublecheck
        return ans;
    }

    uint64_t or_cardinality(const Roaring &r) const {
        uint64_t ans = plain.or_cardinality(r.plain);

        auto it = check.begin();
        auto it_end = check.end();
        auto r_it = r.check.begin();
        auto r_it_end = r.check.end();
        if (it == it_end) { assert(ans == r.check.size()); }  // this empty
        else if (r_it == r_it_end) { assert(ans == check.size()); }  // r empty
        else if (*it > *r.check.rbegin() || *r_it > *check.rbegin()) {
            assert(ans == check.size() + r.check.size());  // obvious disjoint
        } else {
            uint64_t count = 0;
            while (it != it_end || r_it != r_it_end) {
                ++count;  // note matching case counts once but bumps both
                if (it == it_end) { ++r_it; }
                else if (r_it == r_it_end) { ++it; }
                else if (*it == *r_it) { ++it; ++r_it; }  // matching case
                else if (*it < *r_it) { ++it; }
                else { ++r_it; }
            }
            assert(ans == count);
        }

        return ans;
    }

    uint64_t andnot_cardinality(const Roaring &r) const {
        uint64_t ans = plain.andnot_cardinality(r.plain);

        auto it = check.begin();
        auto it_end = check.end();
        auto r_it = r.check.begin();
        auto r_it_end = r.check.end();
        if (it == it_end) { assert(ans == 0); }  // this empty
        else if (r_it == r_it_end) { assert(ans == check.size()); }  // r empty
        else if (*it > *r.check.rbegin() || *r_it > *check.rbegin()) {
            assert(ans == check.size());  // disjoint so nothing removed
        } else {  // may overlap
            uint64_t count = check.size();  // start with cardinality of this
            while (it != it_end && r_it != r_it_end) {
                if (*it == *r_it) { --count; ++it; ++r_it; }  // remove overlap
                else if (*it < *r_it) { ++it; }
                else { ++r_it; }
            }
            assert(ans == count);
        }

        return ans;
    }

    uint64_t xor_cardinality(const Roaring &r) const {
        uint64_t ans = plain.xor_cardinality(r.plain);

        auto it = check.begin();
        auto it_end = check.end();
        auto r_it = r.check.begin();
        auto r_it_end = r.check.end();
        if (it == it_end) { assert(ans == r.check.size()); }  // this empty
        else if (r_it == r_it_end) { assert(ans == check.size()); }  // r empty
        else if (*it > *r.check.rbegin() || *r_it > *check.rbegin()) {
            assert(ans == check.size() + r.check.size());  // obvious disjoint
        } else {  // may overlap
            uint64_t count = 0;
            while (it != it_end || r_it != r_it_end) {
                if (it == it_end) { ++count; ++r_it; }
                else if (r_it == r_it_end) { ++count; ++it; }
                else if (*it == *r_it) { ++it; ++r_it; }  // overlap uncounted
                else if (*it < *r_it) { ++count; ++it; }
                else { ++count; ++r_it; }
            }
            assert(ans == count);
        }

        return ans;
    }

    uint64_t rank(uint32_t x) const {
        uint64_t ans = plain.rank(x);

        uint64_t count = 0;
        auto it = check.begin();
        auto it_end = check.end();
        for (; it != it_end && *it <= x; ++it)
            ++count;
        assert(ans == count);

        return ans;
    }

    size_t write(char *buf, bool portable = true) const {
        return plain.write(buf, portable);
    }

    static Roaring read(const char *buf, bool portable = true) {
        auto plain = roaring::Roaring::read(buf, portable);
        return Roaring(std::move(plain));
    }

    static Roaring readSafe(const char *buf, size_t maxbytes) {
        auto plain = roaring::Roaring::readSafe(buf, maxbytes);
        return Roaring(std::move(plain));
    }

    size_t getSizeInBytes(bool portable = true) const {
        return plain.getSizeInBytes(portable);
    }

    Roaring operator&(const Roaring &o) const {
        Roaring ans(plain & o.plain);

        Roaring inplace(*this);
        assert(ans == (inplace &= o));  // validate against in-place version

        return ans;
    }

    Roaring operator-(const Roaring &o) const {
        Roaring ans(plain - o.plain);

        Roaring inplace(*this);
        assert(ans == (inplace -= o));  // validate against in-place version

        return ans;
    }

    Roaring operator|(const Roaring &o) const {
        Roaring ans(plain | o.plain);

        Roaring inplace(*this);
        assert(ans == (inplace |= o));  // validate against in-place version

        return ans;
    }

    Roaring operator^(const Roaring &o) const {
        Roaring ans(plain ^ o.plain);

        Roaring inplace(*this);
        assert(ans == (inplace ^= o));  // validate against in-place version

        return ans;
    }

    void setCopyOnWrite(bool val) {
        plain.setCopyOnWrite(val);
    }

    void printf() const {
        plain.printf();
    }

    std::string toString() const {
        return plain.toString();
    }

    bool getCopyOnWrite() const {
        return plain.getCopyOnWrite();
    }

    static Roaring fastunion(size_t n, const Roaring **inputs) {
        auto plain_inputs = new const roaring::Roaring*[n];
        for (size_t i = 0; i < n; ++i)
            plain_inputs[i] = &inputs[i]->plain;
        Roaring ans(roaring::Roaring::fastunion(n, plain_inputs));
        delete[] plain_inputs;

        if (n == 0)
            assert(ans.cardinality() == 0);
        else {
            Roaring temp = *inputs[0];
            for (size_t i = 1; i < n; ++i)
                temp |= *inputs[i];
            assert(temp == ans);
        }

        return ans;
    }

    typedef roaring::RoaringSetBitForwardIterator const_iterator;

    const_iterator begin() const {
        return roaring::RoaringSetBitForwardIterator(plain);
    }

    const_iterator &end() const {
        static roaring::RoaringSetBitForwardIterator e(plain, true);
        return e;
    }
};

}  // end `namespace doublechecked`

#endif  // INCLUDE_ROARING_CHECKED_HH_
