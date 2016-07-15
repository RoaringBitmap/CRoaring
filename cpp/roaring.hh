/*
A C++ header for Roaring Bitmaps.
*/
#ifndef INCLUDE_ROARING_HH_
#define INCLUDE_ROARING_HH_

#include <stdarg.h>

#include <algorithm>
#include <new>
#include <stdexcept>
#include <roaring/roaring.h>

class Roaring {
public:
	/**
	 * Create an empty bitmap
	 */
	Roaring () : roaring(NULL) {
		roaring = roaring_bitmap_create();
		if(roaring == NULL) {
			throw std::runtime_error("failed memory alloc in constructor");
		}
	}

	/**
	 * Copy constructor
	 */
	Roaring(const Roaring & r) : roaring(NULL) {
		roaring = roaring_bitmap_copy(r.roaring);
		if(roaring == NULL) {
			throw std::runtime_error("failed memory alloc in constructor");
		}
	}



	/**
	 * Construct a roaring object from the C struct.
	 *
	 * Passing a NULL point is unsafe.
	 */
	Roaring(roaring_bitmap_t * s) : roaring(s) {
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
	 * Construct a bitmap from a list of integer values.
	 */
	static Roaring fromUint32Array(size_t n, const uint32_t * data) {
		Roaring ans;
	    for (size_t i = 0; i < n; i++) {
	      ans.add(data[i]);
	    }
	    return ans;
	}

	/**
	 * Add value x
	 *
	 */
	void add(uint32_t x) {
		roaring_bitmap_add(roaring, x);
	}
	/**
	 * Remove value x
	 *
	 */
	void remove(uint32_t x) {
		roaring_bitmap_remove(roaring, x);
	}

	/**
	 * Check if value x is present
	 */
	bool contains(uint32_t x)  const {
		return roaring_bitmap_contains(roaring, x);
	}

	/**
	 * Destructor
	 */
	~Roaring() {
		roaring_bitmap_free(roaring);
	}

	/**
	 * Copies the content of the provided bitmap, and
	 * discard the current content.
	 */
	Roaring& operator=(const Roaring & r) {
		roaring_bitmap_free(roaring);
		roaring = roaring_bitmap_copy(r.roaring);
		if(roaring == NULL) {
			throw std::runtime_error("failed memory alloc in assignement");
		}
		return *this;
    }

	/**
	 * Compute the intersection between the current bitmap and the provided bitmap,
	 * writing the result in the current bitmap. The provided bitmap is not modified.
	 */
	Roaring& operator&=(const Roaring & r) {
		roaring_bitmap_and_inplace(roaring,r.roaring);
		return *this;
	}


	/**
	 * Compute the union between the current bitmap and the provided bitmap,
	 * writing the result in the current bitmap. The provided bitmap is not modified.
	 *
	 * See also the fastunion function to aggregate many bitmaps more quickly.
	 */
	Roaring& operator|=(const Roaring & r) {
		roaring_bitmap_or_inplace(roaring,r.roaring);
		return *this;
	}


	/**
	 * Compute the symmetric union between the current bitmap and the provided bitmap,
	 * writing the result in the current bitmap. The provided bitmap is not modified.
	 */
	Roaring& operator^=(const Roaring & r) {
		roaring_bitmap_xor_inplace(roaring,r.roaring);
		return *this;
	}


	/**
	 * Exchange the content of this bitmap with another.
	 */
	void swap(Roaring & r) {
		std::swap(r.roaring, roaring);
	}

	/**
	 * Get the cardinality of the bitmap (number of elements).
	 */
	uint64_t cardinality()  const {
		return roaring_bitmap_get_cardinality(roaring);
	}

	/**
	* Returns true if the bitmap is empty (cardinality is zero).
	*/
	bool isEmpty()  const {
		return roaring_bitmap_is_empty(roaring);
	}

	/**
	 * Convert the bitmap to an array. Write the output to "ans",
	 * caller is responsible to ensure that there is enough memory
	 * allocated
	 * (e.g., ans = new uint32[mybitmap.cardinality()];)
	 */
	void toUint32Array(uint32_t *ans)  const {
		roaring_bitmap_to_uint32_array(roaring,ans);
	}

	/**
	 * Return true if the two bitmaps contain the same elements.
	 */
	bool operator==(const Roaring & r)  const {
		return roaring_bitmap_equals(roaring, r.roaring);
	}


	/**
	 * compute the negation of the roaring bitmap within a specified interval.
	 * areas outside the range are passed through unchanged.
	 */
	void flip(uint64_t range_start, uint64_t range_end) {
		roaring_bitmap_flip_inplace(roaring, range_start,range_end);
	}


	/**
	 *  Remove run-length encoding even when it is more space efficient
	 *  return whether a change was applied
	 */
	bool removeRunCompression() {
		return roaring_bitmap_remove_run_compression(roaring);
	}


	/** convert array and bitmap containers to run containers when it is more
	 * efficient;
	 * also convert from run containers when more space efficient.  Returns
	 * true if the result has at least one run container.
	 */
	bool runOptimize() {
		return roaring_bitmap_run_optimize(roaring);
	}

	/**
	 * Iterate over the bitmap elements. The function iterator is called once for
	 *  all the values with ptr (can be NULL) as the second parameter of each call.
	 *
	 *  roaring_iterator is simply a pointer to a function that returns void,
	 *  and takes (uint32_t,void*) as inputs.
	 */
	void iterate(roaring_iterator iterator, void *ptr) const {
		roaring_iterate(roaring, iterator, ptr);
	}

	/**
	 * If the size of the roaring bitmap is strictly greater than rank, then this
	   function returns true and set element to the element of given rank.
	   Otherwise, it returns false.
	 */
	bool select(uint32_t rank, uint32_t *element) const {
		return roaring_bitmap_select(roaring, rank,element);
	}

	/**
	 * write a bitmap to a char buffer. This is meant to be compatible with
	 * the
	 * Java and Go versions. Returns how many bytes were written which should be
	 * getSizeInBytes().
	 */
	size_t write(char *buf) const {
		return roaring_bitmap_portable_serialize(roaring,buf);
	}

	/**
	 * read a bitmap from a serialized version. This is meant to be compatible with
	 * the
	 * Java and Go versions.
	 */
	static Roaring read(const char *buf) {
		Roaring ans(NULL);
		ans.roaring = roaring_bitmap_portable_deserialize(buf);
		if(ans.roaring == NULL) {
			throw std::runtime_error("failed memory alloc while reading");
		}
		return ans;
	}

	/**
	 * How many bytes are required to serialize this bitmap (meant to be compatible
	 * with Java and Go versions)
	 */
	size_t getSizeInBytes() const {
		return roaring_bitmap_portable_size_in_bytes(roaring);
	}


	/**
	 * Computes the intersection between two bitmaps and returns new bitmap.
	 * The current bitmap and the provided bitmap are unchanged.
	 */
	Roaring operator&(const Roaring & o) const {
		roaring_bitmap_t * r = roaring_bitmap_and(roaring,
                o.roaring);
		if(r == NULL) {
			throw std::runtime_error("failed materalization in and");
		}
		return Roaring(r);
	}

	/**
	 * Computes the union between two bitmaps and returns new bitmap.
	 * The current bitmap and the provided bitmap are unchanged.
	 */
	Roaring operator|(const Roaring & o) const {
		roaring_bitmap_t * r = roaring_bitmap_or(roaring,
                o.roaring);
		if(r == NULL) {
			throw std::runtime_error("failed materalization in or");
		}
		return Roaring(r);
	}

	/**
	 * Computes the symmetric union between two bitmaps and returns new bitmap.
	 * The current bitmap and the provided bitmap are unchanged.
	 */
	Roaring operator^(const Roaring & o) const {
		roaring_bitmap_t * r = roaring_bitmap_xor(roaring,
                o.roaring);
		if(r == NULL) {
			throw std::runtime_error("failed materalization in xor");
		}
		return Roaring(r);
	}


	/**
	 * Whether or not we apply copy and write.
	 */
	void setCopyOnWrite(bool val) {
		roaring->copy_on_write = val;
	}


	/**
	 * Print the content of the bitmap
	 */
	void printf() {
	    roaring_bitmap_printf(roaring);
	}


	/**
	 * Whether or not copy and write is active.
	 */
	bool getCopyOnWrite() const {
		return roaring->copy_on_write;
	}

	/**
	 * computes the logical or (union) between "n" bitmaps (referenced by a
	 * pointer).
	 */
	static Roaring fastunion(size_t n, const Roaring **inputs) {
		const roaring_bitmap_t **x = (const roaring_bitmap_t **) malloc(n
				* sizeof(roaring_bitmap_t *));
		if(x == NULL) {
			throw std::runtime_error("failed memory alloc in fastunion");
		}
		for(size_t k = 0 ; k < n; ++k)
			x[k] = inputs[k]->roaring;

		Roaring ans(NULL);
		ans.roaring = roaring_bitmap_or_many(n,x);
		if(ans.roaring == NULL) {
			throw std::runtime_error("failed memory alloc in fastunion");
		}
	    free(x);
		return ans;
	}


	roaring_bitmap_t * roaring;
};




#endif /* INCLUDE_ROARING_HH_ */



