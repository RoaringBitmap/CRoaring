/*
A C++ header for Roaring Bitmaps.
*/
#ifndef INCLUDE_ROARING_HH_
#define INCLUDE_ROARING_HH_

#include <stdarg.h>

#include <roaring/roaring.h>
#include <algorithm>
#include <new>
#include <stdexcept>
#include <string>

class RoaringSetBitForwardIterator;

class Roaring {
   public:
    /**
     * Create an empty bitmap
     */
    Roaring() {
        bool is_ok = ra_init(&roaring.high_low_container);
        if (!is_ok) {
            throw std::runtime_error("failed memory alloc in constructor");
        }
        roaring.copy_on_write = false;
    }

    /**
     * Construct a bitmap from a list of integer values.
     */
    Roaring(size_t n, const uint32_t *data) : Roaring() {
        roaring_bitmap_add_many(&roaring, n, data);
    }
    /**
     * Copy constructor
     */
    Roaring(const Roaring &r) {
        bool is_ok =
            ra_copy(&r.roaring.high_low_container, &roaring.high_low_container,
                    r.roaring.copy_on_write);
        if (!is_ok) {
            throw std::runtime_error("failed memory alloc in constructor");
        }
        roaring.copy_on_write = r.roaring.copy_on_write;
    }

    /**
     * Construct a roaring object from the C struct.
     *
     * Passing a NULL point is unsafe.
     * the pointer to the C struct will be invalid after the call.
     */
    Roaring(roaring_bitmap_t *s) {
        // steal the interior struct
        roaring.high_low_container = s->high_low_container;
        roaring.copy_on_write = s->copy_on_write;
        // deallocate the old container
        free(s);
    }

    /**
     * Construct a bitmap from a list of integer values.
     */
    static Roaring bitmapOf(size_t n, ...) {
        Roaring ans;
        va_list vl;
        va_start(vl, n);
        for (size_t i = 0; i < n; i++) {
            ans.add(va_arg(vl, uint32_t));
        }
        va_end(vl);
        return ans;
    }

    /**
     * Add value x
     *
     */
    void add(uint32_t x) { roaring_bitmap_add(&roaring, x); }

    /**
     * Add value n_args from pointer vals
     *
     */
    void addMany(size_t n_args, const uint32_t *vals) {
        roaring_bitmap_add_many(&roaring, n_args, vals);
    }

    /**
     * Remove value x
     *
     */
    void remove(uint32_t x) { roaring_bitmap_remove(&roaring, x); }

    /**
     * Return the largest value (if not empty)
     *
     */
    uint32_t maximum() const { return roaring_bitmap_maximum(&roaring); }

    /**
    * Return the smallest value (if not empty)
    *
    */
    uint32_t minimum() const { return roaring_bitmap_minimum(&roaring); }

    /**
     * Check if value x is present
     */
    bool contains(uint32_t x) const {
        return roaring_bitmap_contains(&roaring, x);
    }

    /**
     * Destructor
     */
    ~Roaring() { ra_clear(&roaring.high_low_container); }

    /**
     * Copies the content of the provided bitmap, and
     * discard the current content.
     */
    Roaring &operator=(const Roaring &r) {
        ra_clear(&roaring.high_low_container);
        bool is_ok =
            ra_copy(&r.roaring.high_low_container, &roaring.high_low_container,
                    r.roaring.copy_on_write);
        if (!is_ok) {
            throw std::runtime_error("failed memory alloc in assignment");
        }
        roaring.copy_on_write = r.roaring.copy_on_write;
        return *this;
    }

    /**
     * Compute the intersection between the current bitmap and the provided
     * bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     */
    Roaring &operator&=(const Roaring &r) {
        roaring_bitmap_and_inplace(&roaring, &r.roaring);
        return *this;
    }

    /**
     * Compute the difference between the current bitmap and the provided
     * bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     */
    Roaring &operator-=(const Roaring &r) {
        roaring_bitmap_andnot_inplace(&roaring, &r.roaring);
        return *this;
    }

    /**
     * Compute the union between the current bitmap and the provided bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     *
     * See also the fastunion function to aggregate many bitmaps more quickly.
     */
    Roaring &operator|=(const Roaring &r) {
        roaring_bitmap_or_inplace(&roaring, &r.roaring);
        return *this;
    }

    /**
     * Compute the symmetric union between the current bitmap and the provided
     * bitmap,
     * writing the result in the current bitmap. The provided bitmap is not
     * modified.
     */
    Roaring &operator^=(const Roaring &r) {
        roaring_bitmap_xor_inplace(&roaring, &r.roaring);
        return *this;
    }

    /**
     * Exchange the content of this bitmap with another.
     */
    void swap(Roaring &r) { std::swap(r.roaring, roaring); }

    /**
     * Get the cardinality of the bitmap (number of elements).
     */
    uint64_t cardinality() const {
        return roaring_bitmap_get_cardinality(&roaring);
    }

    /**
    * Returns true if the bitmap is empty (cardinality is zero).
    */
    bool isEmpty() const { return roaring_bitmap_is_empty(&roaring); }

    /**
    * Returns true if the bitmap is subset of the other.
    */
    bool isSubset(const Roaring &r) const {
        return roaring_bitmap_is_subset(&roaring, &r.roaring);
    }

    /**
    * Returns true if the bitmap is strict subset of the other.
    */
    bool isStrictSubset(const Roaring &r) const {
        return roaring_bitmap_is_strict_subset(&roaring, &r.roaring);
    }

    /**
     * Convert the bitmap to an array. Write the output to "ans",
     * caller is responsible to ensure that there is enough memory
     * allocated
     * (e.g., ans = new uint32[mybitmap.cardinality()];)
     */
    void toUint32Array(uint32_t *ans) const {
        roaring_bitmap_to_uint32_array(&roaring, ans);
    }

    /**
     * Return true if the two bitmaps contain the same elements.
     */
    bool operator==(const Roaring &r) const {
        return roaring_bitmap_equals(&roaring, &r.roaring);
    }

    /**
     * compute the negation of the roaring bitmap within a specified interval.
     * areas outside the range are passed through unchanged.
     */
    void flip(uint64_t range_start, uint64_t range_end) {
        roaring_bitmap_flip_inplace(&roaring, range_start, range_end);
    }

    /**
     *  Remove run-length encoding even when it is more space efficient
     *  return whether a change was applied
     */
    bool removeRunCompression() {
        return roaring_bitmap_remove_run_compression(&roaring);
    }

    /** convert array and bitmap containers to run containers when it is more
     * efficient;
     * also convert from run containers when more space efficient.  Returns
     * true if the result has at least one run container.
     * Additional savings might be possible by calling shrinkToFit().
     */
    bool runOptimize() { return roaring_bitmap_run_optimize(&roaring); }

    /**
     * If needed, reallocate memory to shrink the memory usage. Returns
     * the number of bytes saved.
    */
    size_t shrinkToFit() { return roaring_bitmap_shrink_to_fit(&roaring); }

    /**
     * Iterate over the bitmap elements. The function iterator is called once
     * for
     *  all the values with ptr (can be NULL) as the second parameter of each
     * call.
     *
     *  roaring_iterator is simply a pointer to a function that returns void,
     *  and takes (uint32_t,void*) as inputs.
     */
    void iterate(roaring_iterator iterator, void *ptr) const {
        roaring_iterate(&roaring, iterator, ptr);
    }

    /**
     * If the size of the roaring bitmap is strictly greater than rank, then
     * this function returns true and set element to the element of given rank.
     *   Otherwise, it returns false.
     */
    bool select(uint32_t rnk, uint32_t *element) const {
        return roaring_bitmap_select(&roaring, rnk, element);
    }
    /**
     * Computes the size of the intersection between two bitmaps.
     *
     */
    uint64_t and_cardinality(const Roaring &r) const {
        return roaring_bitmap_and_cardinality(&roaring, &r.roaring);
    }


    /**
     * Check whether the two bitmaps intersect.
     *
     */
    bool intersect(const Roaring &r) const {
    	 return roaring_bitmap_intersect(&roaring, &r.roaring);
    }

    /**
     * Computes the Jaccard index between two bitmaps. (Also known as the
     * Tanimoto distance,
     * or the Jaccard similarity coefficient)
     *
     * The Jaccard index is undefined if both bitmaps are empty.
     *
     */
    double jaccard_index(const Roaring &r) const {
        return roaring_bitmap_jaccard_index(&roaring, &r.roaring);
    }

    /**
     * Computes the size of the union between two bitmaps.
     *
     */
    uint64_t or_cardinality(const Roaring &r) const {
        return roaring_bitmap_or_cardinality(&roaring, &r.roaring);
    }

    /**
     * Computes the size of the difference (andnot) between two bitmaps.
     *
     */
    uint64_t andnot_cardinality(const Roaring &r) const {
        return roaring_bitmap_andnot_cardinality(&roaring, &r.roaring);
    }

    /**
     * Computes the size of the symmetric difference (andnot) between two
     * bitmaps.
     *
     */
    uint64_t xor_cardinality(const Roaring &r) const {
        return roaring_bitmap_xor_cardinality(&roaring, &r.roaring);
    }

    /**
    * Returns the number of integers that are smaller or equal to x.
    */
    uint64_t rank(uint32_t x) const { return roaring_bitmap_rank(&roaring, x); }
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
        if (portable)
            return roaring_bitmap_portable_serialize(&roaring, buf);
        else
            return roaring_bitmap_serialize(&roaring, buf);
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
     */
    static Roaring read(const char *buf, bool portable = true) {
        if (!portable) {
            if (*(const unsigned char *)buf == SERIALIZATION_ARRAY_UINT32) {
                /* This looks like a compressed set of uint32_t elements */
                uint32_t card;
                memcpy(&card, buf + 1, sizeof(uint32_t));
                const uint32_t *elems =
                    (const uint32_t *)(buf + 1 + sizeof(uint32_t));

                return Roaring((size_t)card, elems);
            } else if (buf[0] == SERIALIZATION_CONTAINER) {
                buf += 1;
            } else
                throw std::runtime_error(
                    "bad serialization type designator in read input");
        }
        {
            Roaring ans;
            bool is_ok =
                ra_portable_deserialize(&ans.roaring.high_low_container, buf);
            if (!is_ok) {
                throw std::runtime_error("failed memory alloc while reading");
            }
            return ans;
        }
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
        if (portable)
            return roaring_bitmap_portable_size_in_bytes(&roaring);
        else
            return roaring_bitmap_size_in_bytes(&roaring);
    }

    /**
     * Computes the intersection between two bitmaps and returns new bitmap.
     * The current bitmap and the provided bitmap are unchanged.
     */
    Roaring operator&(const Roaring &o) const {
        roaring_bitmap_t *r = roaring_bitmap_and(&roaring, &o.roaring);
        if (r == NULL) {
            throw std::runtime_error("failed materalization in and");
        }
        return Roaring(r);
    }

    /**
     * Computes the difference between two bitmaps and returns new bitmap.
     * The current bitmap and the provided bitmap are unchanged.
     */
    Roaring operator-(const Roaring &o) const {
        roaring_bitmap_t *r = roaring_bitmap_andnot(&roaring, &o.roaring);
        if (r == NULL) {
            throw std::runtime_error("failed materalization in andnot");
        }
        return Roaring(r);
    }

    /**
     * Computes the union between two bitmaps and returns new bitmap.
     * The current bitmap and the provided bitmap are unchanged.
     */
    Roaring operator|(const Roaring &o) const {
        roaring_bitmap_t *r = roaring_bitmap_or(&roaring, &o.roaring);
        if (r == NULL) {
            throw std::runtime_error("failed materalization in or");
        }
        return Roaring(r);
    }

    /**
     * Computes the symmetric union between two bitmaps and returns new bitmap.
     * The current bitmap and the provided bitmap are unchanged.
     */
    Roaring operator^(const Roaring &o) const {
        roaring_bitmap_t *r = roaring_bitmap_xor(&roaring, &o.roaring);
        if (r == NULL) {
            throw std::runtime_error("failed materalization in xor");
        }
        return Roaring(r);
    }

    /**
     * Whether or not we apply copy and write.
     */
    void setCopyOnWrite(bool val) { roaring.copy_on_write = val; }

    /**
     * Print the content of the bitmap
     */
    void printf() const { roaring_bitmap_printf(&roaring); }

    /**
     * Print the content of the bitmap into a string
     */
    std::string toString() const {
        struct iter_data {
            std::string str;
            char first_char = '{';
        } outer_iter_data;
        if (!isEmpty()) {
            iterate(
                [](uint32_t value, void *inner_iter_data) -> bool {
                    ((iter_data *)inner_iter_data)->str +=
                        ((iter_data *)inner_iter_data)->first_char;
                    ((iter_data *)inner_iter_data)->str +=
                        std::to_string(value);
                    if (((iter_data *)inner_iter_data)->first_char == '{')
                        ((iter_data *)inner_iter_data)->first_char = ',';
                    return true;
                },
                (void *)&outer_iter_data);
        } else
            outer_iter_data.str = '{';
        outer_iter_data.str += '}';
        return outer_iter_data.str;
    }

    /**
     * Whether or not copy and write is active.
     */
    bool getCopyOnWrite() const { return roaring.copy_on_write; }

    /**
     * computes the logical or (union) between "n" bitmaps (referenced by a
     * pointer).
     */
    static Roaring fastunion(size_t n, const Roaring **inputs) {
        const roaring_bitmap_t **x =
            (const roaring_bitmap_t **)malloc(n * sizeof(roaring_bitmap_t *));
        if (x == NULL) {
            throw std::runtime_error("failed memory alloc in fastunion");
        }
        for (size_t k = 0; k < n; ++k) x[k] = &inputs[k]->roaring;

        roaring_bitmap_t *c_ans = roaring_bitmap_or_many(n, x);
        if (c_ans == NULL) {
            free(x);
            throw std::runtime_error("failed memory alloc in fastunion");
        }
        Roaring ans(c_ans);
        free(x);
        return ans;
    }

    typedef RoaringSetBitForwardIterator const_iterator;

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
    const_iterator &end() const;

    roaring_bitmap_t roaring;
};

/**
 * Used to go through the set bits. Not optimally fast, but convenient.
 */
class RoaringSetBitForwardIterator final {
   public:
    typedef std::forward_iterator_tag iterator_category;
    typedef uint32_t *pointer;
    typedef uint32_t &reference_type;
    typedef uint32_t value_type;
    typedef int32_t difference_type;
    typedef RoaringSetBitForwardIterator type_of_iterator;

    /**
     * Provides the location of the set bit.
     */
    value_type operator*() const { return i.current_value; }

    bool operator<(const type_of_iterator &o) {
        if (!i.has_value) return false;
        if (!o.i.has_value) return true;
        return i.current_value < *o;
    }

    bool operator<=(const type_of_iterator &o) {
        if (!o.i.has_value) return true;
        if (!i.has_value) return false;
        return i.current_value <= *o;
    }

    bool operator>(const type_of_iterator &o) {
        if (!o.i.has_value) return false;
        if (!i.has_value) return true;
        return i.current_value > *o;
    }

    bool operator>=(const type_of_iterator &o) {
        if (!i.has_value) return true;
        if (!o.i.has_value) return false;
        return i.current_value >= *o;
    }

    type_of_iterator &operator++() {  // ++i, must returned inc. value
        roaring_advance_uint32_iterator(&i);
        return *this;
    }

    type_of_iterator operator++(int) {  // i++, must return orig. value
        RoaringSetBitForwardIterator orig(*this);
        roaring_advance_uint32_iterator(&i);
        return orig;
    }

    bool operator==(const RoaringSetBitForwardIterator &o) {
        return i.current_value == *o && i.has_value == o.i.has_value;
    }

    bool operator!=(const RoaringSetBitForwardIterator &o) {
        return i.current_value != *o || i.has_value != o.i.has_value;
    }

    RoaringSetBitForwardIterator(const Roaring &parent,
                                 bool exhausted = false) {
        if (exhausted) {
            i.parent = &parent.roaring;
            i.container_index = INT32_MAX;
            i.has_value = false;
            i.current_value = UINT32_MAX;
        } else {
            roaring_init_iterator(&parent.roaring, &i);
        }
    }

    RoaringSetBitForwardIterator &operator=(
        const RoaringSetBitForwardIterator &o) = default;
    RoaringSetBitForwardIterator &operator=(RoaringSetBitForwardIterator &&o) =
        default;

    ~RoaringSetBitForwardIterator() = default;

    RoaringSetBitForwardIterator(const RoaringSetBitForwardIterator &o)
        : i(o.i) {}

    roaring_uint32_iterator_t i;
};

inline RoaringSetBitForwardIterator Roaring::begin() const {
    return RoaringSetBitForwardIterator(*this);
}

inline RoaringSetBitForwardIterator &Roaring::end() const {
    static RoaringSetBitForwardIterator e(*this, true);
    return e;
}

#endif /* INCLUDE_ROARING_HH_ */
