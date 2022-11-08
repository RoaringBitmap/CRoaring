/*
A C++ header for 64-bit Roaring Bitmaps, implemented by way of a map of many
32-bit Roaring Bitmaps.
*/
#ifndef INCLUDE_ROARING_64_MAP_HH_
#define INCLUDE_ROARING_64_MAP_HH_

#include <algorithm>
#include <cstdarg>  // for va_list handling in bitmapOf()
#include <cstdio>  // for std::printf() in the printf() method
#include <cstring>  // for std::memcpy()
#include <limits>
#include <map>
#include <new>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>

#include "roaring.hh"

namespace roaring {

using roaring::Roaring;

class Roaring64MapSetBitForwardIterator;
class Roaring64MapSetBitBiDirectionalIterator;

class Roaring64Map {
    typedef api::roaring_bitmap_t roaring_bitmap_t;

public:
    /**
     * Create an empty bitmap
     */
    Roaring64Map() = default;

    /**
     * Construct a bitmap from a list of 32-bit integer values.
     */
    Roaring64Map(size_t n, const uint32_t *data) { addMany(n, data); }

    /**
     * Construct a bitmap from a list of 64-bit integer values.
     */
    Roaring64Map(size_t n, const uint64_t *data) { addMany(n, data); }

    /**
     * Construct a 64-bit map from a 32-bit one
     */
    explicit Roaring64Map(const Roaring &r) { emplaceOrInsert(0, r); }

    /**
     * Construct a 64-bit map from a 32-bit rvalue
     */
    explicit Roaring64Map(Roaring &&r) { emplaceOrInsert(0, std::move(r)); }

    /**
     * Construct a roaring object from the C struct.
     *
     * Passing a NULL point is unsafe.
     */
    explicit Roaring64Map(roaring_bitmap_t *s) {
        emplaceOrInsert(0, Roaring(s));
    }

    Roaring64Map(const Roaring64Map& r) = default;

    Roaring64Map(Roaring64Map&& r) noexcept = default;

    /**
     * Copy assignment operator.
     */
    Roaring64Map &operator=(const Roaring64Map &r) = default;

    /**
     * Move assignment operator.
     */
     Roaring64Map &operator=(Roaring64Map &&r) noexcept = default;

    /**
     * Construct a bitmap from a list of integer values.
     */
    static Roaring64Map bitmapOf(size_t n...) {
        Roaring64Map ans;
        va_list vl;
        va_start(vl, n);
        for (size_t i = 0; i < n; i++) {
            ans.add(va_arg(vl, uint64_t));
        }
        va_end(vl);
        return ans;
    }

    /**
     * Add value x
     */
    void add(uint32_t x) {
        roarings[0].add(x);
        roarings[0].setCopyOnWrite(copyOnWrite);
    }
    void add(uint64_t x) {
        roarings[highBytes(x)].add(lowBytes(x));
        roarings[highBytes(x)].setCopyOnWrite(copyOnWrite);
    }

    /**
     * Add value x
     * Returns true if a new value was added, false if the value was already existing.
     */
    bool addChecked(uint32_t x) {
        bool result = roarings[0].addChecked(x);
        roarings[0].setCopyOnWrite(copyOnWrite);
        return result;
    }
    bool addChecked(uint64_t x) {
        bool result = roarings[highBytes(x)].addChecked(lowBytes(x));
        roarings[highBytes(x)].setCopyOnWrite(copyOnWrite);
        return result;
    }

    /**
     * Add all values in range [min, max)
     */
    void addRange(uint64_t min, uint64_t max) {
        if (min >= max) {
            return;
        }
        addRangeClosed(min, max - 1);
    }

    /**
     * Add all values in range [min, max]
     */
    void addRangeClosed(uint32_t min, uint32_t max) {
        roarings[0].addRangeClosed(min, max);
    }
    void addRangeClosed(uint64_t min, uint64_t max) {
        if (min > max) {
            return;
        }
        uint32_t start_high = highBytes(min);
        uint32_t start_low = lowBytes(min);
        uint32_t end_high = highBytes(max);
        uint32_t end_low = lowBytes(max);
        if (start_high == end_high) {
            roarings[start_high].addRangeClosed(start_low, end_low);
            roarings[start_high].setCopyOnWrite(copyOnWrite);
            return;
        }
        // we put std::numeric_limits<>::max/min in parenthesis to avoid a clash
        // with the Windows.h header under Windows
        roarings[start_high].addRangeClosed(
            start_low, (std::numeric_limits<uint32_t>::max)());
        roarings[start_high].setCopyOnWrite(copyOnWrite);
        start_high++;
        for (; start_high < end_high; ++start_high) {
            roarings[start_high].addRangeClosed(
                (std::numeric_limits<uint32_t>::min)(),
                (std::numeric_limits<uint32_t>::max)());
            roarings[start_high].setCopyOnWrite(copyOnWrite);
        }
        roarings[end_high].addRangeClosed(
            (std::numeric_limits<uint32_t>::min)(), end_low);
        roarings[end_high].setCopyOnWrite(copyOnWrite);
    }

    /**
     * Add value n_args from pointer vals
     */
    void addMany(size_t n_args, const uint32_t *vals) {
        Roaring &roaring = roarings[0];
        roaring.addMany(n_args, vals);
        roaring.setCopyOnWrite(copyOnWrite);
    }

    void addMany(size_t n_args, const uint64_t *vals) {
        for (size_t lcv = 0; lcv < n_args; lcv++) {
            roarings[highBytes(vals[lcv])].add(lowBytes(vals[lcv]));
            roarings[highBytes(vals[lcv])].setCopyOnWrite(copyOnWrite);
        }
    }

    /**
     * Removes value x.
     */
    void remove(uint32_t x) {
        auto iter = roarings.begin();
        // Since x is a uint32_t, highbytes(x) == 0. The inner bitmap we are
        // looking for, if it exists, will be at the first slot of 'roarings'.
        if (iter == roarings.end() || iter->first != 0) {
            return;
        }
        auto &bitmap = iter->second;
        bitmap.remove(x);
        eraseIfEmpty(iter);
    }

    /**
     * Removes value x.
     */
    void remove(uint64_t x) {
        auto iter = roarings.find(highBytes(x));
        if (iter == roarings.end()) {
            return;
        }
        auto &bitmap = iter->second;
        bitmap.remove(lowBytes(x));
        eraseIfEmpty(iter);
    }

    /**
     * Removes value x
     * Returns true if a new value was removed, false if the value was not
     * present.
     */
    bool removeChecked(uint32_t x) {
        auto iter = roarings.begin();
        // Since x is a uint32_t, highbytes(x) == 0. The inner bitmap we are
        // looking for, if it exists, will be at the first slot of 'roarings'.
        if (iter == roarings.end() || iter->first != 0) {
            return false;
        }
        auto &bitmap = iter->second;
        if (!bitmap.removeChecked(x)) {
            return false;
        }
        eraseIfEmpty(iter);
        return true;
    }

    /**
     * Remove value x
     * Returns true if a new value was removed, false if the value was not
     * present.
     */
    bool removeChecked(uint64_t x) {
        auto iter = roarings.find(highBytes(x));
        if (iter == roarings.end()) {
            return false;
        }
        auto &bitmap = iter->second;
        if (!bitmap.removeChecked(lowBytes(x))) {
            return false;
        }
        eraseIfEmpty(iter);
        return true;
    }

    /**
     * Removes all values in the half-open interval [min, max).
     */
    void removeRange(uint64_t min, uint64_t max) {
        if (min >= max) {
            return;
        }
        return removeRangeClosed(min, max - 1);
    }

    /**
     * Removes all values in the closed interval [min, max].
     */
    void removeRangeClosed(uint32_t min, uint32_t max) {
        auto iter = roarings.begin();
        // Since min and max are uint32_t, highbytes(min or max) == 0. The inner
        // bitmap we are looking for, if it exists, will be at the first slot of
        // 'roarings'.
        if (iter == roarings.end() || iter->first != 0) {
            return;
        }
        auto &bitmap = iter->second;
        bitmap.removeRangeClosed(min, max);
        eraseIfEmpty(iter);
    }

    /**
     * Removes all values in the closed interval [min, max].
     */
    void removeRangeClosed(uint64_t min, uint64_t max) {
        if (min > max) {
            return;
        }
        uint32_t start_high = highBytes(min);
        uint32_t start_low = lowBytes(min);
        uint32_t end_high = highBytes(max);
        uint32_t end_low = lowBytes(max);

        // We put std::numeric_limits<>::max in parentheses to avoid a
        // clash with the Windows.h header under Windows.
        const uint32_t uint32_max = (std::numeric_limits<uint32_t>::max)();

        // If the outer map is empty, end_high is less than the first key,
        // or start_high is greater than the last key, then exit now because
        // there is no work to do.
        if (roarings.empty() || end_high < roarings.cbegin()->first ||
            start_high > (roarings.crbegin())->first) {
            return;
        }

        // If we get here, start_iter points to the first entry in the outer map
        // with key >= start_high. Such an entry is known to exist (i.e. the
        // iterator will not be equal to end()) because start_high <= the last
        // key in the map (thanks to the above if statement).
        auto start_iter = roarings.lower_bound(start_high);
        // end_iter points to the first entry in the outer map with
        // key >= end_high, if such a key exists. Otherwise, it equals end().
        auto end_iter = roarings.lower_bound(end_high);

        // Note that the 'lower_bound' method will find the start and end slots,
        // if they exist; otherwise it will find the next-higher slots.
        // In the case where 'start' landed on an existing slot, we need to do a
        // partial erase of that slot, and likewise for 'end'. But all the slots
        // in between can be fully erased. More precisely:
        //
        // 1. If the start point falls on an existing entry, there are two
        //    subcases:
        //    a. if the end point falls on that same entry, remove the closed
        //       interval [start_low, end_low] from that entry and we are done.
        //    b. Otherwise, remove the closed interval [start_low, uint32_max]
        //       from that entry, advance start_iter, and fall through to step 2.
        // 2. Completely erase all slots in the half-open interval
        //    [start_iter, end_iter)
        // 3. If the end point falls on an existing entry, remove the closed
        //    interval [0, end_high] from it.

        // Step 1. If the start point falls on an existing entry...
        if (start_iter->first == start_high) {
            auto &start_inner = start_iter->second;
            // 1a. if the end point falls on that same entry...
            if (start_iter == end_iter) {
                start_inner.removeRangeClosed(start_low, end_low);
                eraseIfEmpty(start_iter);
                return;
            }

            // 1b. Otherwise, remove the closed range [start_low, uint32_max]...
            start_inner.removeRangeClosed(start_low, uint32_max);
            // Advance start_iter, but keep the old value so we can check the
            // bitmap we just modified for emptiness and erase if it necessary.
            auto temp = start_iter++;
            eraseIfEmpty(temp);
        }

        // 2. Completely erase all slots in the half-open interval...
        roarings.erase(start_iter, end_iter);

        // 3. If the end point falls on an existing entry...
        if (end_iter != roarings.end() && end_iter->first == end_high) {
            auto &end_inner = end_iter->second;
            end_inner.removeRangeClosed(0, end_low);
            eraseIfEmpty(end_iter);
        }
    }

    /**
     * Clears the bitmap.
     */
    void clear() {
        roarings.clear();
    }

    /**
     * Return the largest value (if not empty)
     */
    uint64_t maximum() const {
        for (auto roaring_iter = roarings.crbegin();
             roaring_iter != roarings.crend(); ++roaring_iter) {
            if (!roaring_iter->second.isEmpty()) {
                return uniteBytes(roaring_iter->first,
                                  roaring_iter->second.maximum());
            }
        }
        // we put std::numeric_limits<>::max/min in parentheses
        // to avoid a clash with the Windows.h header under Windows
        return (std::numeric_limits<uint64_t>::min)();
    }

    /**
     * Return the smallest value (if not empty)
     */
    uint64_t minimum() const {
        for (auto roaring_iter = roarings.cbegin();
             roaring_iter != roarings.cend(); ++roaring_iter) {
            if (!roaring_iter->second.isEmpty()) {
                return uniteBytes(roaring_iter->first,
                                  roaring_iter->second.minimum());
            }
        }
        // we put std::numeric_limits<>::max/min in parentheses
        // to avoid a clash with the Windows.h header under Windows
        return (std::numeric_limits<uint64_t>::max)();
    }

    /**
     * Check if value x is present
     */
    bool contains(uint32_t x) const {
        return roarings.count(0) == 0 ? false : roarings.at(0).contains(x);
    }
    bool contains(uint64_t x) const {
        return roarings.count(highBytes(x)) == 0
            ? false
            : roarings.at(highBytes(x)).contains(lowBytes(x));
    }

    /**
     * Compute the intersection of the current bitmap and the provided bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     */
    Roaring64Map &operator&=(const Roaring64Map &other) {
        if (this == &other) {
            // ANDing *this with itself is a no-op.
            return *this;
        }

        // Logic table summarizing what to do when a given outer key is
        // present vs. absent from self and other.
        //
        // self     other    (self & other)  work to do
        // --------------------------------------------
        // absent   absent   empty           None
        // absent   present  empty           None
        // present  absent   empty           Erase self
        // present  present  empty or not    Intersect self with other, but
        //                                   erase self if result is empty.
        //
        // Because there is only work to do when a key is present in 'self', the
        // main for loop iterates over entries in 'self'.

        decltype(roarings.begin()) self_next;
        for (auto self_iter = roarings.begin(); self_iter != roarings.end();
             self_iter = self_next) {
            // Do the 'next' operation now, so we don't have to worry about
            // invalidation of self_iter down below with the 'erase' operation.
            self_next = std::next(self_iter);

            auto self_key = self_iter->first;
            auto &self_bitmap = self_iter->second;

            auto other_iter = other.roarings.find(self_key);
            if (other_iter == other.roarings.end()) {
                // 'other' doesn't have self_key. In the logic table above,
                // this reflects the case (self.present & other.absent).
                // So, erase self.
                roarings.erase(self_iter);
                continue;
            }

            // Both sides have self_key. In the logic table above, this reflects
            // the case (self.present & other.present). So, intersect self with
            // other.
            const auto &other_bitmap = other_iter->second;
            self_bitmap &= other_bitmap;
            if (self_bitmap.isEmpty()) {
                // ...but if intersection is empty, remove it altogether.
                roarings.erase(self_iter);
            }
        }
        return *this;
    }

    /**
     * Compute the difference between the current bitmap and the provided
     * bitmap, writing the result in the current bitmap. The provided bitmap
     * is not modified.
     */
    Roaring64Map &operator-=(const Roaring64Map &other) {
        if (this == &other) {
            // Subtracting *this from itself results in the empty map.
            roarings.clear();
            return *this;
        }

        // Logic table summarizing what to do when a given outer key is
        // present vs. absent from self and other.
        //
        // self     other    (self - other)  work to do
        // --------------------------------------------
        // absent   absent   empty           None
        // absent   present  empty           None
        // present  absent   unchanged       None
        // present  present  empty or not    Subtract other from self, but
        //                                   erase self if result is empty
        //
        // Because there is only work to do when a key is present in both 'self'
        // and 'other', the main while loop ping-pongs back and forth until it
        // finds the next key that is the same on both sides.

        auto self_iter = roarings.begin();
        auto other_iter = other.roarings.cbegin();

        while (self_iter != roarings.end() &&
               other_iter != other.roarings.cend()) {
            auto self_key = self_iter->first;
            auto other_key = other_iter->first;
            if (self_key < other_key) {
                // Because self_key is < other_key, advance self_iter to the
                // first point where self_key >= other_key (or end).
                self_iter = roarings.lower_bound(other_key);
                continue;
            }

            if (self_key > other_key) {
                // Because self_key is > other_key, advance other_iter to the
                // first point where other_key >= self_key (or end).
                other_iter = other.roarings.lower_bound(self_key);
                continue;
            }

            // Both sides have self_key. In the logic table above, this reflects
            // the case (self.present & other.present). So subtract other from
            // self.
            auto &self_bitmap = self_iter->second;
            const auto &other_bitmap = other_iter->second;
            self_bitmap -= other_bitmap;

            if (self_bitmap.isEmpty()) {
                // ...but if subtraction is empty, remove it altogether.
                self_iter = roarings.erase(self_iter);
            } else {
                ++self_iter;
            }
            ++other_iter;
        }
        return *this;
    }

    /**
     * Compute the union of the current bitmap and the provided bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     *
     * See also the fastunion function to aggregate many bitmaps more quickly.
     */
    Roaring64Map &operator|=(const Roaring64Map &other) {
        if (this == &other) {
            // ORing *this with itself is a no-op.
            return *this;
        }

        // Logic table summarizing what to do when a given outer key is
        // present vs. absent from self and other.
        //
        // self     other    (self | other)  work to do
        // --------------------------------------------
        // absent   absent   empty           None
        // absent   present  not empty       Copy other to self and set flags
        // present  absent   unchanged       None
        // present  present  not empty       self |= other
        //
        // Because there is only work to do when a key is present in 'other',
        // the main for loop iterates over entries in 'other'.

        for (const auto &other_entry : other.roarings) {
            const auto &other_bitmap = other_entry.second;

            // Try to insert other_bitmap into self at other_key. We take
            // advantage of the fact that std::map::insert will not overwrite an
            // existing entry.
            auto insert_result = roarings.insert(other_entry);
            auto self_iter = insert_result.first;
            auto insert_happened = insert_result.second;
            auto &self_bitmap = self_iter->second;

            if (insert_happened) {
                // Key was not present in self, so insert was performed above.
                // In the logic table above, this reflects the case
                // (self.absent | other.present). Because the copy has already
                // happened, thanks to the 'insert' operation above, we just
                // need to set the copyOnWrite flag.
                self_bitmap.setCopyOnWrite(copyOnWrite);
                continue;
            }

            // Both sides have self_key, and the insert was not performed. In
            // the logic table above, this reflects the case
            // (self.present & other.present). So OR other into self.
            self_bitmap |= other_bitmap;
        }
        return *this;
    }

    /**
     * Compute the XOR of the current bitmap and the provided bitmap, writing
     * the result in the current bitmap. The provided bitmap is not modified.
     */
    Roaring64Map &operator^=(const Roaring64Map &other) {
        if (this == &other) {
            // XORing *this with itself results in the empty map.
            roarings.clear();
            return *this;
        }

        // Logic table summarizing what to do when a given outer key is
        // present vs. absent from self and other.
        //
        // self     other    (self ^ other)  work to do
        // --------------------------------------------
        // absent   absent   empty           None
        // absent   present  non-empty       Copy other to self and set flags
        // present  absent   unchanged       None
        // present  present  empty or not    XOR other into self, but erase self
        //                                   if result is empty.
        //
        // Because there is only work to do when a key is present in 'other',
        // the main for loop iterates over entries in 'other'.

        for (const auto &other_entry : other.roarings) {
            const auto &other_bitmap = other_entry.second;

            // Try to insert other_bitmap into self at other_key. We take
            // advantage of the fact that std::map::insert will not overwrite an
            // existing entry.
            auto insert_result = roarings.insert(other_entry);
            auto self_iter = insert_result.first;
            auto insert_happened = insert_result.second;
            auto &self_bitmap = self_iter->second;

            if (insert_happened) {
                // Key was not present in self, so insert was performed above.
                // In the logic table above, this reflects the case
                // (self.absent ^ other.present). Because the copy has already
                // happened, thanks to the 'insert' operation above, we just
                // need to set the copyOnWrite flag.
                self_bitmap.setCopyOnWrite(copyOnWrite);
                continue;
            }

            // Both sides have self_key, and the insert was not performed. In
            // the logic table above, this reflects the case
            // (self.present ^ other.present). So XOR other into self.
            self_bitmap ^= other_bitmap;

            if (self_bitmap.isEmpty()) {
                // ...but if intersection is empty, remove it altogether.
                roarings.erase(self_iter);
            }
        }
        return *this;
    }

    /**
     * Exchange the content of this bitmap with another.
     */
    void swap(Roaring64Map &r) { roarings.swap(r.roarings); }

    /**
     * Get the cardinality of the bitmap (number of elements).
     * Throws std::length_error in the special case where the bitmap is full
     * (cardinality() == 2^64). Check isFull() before calling to avoid
     * exception.
     */
    uint64_t cardinality() const {
        if (isFull()) {
#if ROARING_EXCEPTIONS
            throw std::length_error("bitmap is full, cardinality is 2^64, "
                                    "unable to represent in a 64-bit integer");
#else
            ROARING_TERMINATE("bitmap is full, cardinality is 2^64, "
                              "unable to represent in a 64-bit integer");
#endif
        }
        return std::accumulate(
            roarings.cbegin(), roarings.cend(), (uint64_t)0,
            [](uint64_t previous,
               const std::pair<const uint32_t, Roaring> &map_entry) {
                return previous + map_entry.second.cardinality();
            });
    }

    /**
     * Returns true if the bitmap is empty (cardinality is zero).
     */
    bool isEmpty() const {
        return std::all_of(roarings.cbegin(), roarings.cend(),
                           [](const std::pair<const uint32_t, Roaring> &map_entry) {
                               return map_entry.second.isEmpty();
                           });
    }

    /**
     * Returns true if the bitmap is full (cardinality is max uint64_t + 1).
     */
    bool isFull() const {
        // only bother to check if map is fully saturated
        //
        // we put std::numeric_limits<>::max/min in parentheses
        // to avoid a clash with the Windows.h header under Windows
        return roarings.size() ==
            ((uint64_t)(std::numeric_limits<uint32_t>::max)()) + 1
            ? std::all_of(
                roarings.cbegin(), roarings.cend(),
                [](const std::pair<const uint32_t, Roaring> &roaring_map_entry) {
                    // roarings within map are saturated if cardinality
                    // is uint32_t max + 1
                    return roaring_map_entry.second.cardinality() ==
                        ((uint64_t)
                         (std::numeric_limits<uint32_t>::max)()) +
                        1;
                })
            : false;
    }

    /**
     * Returns true if the bitmap is subset of the other.
     */
    bool isSubset(const Roaring64Map &r) const {
        for (const auto &map_entry : roarings) {
            if (map_entry.second.isEmpty()) {
                continue;
            }
            auto roaring_iter = r.roarings.find(map_entry.first);
            if (roaring_iter == r.roarings.cend())
                return false;
            else if (!map_entry.second.isSubset(roaring_iter->second))
                return false;
        }
        return true;
    }

    /**
     * Returns true if the bitmap is strict subset of the other.
     * Throws std::length_error in the special case where the bitmap is full
     * (cardinality() == 2^64). Check isFull() before calling to avoid exception.
     */
    bool isStrictSubset(const Roaring64Map &r) const {
        return isSubset(r) && cardinality() != r.cardinality();
    }

    /**
     * Convert the bitmap to an array. Write the output to "ans",
     * caller is responsible to ensure that there is enough memory
     * allocated
     * (e.g., ans = new uint32[mybitmap.cardinality()];)
     */
    void toUint64Array(uint64_t *ans) const {
        // Annoyingly, VS 2017 marks std::accumulate() as [[nodiscard]]
        (void)std::accumulate(roarings.cbegin(), roarings.cend(), ans,
                              [](uint64_t *previous,
                                 const std::pair<const uint32_t, Roaring> &map_entry) {
                                  for (uint32_t low_bits : map_entry.second)
                                      *previous++ =
                                          uniteBytes(map_entry.first, low_bits);
                                  return previous;
                              });
    }

    /**
     * Return true if the two bitmaps contain the same elements.
     */
    bool operator==(const Roaring64Map &r) const {
        // we cannot use operator == on the map because either side may contain
        // empty Roaring Bitmaps
        auto lhs_iter = roarings.cbegin();
        auto lhs_cend = roarings.cend();
        auto rhs_iter = r.roarings.cbegin();
        auto rhs_cend = r.roarings.cend();
        while (lhs_iter != lhs_cend && rhs_iter != rhs_cend) {
            auto lhs_key = lhs_iter->first, rhs_key = rhs_iter->first;
            const auto &lhs_map = lhs_iter->second, &rhs_map = rhs_iter->second;
            if (lhs_map.isEmpty()) {
                ++lhs_iter;
                continue;
            }
            if (rhs_map.isEmpty()) {
                ++rhs_iter;
                continue;
            }
            if (!(lhs_key == rhs_key)) {
                return false;
            }
            if (!(lhs_map == rhs_map)) {
                return false;
            }
            ++lhs_iter;
            ++rhs_iter;
        }
        while (lhs_iter != lhs_cend) {
            if (!lhs_iter->second.isEmpty()) {
                return false;
            }
            ++lhs_iter;
        }
        while (rhs_iter != rhs_cend) {
            if (!rhs_iter->second.isEmpty()) {
                return false;
            }
            ++rhs_iter;
        }
        return true;
    }

    /**
     * Compute the negation of the roaring bitmap within a specified interval.
     * areas outside the range are passed through unchanged.
     */
    void flip(uint64_t range_start, uint64_t range_end) {
        if (range_start >= range_end) {
          return;
        }
        uint32_t start_high = highBytes(range_start);
        uint32_t start_low = lowBytes(range_start);
        uint32_t end_high = highBytes(range_end);
        uint32_t end_low = lowBytes(range_end);

        if (start_high == end_high) {
            roarings[start_high].flip(start_low, end_low);
            return;
        }
        // we put std::numeric_limits<>::max/min in parentheses
        // to avoid a clash with the Windows.h header under Windows
        // flip operates on the range [lower_bound, upper_bound)
        const uint64_t max_upper_bound =
            static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)()) + 1;
        roarings[start_high].flip(start_low, max_upper_bound);
        roarings[start_high++].setCopyOnWrite(copyOnWrite);

        for (; start_high <= highBytes(range_end) - 1; ++start_high) {
            roarings[start_high].flip((std::numeric_limits<uint32_t>::min)(),
                                      max_upper_bound);
            roarings[start_high].setCopyOnWrite(copyOnWrite);
        }

        roarings[start_high].flip((std::numeric_limits<uint32_t>::min)(),
                                  end_low);
        roarings[start_high].setCopyOnWrite(copyOnWrite);
    }

    /**
     * Remove run-length encoding even when it is more space efficient
     * return whether a change was applied
     */
    bool removeRunCompression() {
        return std::accumulate(
            roarings.begin(), roarings.end(), true,
            [](bool previous, std::pair<const uint32_t, Roaring> &map_entry) {
                return map_entry.second.removeRunCompression() && previous;
            });
    }

    /**
     * Convert array and bitmap containers to run containers when it is more
     * efficient; also convert from run containers when more space efficient.
     * Returns true if the result has at least one run container.
     * Additional savings might be possible by calling shrinkToFit().
     */
    bool runOptimize() {
        return std::accumulate(
            roarings.begin(), roarings.end(), true,
            [](bool previous, std::pair<const uint32_t, Roaring> &map_entry) {
                return map_entry.second.runOptimize() && previous;
            });
    }

    /**
     * If needed, reallocate memory to shrink the memory usage.
     * Returns the number of bytes saved.
     */
    size_t shrinkToFit() {
        size_t savedBytes = 0;
        auto iter = roarings.begin();
        while (iter != roarings.cend()) {
            if (iter->second.isEmpty()) {
                // empty Roarings are 84 bytes
                savedBytes += 88;
                roarings.erase(iter++);
            } else {
                savedBytes += iter->second.shrinkToFit();
                iter++;
            }
        }
        return savedBytes;
    }

    /**
     * Iterate over the bitmap elements in order(start from the smallest one)
     * and call iterator once for every element until the iterator function
     * returns false. To iterate over all values, the iterator function should
     * always return true.
     *
     * The roaring_iterator64 parameter is a pointer to a function that
     * returns bool (true means that the iteration should continue while false
     * means that it should stop), and takes (uint64_t element, void* ptr) as
     * inputs.
     */
    void iterate(api::roaring_iterator64 iterator, void *ptr) const {
        for (const auto &map_entry : roarings) {
            bool should_continue =
                roaring_iterate64(&map_entry.second.roaring, iterator,
                                  uint64_t(map_entry.first) << 32, ptr);
            if (!should_continue) {
                break;
            }
        }
    }

    /**
     * If the size of the roaring bitmap is strictly greater than rank, then
     * this function returns true and set element to the element of given
     * rank.  Otherwise, it returns false.
     */
    bool select(uint64_t rnk, uint64_t *element) const {
        for (const auto &map_entry : roarings) {
            uint64_t sub_cardinality = (uint64_t)map_entry.second.cardinality();
            if (rnk < sub_cardinality) {
                *element = ((uint64_t)map_entry.first) << 32;
                // assuming little endian
                return map_entry.second.select((uint32_t)rnk,
                                               ((uint32_t *)element));
            }
            rnk -= sub_cardinality;
        }
        return false;
    }

    /**
     * Returns the number of integers that are smaller or equal to x.
     */
    uint64_t rank(uint64_t x) const {
        uint64_t result = 0;
        auto roaring_destination = roarings.find(highBytes(x));
        if (roaring_destination != roarings.cend()) {
            for (auto roaring_iter = roarings.cbegin();
                 roaring_iter != roaring_destination; ++roaring_iter) {
                result += roaring_iter->second.cardinality();
            }
            result += roaring_destination->second.rank(lowBytes(x));
            return result;
        }
        roaring_destination = roarings.lower_bound(highBytes(x));
        for (auto roaring_iter = roarings.cbegin();
             roaring_iter != roaring_destination; ++roaring_iter) {
            result += roaring_iter->second.cardinality();
        }
        return result;
    }

    /**
     * Write a bitmap to a char buffer. This is meant to be compatible with
     * the Java and Go versions. Returns how many bytes were written which
     * should be getSizeInBytes().
     *
     * Setting the portable flag to false enables a custom format that
     * can save space compared to the portable format (e.g., for very
     * sparse bitmaps).
     */
    size_t write(char *buf, bool portable = true) const {
        const char *orig = buf;
        // push map size
        uint64_t map_size = roarings.size();
        std::memcpy(buf, &map_size, sizeof(uint64_t));
        buf += sizeof(uint64_t);
        std::for_each(
            roarings.cbegin(), roarings.cend(),
            [&buf, portable](const std::pair<const uint32_t, Roaring> &map_entry) {
                // push map key
                std::memcpy(buf, &map_entry.first, sizeof(uint32_t));
                // ^-- Note: `*((uint32_t*)buf) = map_entry.first;` is undefined

                buf += sizeof(uint32_t);
                // push map value Roaring
                buf += map_entry.second.write(buf, portable);
            });
        return buf - orig;
    }

    /**
     * Read a bitmap from a serialized version. This is meant to be compatible
     * with the Java and Go versions.
     *
     * Setting the portable flag to false enable a custom format that
     * can save space compared to the portable format (e.g., for very
     * sparse bitmaps).
     *
     * This function is unsafe in the sense that if you provide bad data, many
     * bytes could be read, possibly causing a buffer overflow. See also
     * readSafe.
     */
    static Roaring64Map read(const char *buf, bool portable = true) {
        Roaring64Map result;
        // get map size
        uint64_t map_size;
        std::memcpy(&map_size, buf, sizeof(uint64_t));
        buf += sizeof(uint64_t);
        for (uint64_t lcv = 0; lcv < map_size; lcv++) {
            // get map key
            uint32_t key;
            std::memcpy(&key, buf, sizeof(uint32_t));
            // ^-- Note: `uint32_t key = *((uint32_t*)buf);` is undefined

            buf += sizeof(uint32_t);
            // read map value Roaring
            Roaring read_var = Roaring::read(buf, portable);
            // forward buffer past the last Roaring Bitmap
            buf += read_var.getSizeInBytes(portable);
            result.emplaceOrInsert(key, std::move(read_var));
        }
        return result;
    }

    /**
     * Read a bitmap from a serialized version, reading no more than maxbytes
     * bytes.  This is meant to be compatible with the Java and Go versions.
     *
     * Setting the portable flag to false enable a custom format that can save
     * space compared to the portable format (e.g., for very sparse bitmaps).
     */
    static Roaring64Map readSafe(const char *buf, size_t maxbytes) {
        if (maxbytes < sizeof(uint64_t)) {
            ROARING_TERMINATE("ran out of bytes");
        }
        Roaring64Map result;
        uint64_t map_size;
        std::memcpy(&map_size, buf, sizeof(uint64_t));
        buf += sizeof(uint64_t);
        maxbytes -= sizeof(uint64_t);
        for (uint64_t lcv = 0; lcv < map_size; lcv++) {
            if(maxbytes < sizeof(uint32_t)) {
                ROARING_TERMINATE("ran out of bytes");
            }
            uint32_t key;
            std::memcpy(&key, buf, sizeof(uint32_t));
            // ^-- Note: `uint32_t key = *((uint32_t*)buf);` is undefined

            buf += sizeof(uint32_t);
            maxbytes -= sizeof(uint32_t);
            // read map value Roaring
            Roaring read_var = Roaring::readSafe(buf, maxbytes);
            // forward buffer past the last Roaring Bitmap
            size_t tz = read_var.getSizeInBytes(true);
            buf += tz;
            maxbytes -= tz;
            result.emplaceOrInsert(key, std::move(read_var));
        }
        return result;
    }

    /**
     * Return the number of bytes required to serialize this bitmap (meant to
     * be compatible with Java and Go versions)
     *
     * Setting the portable flag to false enable a custom format that can save
     * space compared to the portable format (e.g., for very sparse bitmaps).
     */
    size_t getSizeInBytes(bool portable = true) const {
        // start with, respectively, map size and size of keys for each map
        // entry
        return std::accumulate(
            roarings.cbegin(), roarings.cend(),
            sizeof(uint64_t) + roarings.size() * sizeof(uint32_t),
            [=](size_t previous,
                const std::pair<const uint32_t, Roaring> &map_entry) {
                // add in bytes used by each Roaring
                return previous + map_entry.second.getSizeInBytes(portable);
            });
    }

    static const Roaring64Map frozenView(const char *buf) {
        // size of bitmap buffer and key
        const size_t metadata_size = sizeof(size_t) + sizeof(uint32_t);

        Roaring64Map result;

        // get map size
        uint64_t map_size;
        memcpy(&map_size, buf, sizeof(uint64_t));
        buf += sizeof(uint64_t);

        for (uint64_t lcv = 0; lcv < map_size; lcv++) {
            // pad to 32 bytes minus the metadata size
            while (((uintptr_t)buf + metadata_size) % 32 != 0) buf++;

            // get bitmap size
            size_t len;
            memcpy(&len, buf, sizeof(size_t));
            buf += sizeof(size_t);

            // get map key
            uint32_t key;
            memcpy(&key, buf, sizeof(uint32_t));
            buf += sizeof(uint32_t);

            // read map value Roaring
            const Roaring read = Roaring::frozenView(buf, len);
            result.emplaceOrInsert(key, read);

            // forward buffer past the last Roaring Bitmap
            buf += len;
        }
        return result;
    }

    // As with serialized 64-bit bitmaps, 64-bit frozen bitmaps are serialized
    // by concatenating one or more Roaring::write output buffers with the
    // preceeding map key. Unlike standard bitmap serialization, frozen bitmaps
    // must be 32-byte aligned and requires a buffer length to parse. As a
    // result, each concatenated output of Roaring::writeFrozen is preceeded by
    // padding, the buffer size (size_t), and the map key (uint32_t). The
    // padding is used to ensure 32-byte alignment, but since it is followed by
    // the buffer size and map key, it actually pads to `(x - sizeof(size_t) +
    // sizeof(uint32_t)) mod 32` to leave room for the metadata.
    void writeFrozen(char *buf) const {
        // size of bitmap buffer and key
        const size_t metadata_size = sizeof(size_t) + sizeof(uint32_t);

        // push map size
        uint64_t map_size = roarings.size();
        memcpy(buf, &map_size, sizeof(uint64_t));
        buf += sizeof(uint64_t);

        for (auto &map_entry : roarings) {
            size_t frozenSizeInBytes = map_entry.second.getFrozenSizeInBytes();

            // pad to 32 bytes minus the metadata size
            while (((uintptr_t)buf + metadata_size) % 32 != 0) buf++;

            // push bitmap size
            memcpy(buf, &frozenSizeInBytes, sizeof(size_t));
            buf += sizeof(size_t);

            // push map key
            memcpy(buf, &map_entry.first, sizeof(uint32_t));
            buf += sizeof(uint32_t);

            // push map value Roaring
            map_entry.second.writeFrozen(buf);
            buf += map_entry.second.getFrozenSizeInBytes();
        }
    }

    size_t getFrozenSizeInBytes() const {
        // size of bitmap size and map key
        const size_t metadata_size = sizeof(size_t) + sizeof(uint32_t);
        size_t ret = 0;

        // map size
        ret += sizeof(uint64_t);

        for (auto &map_entry : roarings) {
            // pad to 32 bytes minus the metadata size
            while ((ret + metadata_size) % 32 != 0) ret++;
            ret += metadata_size;

            // frozen bitmaps must be 32-byte aligned
            ret += map_entry.second.getFrozenSizeInBytes();
        }
        return ret;
    }

    /**
     * Computes the intersection between two bitmaps and returns new bitmap.
     * The current bitmap and the provided bitmap are unchanged.
     */
    Roaring64Map operator&(const Roaring64Map &o) const {
        return Roaring64Map(*this) &= o;
    }

    /**
     * Computes the difference between two bitmaps and returns new bitmap.
     * The current bitmap and the provided bitmap are unchanged.
     */
    Roaring64Map operator-(const Roaring64Map &o) const {
        return Roaring64Map(*this) -= o;
    }

    /**
     * Computes the union between two bitmaps and returns new bitmap.
     * The current bitmap and the provided bitmap are unchanged.
     */
    Roaring64Map operator|(const Roaring64Map &o) const {
        return Roaring64Map(*this) |= o;
    }

    /**
     * Computes the symmetric union between two bitmaps and returns new bitmap.
     * The current bitmap and the provided bitmap are unchanged.
     */
    Roaring64Map operator^(const Roaring64Map &o) const {
        return Roaring64Map(*this) ^= o;
    }

    /**
     * Whether or not we apply copy and write.
     */
    void setCopyOnWrite(bool val) {
        if (copyOnWrite == val) return;
        copyOnWrite = val;
        std::for_each(roarings.begin(), roarings.end(),
                      [=](std::pair<const uint32_t, Roaring> &map_entry) {
                          map_entry.second.setCopyOnWrite(val);
                      });
    }

    /**
     * Print the content of the bitmap
     */
    void printf() const {
        if (!isEmpty()) {
            auto map_iter = roarings.cbegin();
            while (map_iter->second.isEmpty()) ++map_iter;
            struct iter_data {
                uint32_t high_bits{};
                char first_char{'{'};
            } outer_iter_data;
            outer_iter_data.high_bits = roarings.begin()->first;
            map_iter->second.iterate(
                [](uint32_t low_bits, void *inner_iter_data) -> bool {
                    std::printf("%c%llu",
                                ((iter_data *)inner_iter_data)->first_char,
                                (long long unsigned)uniteBytes(
                                    ((iter_data *)inner_iter_data)->high_bits,
                                    low_bits));
                    ((iter_data *)inner_iter_data)->first_char = ',';
                    return true;
                },
                (void *)&outer_iter_data);
            std::for_each(
                ++map_iter, roarings.cend(),
                [](const std::pair<const uint32_t, Roaring> &map_entry) {
                    map_entry.second.iterate(
                        [](uint32_t low_bits, void *high_bits) -> bool {
                            std::printf(",%llu",
                                        (long long unsigned)uniteBytes(
                                            *(uint32_t *)high_bits, low_bits));
                            return true;
                        },
                        (void *)&map_entry.first);
                });
        } else
            std::printf("{");
        std::printf("}\n");
    }

    /**
     * Print the content of the bitmap into a string
     */
    std::string toString() const {
        struct iter_data {
            std::string str{}; // The empty constructor silences warnings from pedantic static analyzers.
            uint32_t high_bits{0};
            char first_char{'{'};
        } outer_iter_data;
        if (!isEmpty()) {
            auto map_iter = roarings.cbegin();
            while (map_iter->second.isEmpty()) ++map_iter;
            outer_iter_data.high_bits = roarings.begin()->first;
            map_iter->second.iterate(
                [](uint32_t low_bits, void *inner_iter_data) -> bool {
                    ((iter_data *)inner_iter_data)->str +=
                        ((iter_data *)inner_iter_data)->first_char;
                    ((iter_data *)inner_iter_data)->str += std::to_string(
                        uniteBytes(((iter_data *)inner_iter_data)->high_bits,
                                   low_bits));
                    ((iter_data *)inner_iter_data)->first_char = ',';
                    return true;
                },
                (void *)&outer_iter_data);
            std::for_each(
                ++map_iter, roarings.cend(),
                [&outer_iter_data](
                    const std::pair<const uint32_t, Roaring> &map_entry) {
                    outer_iter_data.high_bits = map_entry.first;
                    map_entry.second.iterate(
                        [](uint32_t low_bits, void *inner_iter_data) -> bool {
                            ((iter_data *)inner_iter_data)->str +=
                                ((iter_data *)inner_iter_data)->first_char;
                            ((iter_data *)inner_iter_data)->str +=
                                std::to_string(uniteBytes(
                                    ((iter_data *)inner_iter_data)->high_bits,
                                    low_bits));
                            return true;
                        },
                        (void *)&outer_iter_data);
                });
        } else
            outer_iter_data.str = '{';
        outer_iter_data.str += '}';
        return outer_iter_data.str;
    }

    /**
     * Whether or not copy and write is active.
     */
    bool getCopyOnWrite() const { return copyOnWrite; }

    /**
     * Computes the logical or (union) between "n" bitmaps (referenced by a
     * pointer).
     */
    static Roaring64Map fastunion(size_t n, const Roaring64Map **inputs) {
        Roaring64Map ans;
        // not particularly fast
        for (size_t lcv = 0; lcv < n; ++lcv) {
            ans |= *(inputs[lcv]);
        }
        return ans;
    }

    friend class Roaring64MapSetBitForwardIterator;
    friend class Roaring64MapSetBitBiDirectionalIterator;
    typedef Roaring64MapSetBitForwardIterator const_iterator;
    typedef Roaring64MapSetBitBiDirectionalIterator const_bidirectional_iterator;

    /**
     * Returns an iterator that can be used to access the position of the set
     * bits. The running time complexity of a full scan is proportional to the
     * number of set bits: be aware that if you have long strings of 1s, this
     * can be very inefficient.
     *
     * It can be much faster to use the toArray method if you want to
     * retrieve the set bits.
     */
    const_iterator begin() const;

    /**
     * A bogus iterator that can be used together with begin()
     * for constructions such as: for (auto i = b.begin(); * i!=b.end(); ++i) {}
     */
    const_iterator end() const;

private:
    typedef std::map<uint32_t, Roaring> roarings_t;
    roarings_t roarings{}; // The empty constructor silences warnings from pedantic static analyzers.
    bool copyOnWrite{false};
    static uint32_t highBytes(const uint64_t in) { return uint32_t(in >> 32); }
    static uint32_t lowBytes(const uint64_t in) { return uint32_t(in); }
    static uint64_t uniteBytes(const uint32_t highBytes,
                               const uint32_t lowBytes) {
        return (uint64_t(highBytes) << 32) | uint64_t(lowBytes);
    }
    // this is needed to tolerate gcc's C++11 libstdc++ lacking emplace
    // prior to version 4.8
    void emplaceOrInsert(const uint32_t key, const Roaring &value) {
#if defined(__GLIBCXX__) && __GLIBCXX__ < 20130322
        roarings.insert(std::make_pair(key, value));
#else
        roarings.emplace(std::make_pair(key, value));
#endif
    }

    void emplaceOrInsert(const uint32_t key, Roaring &&value) {
#if defined(__GLIBCXX__) && __GLIBCXX__ < 20130322
        roarings.insert(std::make_pair(key, std::move(value)));
#else
        roarings.emplace(key, std::move(value));
#endif
    }

    /**
     * Erases the entry pointed to by 'iter' from the 'roarings' map. Warning:
     * this invalidates 'iter'.
     */
    void eraseIfEmpty(roarings_t::iterator iter) {
        const auto &bitmap = iter->second;
        if (bitmap.isEmpty()) {
            roarings.erase(iter);
        }
    }
};

/**
 * Used to go through the set bits. Not optimally fast, but convenient.
 */
class Roaring64MapSetBitForwardIterator {
public:
    typedef std::forward_iterator_tag iterator_category;
    typedef uint64_t *pointer;
    typedef uint64_t &reference;
    typedef uint64_t value_type;
    typedef int64_t difference_type;
    typedef Roaring64MapSetBitForwardIterator type_of_iterator;

    /**
     * Provides the location of the set bit.
     */
    value_type operator*() const {
        return Roaring64Map::uniteBytes(map_iter->first, i.current_value);
    }

    bool operator<(const type_of_iterator &o) const {
        if (map_iter == map_end) return false;
        if (o.map_iter == o.map_end) return true;
        return **this < *o;
    }

    bool operator<=(const type_of_iterator &o) const {
        if (o.map_iter == o.map_end) return true;
        if (map_iter == map_end) return false;
        return **this <= *o;
    }

    bool operator>(const type_of_iterator &o) const {
        if (o.map_iter == o.map_end) return false;
        if (map_iter == map_end) return true;
        return **this > *o;
    }

    bool operator>=(const type_of_iterator &o) const {
        if (map_iter == map_end) return true;
        if (o.map_iter == o.map_end) return false;
        return **this >= *o;
    }

    type_of_iterator &operator++() {  // ++i, must returned inc. value
        if (i.has_value == true) roaring_advance_uint32_iterator(&i);
        while (!i.has_value) {
            map_iter++;
            if (map_iter == map_end) return *this;
            roaring_init_iterator(&map_iter->second.roaring, &i);
        }
        return *this;
    }

    type_of_iterator operator++(int) {  // i++, must return orig. value
        Roaring64MapSetBitForwardIterator orig(*this);
        roaring_advance_uint32_iterator(&i);
        while (!i.has_value) {
            map_iter++;
            if (map_iter == map_end) return orig;
            roaring_init_iterator(&map_iter->second.roaring, &i);
        }
        return orig;
    }

    bool move(const value_type& x) {
        map_iter = p.lower_bound(Roaring64Map::highBytes(x));
        if (map_iter != p.cend()) {
            roaring_init_iterator(&map_iter->second.roaring, &i);
            if (map_iter->first == Roaring64Map::highBytes(x)) {
                if (roaring_move_uint32_iterator_equalorlarger(&i, Roaring64Map::lowBytes(x)))
                    return true;
                map_iter++;
                if (map_iter == map_end) return false;
                roaring_init_iterator(&map_iter->second.roaring, &i);
            }
            return true;
        }
        return false;
    }

    bool operator==(const Roaring64MapSetBitForwardIterator &o) const {
        if (map_iter == map_end && o.map_iter == o.map_end) return true;
        if (o.map_iter == o.map_end) return false;
        return **this == *o;
    }

    bool operator!=(const Roaring64MapSetBitForwardIterator &o) const {
        if (map_iter == map_end && o.map_iter == o.map_end) return false;
        if (o.map_iter == o.map_end) return true;
        return **this != *o;
    }

    Roaring64MapSetBitForwardIterator &operator=(const Roaring64MapSetBitForwardIterator& r) {
        map_iter = r.map_iter;
        map_end = r.map_end;
        i = r.i;
        return *this;
    }

    Roaring64MapSetBitForwardIterator(const Roaring64MapSetBitForwardIterator& r)
        : p(r.p),
          map_iter(r.map_iter),
          map_end(r.map_end),
          i(r.i)
    {}

    Roaring64MapSetBitForwardIterator(const Roaring64Map &parent,
                                      bool exhausted = false)
        : p(parent.roarings), map_end(parent.roarings.cend()) {
        if (exhausted || parent.roarings.empty()) {
            map_iter = parent.roarings.cend();
        } else {
            map_iter = parent.roarings.cbegin();
            roaring_init_iterator(&map_iter->second.roaring, &i);
            while (!i.has_value) {
                map_iter++;
                if (map_iter == map_end) return;
                roaring_init_iterator(&map_iter->second.roaring, &i);
            }
        }
    }

protected:
    const std::map<uint32_t, Roaring>& p;
    std::map<uint32_t, Roaring>::const_iterator map_iter{}; // The empty constructor silences warnings from pedantic static analyzers.
    std::map<uint32_t, Roaring>::const_iterator map_end{}; // The empty constructor silences warnings from pedantic static analyzers.
    api::roaring_uint32_iterator_t i{}; // The empty constructor silences warnings from pedantic static analyzers.
};

class Roaring64MapSetBitBiDirectionalIterator final :public Roaring64MapSetBitForwardIterator {
public:
    explicit Roaring64MapSetBitBiDirectionalIterator(const Roaring64Map &parent,
                                                     bool exhausted = false)
        : Roaring64MapSetBitForwardIterator(parent, exhausted), map_begin(parent.roarings.cbegin())
    {}

    Roaring64MapSetBitBiDirectionalIterator &operator=(const Roaring64MapSetBitForwardIterator& r) {
        *(Roaring64MapSetBitForwardIterator*)this = r;
        return *this;
    }

    Roaring64MapSetBitBiDirectionalIterator& operator--() { //  --i, must return dec.value
        if (map_iter == map_end) {
            --map_iter;
            roaring_init_iterator_last(&map_iter->second.roaring, &i);
            if (i.has_value) return *this;
        }

        roaring_previous_uint32_iterator(&i);
        while (!i.has_value) {
            if (map_iter == map_begin) return *this;
            map_iter--;
            roaring_init_iterator_last(&map_iter->second.roaring, &i);
        }
        return *this;
    }

    Roaring64MapSetBitBiDirectionalIterator operator--(int) {  // i--, must return orig. value
        Roaring64MapSetBitBiDirectionalIterator orig(*this);
        if (map_iter == map_end) {
            --map_iter;
            roaring_init_iterator_last(&map_iter->second.roaring, &i);
            return orig;
        }

        roaring_previous_uint32_iterator(&i);
        while (!i.has_value) {
            if (map_iter == map_begin) return orig;
            map_iter--;
            roaring_init_iterator_last(&map_iter->second.roaring, &i);
        }
        return orig;
    }

protected:
    std::map<uint32_t, Roaring>::const_iterator map_begin;
};

inline Roaring64MapSetBitForwardIterator Roaring64Map::begin() const {
    return Roaring64MapSetBitForwardIterator(*this);
}

inline Roaring64MapSetBitForwardIterator Roaring64Map::end() const {
    return Roaring64MapSetBitForwardIterator(*this, true);
}

}  // namespace roaring

#endif /* INCLUDE_ROARING_64_MAP_HH_ */
