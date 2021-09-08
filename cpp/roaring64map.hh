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
using roaring::Roaring;

namespace roaring {

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
     * Construct a roaring object from the C struct.
     *
     * Passing a NULL point is unsafe.
     */
    explicit Roaring64Map(roaring_bitmap_t *s) {
        Roaring r(s);
        emplaceOrInsert(0, r);
    }

    Roaring64Map(const Roaring64Map& r)
      : roarings(r.roarings),
        copyOnWrite(r.copyOnWrite) { }

    Roaring64Map(Roaring64Map&& r)
      : roarings(r.roarings),
        copyOnWrite(r.copyOnWrite) { }

	/**
	 * Assignment operator.
	 */
	Roaring64Map &operator=(const Roaring64Map &r) {
		roarings = r.roarings;
		return *this;
	}

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
     *
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
     * Add value n_args from pointer vals
     *
     */
    void addMany(size_t n_args, const uint32_t *vals) {
        for (size_t lcv = 0; lcv < n_args; lcv++) {
            roarings[0].add(vals[lcv]);
            roarings[0].setCopyOnWrite(copyOnWrite);
        }
    }
    void addMany(size_t n_args, const uint64_t *vals) {
        for (size_t lcv = 0; lcv < n_args; lcv++) {
            roarings[highBytes(vals[lcv])].add(lowBytes(vals[lcv]));
            roarings[highBytes(vals[lcv])].setCopyOnWrite(copyOnWrite);
        }
    }

    /**
     * Remove value x
     *
     */
    void remove(uint32_t x) { roarings[0].remove(x); }
    void remove(uint64_t x) {
        auto roaring_iter = roarings.find(highBytes(x));
        if (roaring_iter != roarings.cend())
            roaring_iter->second.remove(lowBytes(x));
    }

    /**
     * Remove value x
     * Returns true if a new value was removed, false if the value was not existing.
     */
    bool removeChecked(uint32_t x) {
        return roarings[0].removeChecked(x);
    }
    bool removeChecked(uint64_t x) {
        auto roaring_iter = roarings.find(highBytes(x));
        if (roaring_iter != roarings.cend())
            return roaring_iter->second.removeChecked(lowBytes(x));
        return false;
    }

	/**
     * Clear the bitmap
     */
	void clear() {
		roarings.clear();
	}

    /**
     * Return the largest value (if not empty)
     *
     */
    uint64_t maximum() const {
        for (auto roaring_iter = roarings.crbegin();
             roaring_iter != roarings.crend(); ++roaring_iter) {
            if (!roaring_iter->second.isEmpty()) {
                return uniteBytes(roaring_iter->first,
                                  roaring_iter->second.maximum());
            }
        }
        // we put std::numeric_limits<>::max/min in parenthesis
        // to avoid a clash with the Windows.h header under Windows
        return (std::numeric_limits<uint64_t>::min)();
    }

    /**
     * Return the smallest value (if not empty)
     *
     */
    uint64_t minimum() const {
        for (auto roaring_iter = roarings.cbegin();
             roaring_iter != roarings.cend(); ++roaring_iter) {
            if (!roaring_iter->second.isEmpty()) {
                return uniteBytes(roaring_iter->first,
                                  roaring_iter->second.minimum());
            }
        }
        // we put std::numeric_limits<>::max/min in parenthesis
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
     * Compute the intersection between the current bitmap and the provided
     * bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     */
    Roaring64Map &operator&=(const Roaring64Map &r) {
        for (auto &map_entry : roarings) {
            if (r.roarings.count(map_entry.first) == 1)
                map_entry.second &= r.roarings.at(map_entry.first);
            else
                map_entry.second = Roaring();
        }
        return *this;
    }

    /**
     * Compute the difference between the current bitmap and the provided
     * bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     */
    Roaring64Map &operator-=(const Roaring64Map &r) {
        for (auto &map_entry : roarings) {
            if (r.roarings.count(map_entry.first) == 1)
                map_entry.second -= r.roarings.at(map_entry.first);
        }
        return *this;
    }

    /**
     * Compute the union between the current bitmap and the provided bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     *
     * See also the fastunion function to aggregate many bitmaps more quickly.
     */
    Roaring64Map &operator|=(const Roaring64Map &r) {
        for (const auto &map_entry : r.roarings) {
            if (roarings.count(map_entry.first) == 0) {
                roarings[map_entry.first] = map_entry.second;
                roarings[map_entry.first].setCopyOnWrite(copyOnWrite);
            } else
                roarings[map_entry.first] |= map_entry.second;
        }
        return *this;
    }

    /**
     * Compute the symmetric union between the current bitmap and the provided
     * bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     */
    Roaring64Map &operator^=(const Roaring64Map &r) {
        for (const auto &map_entry : r.roarings) {
            if (roarings.count(map_entry.first) == 0) {
                roarings[map_entry.first] = map_entry.second;
                roarings[map_entry.first].setCopyOnWrite(copyOnWrite);
            } else
                roarings[map_entry.first] ^= map_entry.second;
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
               const std::pair<uint32_t, Roaring> &map_entry) {
                return previous + map_entry.second.cardinality();
            });
    }

    /**
    * Returns true if the bitmap is empty (cardinality is zero).
    */
    bool isEmpty() const {
        return std::all_of(roarings.cbegin(), roarings.cend(),
                           [](const std::pair<uint32_t, Roaring> &map_entry) {
                               return map_entry.second.isEmpty();
                           });
    }

    /**
    * Returns true if the bitmap is full (cardinality is max uint64_t + 1).
    */
    bool isFull() const {
        // only bother to check if map is fully saturated
        //
        // we put std::numeric_limits<>::max/min in parenthesis
        // to avoid a clash with the Windows.h header under Windows
        return roarings.size() ==
                       ((uint64_t)(std::numeric_limits<uint32_t>::max)()) + 1
                   ? std::all_of(
                         roarings.cbegin(), roarings.cend(),
                         [](const std::pair<uint32_t, Roaring> &roaring_map_entry) {
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
                                 const std::pair<uint32_t, Roaring> &map_entry) {
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
        auto rhs_iter = r.roarings.cbegin();
        do {
            // if the left map has reached its end, ensure that the right map
            // contains only empty Bitmaps
            if (lhs_iter == roarings.cend()) {
                while (rhs_iter != r.roarings.cend()) {
                    if (rhs_iter->second.isEmpty()) {
                        ++rhs_iter;
                        continue;
                    }
                    return false;
                }
                return true;
            }
            // if the left map has an empty bitmap, skip it
            if (lhs_iter->second.isEmpty()) {
                ++lhs_iter;
                continue;
            }

            do {
                // if the right map has reached its end, ensure that the right
                // map contains only empty Bitmaps
                if (rhs_iter == r.roarings.cend()) {
                    while (lhs_iter != roarings.cend()) {
                        if (lhs_iter->second.isEmpty()) {
                            ++lhs_iter;
                            continue;
                        }
                        return false;
                    }
                    return true;
                }
                // if the right map has an empty bitmap, skip it
                if (rhs_iter->second.isEmpty()) {
                    ++rhs_iter;
                    continue;
                }
            } while (false);
            // if neither map has reached its end ensure elements are equal and
            // move to the next element in both
        } while (lhs_iter++->second == rhs_iter++->second);
        return false;
    }

    /**
     * compute the negation of the roaring bitmap within a specified interval.
     * areas outside the range are passed through unchanged.
     */
    void flip(uint64_t range_start, uint64_t range_end) {
        uint32_t start_high = highBytes(range_start);
        uint32_t start_low = lowBytes(range_start);
        uint32_t end_high = highBytes(range_end);
        uint32_t end_low = lowBytes(range_end);

        if (start_high == end_high) {
            roarings[start_high].flip(start_low, end_low);
            return;
        }
        // we put std::numeric_limits<>::max/min in parenthesis
        // to avoid a clash with the Windows.h header under Windows
        roarings[start_high].flip(start_low,
                                  (std::numeric_limits<uint32_t>::max)());
        roarings[start_high++].setCopyOnWrite(copyOnWrite);

        for (; start_high <= highBytes(range_end) - 1; ++start_high) {
            roarings[start_high].flip((std::numeric_limits<uint32_t>::min)(),
                                      (std::numeric_limits<uint32_t>::max)());
            roarings[start_high].setCopyOnWrite(copyOnWrite);
        }

        roarings[start_high].flip((std::numeric_limits<uint32_t>::min)(),
                                  end_low);
        roarings[start_high].setCopyOnWrite(copyOnWrite);
    }

    /**
     *  Remove run-length encoding even when it is more space efficient
     *  return whether a change was applied
     */
    bool removeRunCompression() {
        return std::accumulate(
            roarings.begin(), roarings.end(), true,
            [](bool previous, std::pair<const uint32_t, Roaring> &map_entry) {
                return map_entry.second.removeRunCompression() && previous;
            });
    }

    /** convert array and bitmap containers to run containers when it is more
     * efficient;
     * also convert from run containers when more space efficient.  Returns
     * true if the result has at least one run container.
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
     * If needed, reallocate memory to shrink the memory usage. Returns
     * the number of bytes saved.
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
     * and call iterator once for every element until the iterator function returns
     * false. To iterate over all values, the iterator function should always return
     * true.
     *
     * The roaring_iterator64 parameter is a pointer to a function that returns bool
     * (true means that the iteration should continue while false means that it
     * should stop), and takes (uint64_t element, void* ptr) as inputs.
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
     this
       function returns true and set element to the element of given rank.
       Otherwise, it returns false.
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
     * write a bitmap to a char buffer. This is meant to be compatible with
     * the
     * Java and Go versions. Returns how many bytes were written which should be
     * getSizeInBytes().
     *
     * Setting the portable flag to false enable a custom format that
     * can save space compared to the portable format (e.g., for very
     * sparse bitmaps).
     */
    size_t write(char *buf, bool portable = true) const {
        const char *orig = buf;
        // push map size
        *((uint64_t *)buf) = roarings.size();
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
     * read a bitmap from a serialized version. This is meant to be compatible
     * with
     * the
     * Java and Go versions.
     *
     * Setting the portable flag to false enable a custom format that
     * can save space compared to the portable format (e.g., for very
     * sparse bitmaps).
     *
     * This function is unsafe in the sense that if you provide bad data,
     * many bytes could be read, possibly causing a buffer overflow. See also readSafe.
     */
    static Roaring64Map read(const char *buf, bool portable = true) {
        Roaring64Map result;
        // get map size
        uint64_t map_size = *((uint64_t *)buf);
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
     * read a bitmap from a serialized version, reading no more than maxbytes bytes.
     * This is meant to be compatible with the Java and Go versions.
     *
     * Setting the portable flag to false enable a custom format that
     * can save space compared to the portable format (e.g., for very
     * sparse bitmaps).
     */
    static Roaring64Map readSafe(const char *buf, size_t maxbytes) {
        Roaring64Map result;
        // get map size
        uint64_t map_size = *((uint64_t *)buf);
        buf += sizeof(uint64_t);
        for (uint64_t lcv = 0; lcv < map_size; lcv++) {
            // get map key
            if(maxbytes < sizeof(uint32_t)) {
#if ROARING_EXCEPTIONS
                throw std::runtime_error("ran out of bytes");
#else
                ROARING_TERMINATE("ran out of bytes");
#endif
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
     * How many bytes are required to serialize this bitmap (meant to be
     * compatible
     * with Java and Go versions)
     *
     * Setting the portable flag to false enable a custom format that
     * can save space compared to the portable format (e.g., for very
     * sparse bitmaps).
     */
    size_t getSizeInBytes(bool portable = true) const {
        // start with, respectively, map size and size of keys for each map
        // entry
        return std::accumulate(
            roarings.cbegin(), roarings.cend(),
            sizeof(uint64_t) + roarings.size() * sizeof(uint32_t),
            [=](size_t previous,
                const std::pair<uint32_t, Roaring> &map_entry) {
                // add in bytes used by each Roaring
                return previous + map_entry.second.getSizeInBytes(portable);
            });
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
     * computes the logical or (union) between "n" bitmaps (referenced by a
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
    * Returns an iterator that can be used to access the position of the
    * set bits. The running time complexity of a full scan is proportional to
    * the
    * number
    * of set bits: be aware that if you have long strings of 1s, this can be
    * very inefficient.
    *
    * It can be much faster to use the toArray method if you want to
    * retrieve the set bits.
    */
    const_iterator begin() const;

    /**
    * A bogus iterator that can be used together with begin()
    * for constructions such as for(auto i = b.begin();
    * i!=b.end(); ++i) {}
    */
    const_iterator end() const;

   private:
    std::map<uint32_t, Roaring> roarings{}; // The empty constructor silences warnings from pedantic static analyzers.
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
        roarings.emplace(key, value);
#endif
    }
};

/**
 * Used to go through the set bits. Not optimally fast, but convenient.
 */
class Roaring64MapSetBitForwardIterator {
   public:
    typedef std::forward_iterator_tag iterator_category;
    typedef uint64_t *pointer;
    typedef uint64_t &reference_type;
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
    { }

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
        : Roaring64MapSetBitForwardIterator(parent, exhausted), map_begin(parent.roarings.cbegin()) {}

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
