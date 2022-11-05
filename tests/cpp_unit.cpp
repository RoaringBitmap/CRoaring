/**
 * The purpose of this test is to check that we can call CRoaring from C++
 */

#include <assert.h>
#include <iostream>
#include <roaring/misc/configreport.h>
#include <roaring/roaring.h>  // access to pure C exported API for testing
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <random>
#include <vector>


#include <fstream>
#include <iostream>
#include <type_traits>
#include <vector>

#include "config.h"
#include "roaring.hh"
using roaring::Roaring;  // the C++ wrapper class

#include "roaring64map.hh"
using roaring::Roaring64Map;  // C++ class extended for 64-bit numbers

#include "roaring64map_checked.hh"

#include "test.h"

static_assert(std::is_nothrow_move_constructible<Roaring>::value,
              "Expected Roaring to be no except move constructable");

bool roaring_iterator_sumall(uint32_t value, void *param) {
    *(uint32_t *)param += value;
    return true;  // we always process all values
}

bool roaring_iterator_sumall64(uint64_t value, void *param) {
    *(uint64_t *)param += value;
    return true;  // we always process all values
}

DEFINE_TEST(serial_test) {
    uint32_t values[] = {5, 2, 3, 4, 1};
    Roaring r1(sizeof(values) / sizeof(uint32_t), values);
    uint32_t serializesize = r1.getSizeInBytes();
    char *serializedbytes = new char[serializesize];
    r1.write(serializedbytes);
    Roaring t = Roaring::read(serializedbytes);
    assert_true(r1 == t);
    char *copy = new char[serializesize];
    memcpy(copy, serializedbytes, serializesize);
    Roaring t2 = Roaring::read(copy);
    assert_true(t2 == t);
    delete[] serializedbytes;
    delete[] copy;
}

void test_example(bool copy_on_write) {
    // create a new empty bitmap
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    roaring_bitmap_set_copy_on_write(r1, copy_on_write);
    assert_ptr_not_equal(r1, NULL);

    // then we can add values
    for (uint32_t i = 100; i < 1000; i++) {
        roaring_bitmap_add(r1, i);
    }
    // check whether a value is contained
    assert_true(roaring_bitmap_contains(r1, 500));

    // compute how many bits there are:
    uint64_t cardinality = roaring_bitmap_get_cardinality(r1);
    printf("Cardinality = %d \n", (int)cardinality);
    assert_int_equal(900, cardinality);

    // if your bitmaps have long runs, you can compress them by calling
    // run_optimize
    size_t size = roaring_bitmap_portable_size_in_bytes(r1);
    roaring_bitmap_run_optimize(r1);
    size_t compact_size = roaring_bitmap_portable_size_in_bytes(r1);
    printf("size before run optimize %zu bytes, and after %zu bytes\n", size,
           compact_size);
    // create a new bitmap with varargs
    roaring_bitmap_t *r2 = roaring_bitmap_of(5, 1, 2, 3, 5, 6);
    assert_ptr_not_equal(r2, NULL);
    roaring_bitmap_printf(r2);
    printf("\n");
    // we can also create a bitmap from a pointer to 32-bit integers
    const uint32_t values[] = {2, 3, 4};
    roaring_bitmap_t *r3 = roaring_bitmap_of_ptr(3, values);
    roaring_bitmap_set_copy_on_write(r3, copy_on_write);
    // we can also go in reverse and go from arrays to bitmaps
    uint64_t card1 = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr1 = new uint32_t[card1];
    assert_ptr_not_equal(arr1, NULL);
    roaring_bitmap_to_uint32_array(r1, arr1);

    roaring_bitmap_t *r1f = roaring_bitmap_of_ptr(card1, arr1);
    delete[] arr1;
    assert_ptr_not_equal(r1f, NULL);

    // bitmaps shall be equal
    assert_true(roaring_bitmap_equals(r1, r1f));
    roaring_bitmap_free(r1f);

    // we can copy and compare bitmaps
    roaring_bitmap_t *z = roaring_bitmap_copy(r3);
    roaring_bitmap_set_copy_on_write(z, copy_on_write);
    assert_true(roaring_bitmap_equals(r3, z));

    roaring_bitmap_free(z);

    // we can compute union two-by-two
    roaring_bitmap_t *r1_2_3 = roaring_bitmap_or(r1, r2);
    roaring_bitmap_set_copy_on_write(r1_2_3, copy_on_write);
    roaring_bitmap_or_inplace(r1_2_3, r3);

    // we can compute a big union
    const roaring_bitmap_t *allmybitmaps[] = {r1, r2, r3};
    roaring_bitmap_t *bigunion = roaring_bitmap_or_many(3, allmybitmaps);
    assert_true(roaring_bitmap_equals(r1_2_3, bigunion));
    roaring_bitmap_t *bigunionheap =
        roaring_bitmap_or_many_heap(3, allmybitmaps);
    assert_true(roaring_bitmap_equals(r1_2_3, bigunionheap));
    roaring_bitmap_free(r1_2_3);
    roaring_bitmap_free(bigunion);
    roaring_bitmap_free(bigunionheap);

    // we can compute intersection two-by-two
    roaring_bitmap_t *i1_2 = roaring_bitmap_and(r1, r2);
    roaring_bitmap_free(i1_2);

    // we can write a bitmap to a pointer and recover it later
    size_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    char *serializedbytes = (char *)malloc(expectedsize);
    roaring_bitmap_portable_serialize(r1, serializedbytes);
    roaring_bitmap_t *t = roaring_bitmap_portable_deserialize(serializedbytes);
    assert_true(expectedsize == roaring_bitmap_portable_size_in_bytes(t));
    assert_true(roaring_bitmap_equals(r1, t));
    roaring_bitmap_free(t);
    free(serializedbytes);

    // we can iterate over all values using custom functions
    uint32_t counter = 0;
    roaring_iterate(r1, roaring_iterator_sumall, &counter);
    /**
     * void roaring_iterator_sumall(uint32_t value, void *param) {
     *        *(uint32_t *) param += value;
     *  }
     *
     */

    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r3);
}

void test_issue304(void) {
    Roaring64Map roaring;
    assert_false(roaring.isFull());
}

DEFINE_TEST(test_issue304) { test_issue304(); }

DEFINE_TEST(issue316) {
    Roaring r1;
    r1.setCopyOnWrite(true);
    r1.addRange(1, 100);
    Roaring r2;
    r2 |= r1;
    assert_true(r2.isSubset(r1));
    assert_true(r1.isSubset(r2));
    assert_true(r1 == r2);

    Roaring r3 = r2;
    assert_true(r3.isSubset(r1));
    assert_true(r1.isSubset(r3));
    assert_true(r1 == r3);
    assert_true(r1 == r2);
}

DEFINE_TEST(issue_336) {
    Roaring64Map r1, r2;

    r1.add((uint64_t)0x000000000UL);
    r1.add((uint64_t)0x100000000UL);
    r1.add((uint64_t)0x200000000UL);
    r1.add((uint64_t)0x300000000UL);

    r1.remove((uint64_t)0x100000000UL);
    r1.remove((uint64_t)0x200000000UL);

    r2.add((uint64_t)0x000000000UL);
    r2.add((uint64_t)0x300000000UL);

    assert_true(r1 == r2);
    assert_true(r2 == r1);
}

DEFINE_TEST(issue_372) {
    Roaring64Map roaring;
    // Flip multiple buckets
    uint64_t upper_bound = ((uint64_t)1 << 32) * 3;
    roaring.flip(0, upper_bound);
    assert_int_equal(roaring.cardinality(), upper_bound);
    roaring.flip(1, upper_bound - 1);
    assert_int_equal(roaring.cardinality(), 2);
}

void test_roaring64_iterate_multi_roaring(void) {
    Roaring64Map roaring;

    assert_true(roaring.addChecked(uint64_t(1)));
    assert_true(roaring.addChecked(uint64_t(2)));
    assert_true(roaring.addChecked(uint64_t(1) << 32));
    assert_true(roaring.addChecked(uint64_t(2) << 32));

    uint64_t iterate_count = 0;
    auto iterate_func = [](uint64_t, void *param) -> bool {
        auto *count = static_cast<uint64_t *>(param);
        *count += 1;
        return *count < 2;
    };
    roaring.iterate(iterate_func, &iterate_count);
    assert_true(iterate_count == 2);
}

DEFINE_TEST(test_roaring64_iterate_multi_roaring) {
    test_roaring64_iterate_multi_roaring();
}

void test_example_cpp(bool copy_on_write) {
    // create a new empty bitmap
    Roaring r1;
    r1.setCopyOnWrite(copy_on_write);
    // then we can add values
    for (uint32_t i = 100; i < 1000; i++) {
        r1.add(i);
    }

    // check whether a value is contained
    assert_true(r1.contains(500));

    // compute how many bits there are:
    uint64_t cardinality = r1.cardinality();
    std::cout << "Cardinality = " << cardinality << std::endl;

    // if your bitmaps have long runs, you can compress them by calling
    // run_optimize
    size_t size = r1.getSizeInBytes();
    r1.runOptimize();
    size_t compact_size = r1.getSizeInBytes();

    std::cout << "size before run optimize " << size << " bytes, and after "
              << compact_size << " bytes." << std::endl;

    // create a new bitmap with varargs
    Roaring r2 = Roaring::bitmapOf(5, 1, 2, 3, 5, 6);

    r2.printf();
    printf("\n");

    // test select
    uint32_t element;
    r2.select(3, &element);
    assert_true(element == 5);

    assert_true(r2.minimum() == 1);

    assert_true(r2.maximum() == 6);

    assert_true(r2.rank(4) == 3);

    // we can also create a bitmap from a pointer to 32-bit integers
    const uint32_t values[] = {2, 3, 4};
    Roaring r3(3, values);
    r3.setCopyOnWrite(copy_on_write);

    // we can also go in reverse and go from arrays to bitmaps
    uint64_t card1 = r1.cardinality();
    uint32_t *arr1 = new uint32_t[card1];
    assert_true(arr1 != NULL);
    r1.toUint32Array(arr1);
    Roaring r1f(card1, arr1);
    delete[] arr1;

    // bitmaps shall be equal
    assert_true(r1 == r1f);

    // we can copy and compare bitmaps
    Roaring z(r3);
    z.setCopyOnWrite(copy_on_write);
    assert_true(r3 == z);

    // we can compute union two-by-two
    Roaring r1_2_3 = r1 | r2;
    r1_2_3.setCopyOnWrite(copy_on_write);
    r1_2_3 |= r3;

    // we can compute a big union
    const Roaring *allmybitmaps[] = {&r1, &r2, &r3};
    Roaring bigunion = Roaring::fastunion(3, allmybitmaps);
    assert_true(r1_2_3 == bigunion);

    // we can compute intersection two-by-two
    Roaring i1_2 = r1 & r2;

    // we can write a bitmap to a pointer and recover it later
    size_t expectedsize = r1.getSizeInBytes();
    char *serializedbytes = new char[expectedsize];
    r1.write(serializedbytes);
    Roaring t = Roaring::read(serializedbytes);
    assert_true(expectedsize == t.getSizeInBytes());
    assert_true(r1 == t);

    Roaring t2 = Roaring::readSafe(serializedbytes, expectedsize);
    assert_true(expectedsize == t2.getSizeInBytes());
    assert_true(r1 == t2);

    delete[] serializedbytes;

    // we can iterate over all values using custom functions
    uint32_t counter = 0;
    r1.iterate(roaring_iterator_sumall, &counter);
    /**
     * void roaring_iterator_sumall(uint32_t value, void *param) {
     *        *(uint32_t *) param += value;
     *  }
     *
     */
    // we can also iterate the C++ way
    counter = 0;
    for (Roaring::const_iterator i = t.begin(); i != t.end(); i++) {
        ++counter;
    }
    assert_true(counter == t.cardinality());

    // we can move iterators
    const uint32_t manyvalues[] = {2, 3, 4, 7, 8};
    Roaring rogue(5, manyvalues);
    Roaring::const_iterator j = rogue.begin();
    j.equalorlarger(4);
    assert_true(*j == 4);

    // test move constructor
    {
        Roaring b;
        b.add(10);
        b.add(20);

        Roaring a(std::move(b));
        assert_true(a.cardinality() == 2);
        assert_true(a.contains(10));
        assert_true(a.contains(20));

        // b should be destroyed without any errors
        assert_true(b.cardinality() == 0);
    }

    // test move operator
    {
        Roaring b;
        b.add(10);
        b.add(20);

        Roaring a;

        a = std::move(b);
        assert_int_equal(2, a.cardinality());
        assert_true(a.contains(10));
        assert_true(a.contains(20));

        // b should be destroyed without any errors
        assert_int_equal(0, b.cardinality());
    }

    // test toString
    {
        Roaring a;
        a.add(1);
        a.add(2);
        a.add(3);
        a.add(4);

        assert_string_equal("{1,2,3,4}", a.toString().c_str());
    }
}

void test_run_compression_cpp(bool copy_on_write) {
    Roaring r1;
    r1.setCopyOnWrite(copy_on_write);
    for (uint32_t i = 100; i <= 10000; i++) {
        r1.add(i);
    }
    uint64_t size_origin = r1.getSizeInBytes();
    bool has_run = r1.runOptimize();
    uint64_t size_optimized = r1.getSizeInBytes();
    assert_true(has_run);
    assert_true(size_origin > size_optimized);
    bool removed = r1.removeRunCompression();
    assert_true(removed);
    uint64_t size_removed = r1.getSizeInBytes();
    assert_true(size_removed > size_optimized);
    return;
}

void test_run_compression_cpp_64(bool copy_on_write) {
    Roaring64Map r1;
    r1.setCopyOnWrite(copy_on_write);
    for (uint64_t i = 100; i <= 10000; i++) {
        r1.add(i);
    }
    uint64_t size_origin = r1.getSizeInBytes();
    bool has_run = r1.runOptimize();
    uint64_t size_optimized = r1.getSizeInBytes();
    assert_true(has_run);
    assert_true(size_origin > size_optimized);
    bool removed = r1.removeRunCompression();
    assert_true(removed);
    uint64_t size_removed = r1.getSizeInBytes();
    assert_true(size_removed > size_optimized);
    return;
}

void test_example_cpp_64(bool copy_on_write) {
    // create a new empty bitmap
    Roaring64Map r1;
    r1.setCopyOnWrite(copy_on_write);
    // then we can add values
    for (uint64_t i = 100; i < 1000; i++) {
        r1.add(i);
    }
    for (uint64_t i = 14000000000000000100ull; i < 14000000000000001000ull;
         i++) {
        r1.add(i);
    }

    // check whether a value is contained
    assert_true(r1.contains((uint64_t)14000000000000000500ull));

    // compute how many bits there are:
    uint64_t cardinality = r1.cardinality();
    std::cout << "Cardinality = " << cardinality << std::endl;

    // if your bitmaps have long runs, you can compress them by calling
    // run_optimize
    uint64_t size = r1.getSizeInBytes();
    r1.runOptimize();
    uint64_t compact_size = r1.getSizeInBytes();

    std::cout << "size before run optimize " << size << " bytes, and after "
              << compact_size << " bytes." << std::endl;

    // create a new bitmap with varargs
    Roaring64Map r2 =
        Roaring64Map::bitmapOf(5, 1ull, 2ull, 234294967296ull, 195839473298ull,
                               14000000000000000100ull);

    r2.printf();
    printf("\n");

    // test select
    uint64_t element;
    r2.select(4, &element);
    assert_true(element == 14000000000000000100ull);

    assert_true(r2.minimum() == 1ull);

    assert_true(r2.maximum() == 14000000000000000100ull);

    assert_true(r2.rank(234294967296ull) == 4ull);

    // we can also create a bitmap from a pointer to 32-bit integers
    const uint32_t values[] = {2, 3, 4};
    Roaring64Map r3(3, values);
    r3.setCopyOnWrite(copy_on_write);

    // we can also go in reverse and go from arrays to bitmaps
    uint64_t card1 = r1.cardinality();
    uint64_t *arr1 = new uint64_t[card1];
    assert_true(arr1 != NULL);
    r1.toUint64Array(arr1);
    Roaring64Map r1f(card1, arr1);
    delete[] arr1;

    // bitmaps shall be equal
    assert_true(r1 == r1f);

    // we can copy and compare bitmaps
    Roaring64Map z(r3);
    z.setCopyOnWrite(copy_on_write);
    assert_true(r3 == z);

    // we can compute union two-by-two
    Roaring64Map r1_2_3 = r1 | r2;
    r1_2_3.setCopyOnWrite(copy_on_write);
    r1_2_3 |= r3;

    // we can compute a big union
    const Roaring64Map *allmybitmaps[] = {&r1, &r2, &r3};
    Roaring64Map bigunion = Roaring64Map::fastunion(3, allmybitmaps);
    assert_true(r1_2_3 == bigunion);

    // we can compute intersection two-by-two
    Roaring64Map i1_2 = r1 & r2;

    // we can write a bitmap to a pointer and recover it later
    size_t expectedsize = r1.getSizeInBytes();
    char *serializedbytes = new char[expectedsize];
    r1.write(serializedbytes);
    Roaring64Map t = Roaring64Map::read(serializedbytes);
    assert_true(expectedsize == t.getSizeInBytes());
    assert_true(r1 == t);
    delete[] serializedbytes;

    // we can iterate over all values using custom functions
    uint64_t counter = 0;
    r1.iterate(roaring_iterator_sumall64, &counter);
    /**
     * void roaring_iterator_sumall64(uint64_t value, void *param) {
     *        *(uint64_t *) param += value;
     *  }
     *
     */
    // we can also iterate the C++ way
    counter = 0;
    for (Roaring64Map::const_iterator i = t.begin(); i != t.end(); i++) {
        ++counter;
    }
    assert_true(counter == t.cardinality());

    {
        Roaring64Map b;
        b.add(1u);
        b.add(2u);
        b.add(3u);
        assert_int_equal(3, b.cardinality());

        Roaring64Map a(std::move(b));
        assert_int_equal(3, a.cardinality());
        // assert_int_equal(0, b.cardinality()); // no: b is now unspecified.
    }

    {
        Roaring64Map a, b;
        b.add(1u);
        b.add(2u);
        b.add(3u);
        assert_int_equal(3, b.cardinality());

        a = std::move(b);
        assert_int_equal(3, a.cardinality());
        // assert_int_equal(0, b.cardinality()); // no: b is unspecified
    }
}

DEFINE_TEST(test_example_true) { test_example(true); }

DEFINE_TEST(test_example_false) { test_example(false); }

DEFINE_TEST(test_example_cpp_true) { test_example_cpp(true); }

DEFINE_TEST(test_example_cpp_false) { test_example_cpp(false); }

DEFINE_TEST(test_example_cpp_64_true) { test_example_cpp_64(true); }

DEFINE_TEST(test_example_cpp_64_false) { test_example_cpp_64(false); }

DEFINE_TEST(test_run_compression_cpp_64_true) {
    test_run_compression_cpp_64(true);
}

DEFINE_TEST(test_run_compression_cpp_64_false) {
    test_run_compression_cpp_64(false);
}

DEFINE_TEST(test_run_compression_cpp_true) { test_run_compression_cpp(true); }

DEFINE_TEST(test_run_compression_cpp_false) { test_run_compression_cpp(false); }

DEFINE_TEST(test_cpp_add_remove_checked) {
    Roaring roaring;
    uint32_t values[4] = {123, 9999, 0xFFFFFFF7, 0xFFFFFFFF};
    for (int i = 0; i < 4; ++i) {
        assert_true(roaring.addChecked(values[i]));
        assert_false(roaring.addChecked(values[i]));
    }
    for (int i = 0; i < 4; ++i) {
        assert_true(roaring.removeChecked(values[i]));
        assert_false(roaring.removeChecked(values[i]));
    }
    assert_true(roaring.isEmpty());
}

DEFINE_TEST(test_cpp_add_remove_checked_64) {
    Roaring64Map roaring;

    uint32_t values32[4] = {123, 9999, 0xFFFFFFF7, 0xFFFFFFFF};
    for (int i = 0; i < 4; ++i) {
        assert_true(roaring.addChecked(values32[i]));
        assert_false(roaring.addChecked(values32[i]));
    }
    for (int i = 0; i < 4; ++i) {
        assert_true(roaring.removeChecked(values32[i]));
        assert_false(roaring.removeChecked(values32[i]));
    }

    uint64_t values64[4] = {123ULL, 0xA00000000AULL, 0xAFFFFFFF7ULL,
                            0xFFFFFFFFFULL};
    for (int i = 0; i < 4; ++i) {
        assert_true(roaring.addChecked(values64[i]));
        assert_false(roaring.addChecked(values64[i]));
    }
    for (int i = 0; i < 4; ++i) {
        assert_true(roaring.removeChecked(values64[i]));
        assert_false(roaring.removeChecked(values64[i]));
    }
    assert_true(roaring.isEmpty());
}

DEFINE_TEST(test_cpp_add_range) {
    std::vector<std::pair<uint64_t, uint64_t>> ranges = {
      {1, 5},
      {1, 1},
      {2, 1},
    };
    for (const auto &range : ranges) {
        uint64_t min = range.first;
        uint64_t max = range.second;
        Roaring r1;
        r1.addRangeClosed(min, max);
        Roaring r2;
        for (uint64_t v = min; v <= max; ++v) {
            r2.add(v);
        }
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_cpp_remove_range) {
    {
        // min < r1.minimum, max > r1.maximum
        Roaring r1 = Roaring::bitmapOf(3, 1, 2, 4);
        r1.removeRangeClosed(0, 5);
        assert_true(r1.isEmpty());
    }
    {
        // min < r1.minimum, max < r1.maximum, max does not exactly match an
        // element
        Roaring r1 = Roaring::bitmapOf(3, 1, 2, 4);
        r1.removeRangeClosed(0, 3);
        Roaring r2 = Roaring::bitmapOf(1, 4);
        assert_true(r1 == r2);
    }
    {
        // min < r1.minimum, max < r1.maximum, max exactly matches an element
        Roaring r1 = Roaring::bitmapOf(3, 1, 2, 4);
        r1.removeRangeClosed(0, 2);
        Roaring r2 = Roaring::bitmapOf(1, 4);
        assert_true(r1 == r2);
    }
    {
        // min > r1.minimum, max > r1.maximum, min does not exactly match an
        // element
        Roaring r1 = Roaring::bitmapOf(3, 1, 2, 4);
        r1.removeRangeClosed(3, 5);
        Roaring r2 = Roaring::bitmapOf(2, 1, 2);
        assert_true(r1 == r2);
    }
    {
        // min > r1.minimum, max > r1.maximum, min exactly matches an element
        Roaring r1 = Roaring::bitmapOf(3, 1, 2, 4);
        r1.removeRangeClosed(2, 5);
        Roaring r2 = Roaring::bitmapOf(1, 1);
        assert_true(r1 == r2);
    }
    {
        // min > r1.minimum, max < r1.maximum, no elements between min and max
        Roaring r1 = Roaring::bitmapOf(3, 1, 2, 4);
        r1.removeRangeClosed(3, 3);
        Roaring r2 = Roaring::bitmapOf(3, 1, 2, 4);
        assert_true(r1 == r2);
    }
    {
        // max < r1.minimum
        Roaring r1 = Roaring::bitmapOf(3, 1, 2, 4);
        r1.removeRangeClosed(0, 0);
        Roaring r2 = Roaring::bitmapOf(3, 1, 2, 4);
        assert_true(r1 == r2);
    }
    {
        // min > r1.maximum
        Roaring r1 = Roaring::bitmapOf(3, 1, 2, 4);
        r1.removeRangeClosed(5, 6);
        Roaring r2 = Roaring::bitmapOf(3, 1, 2, 4);
        assert_true(r1 == r2);
    }
    {
        // min > max
        Roaring r1 = Roaring::bitmapOf(3, 1, 2, 4);
        r1.removeRangeClosed(2, 1);
        Roaring r2 = Roaring::bitmapOf(3, 1, 2, 4);
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_cpp_add_range_64) {
    {
        // 32-bit integers
        Roaring64Map r1;
        r1.addRangeClosed(uint32_t(1), uint32_t(5));
        Roaring64Map r2;
        for (uint32_t v = 1; v <= 5; ++v) {
            r2.add(v);
        }
        assert_true(r1 == r2);
    }
    std::vector<std::pair<uint64_t, uint64_t>> ranges = {
        {uint64_t(1) << 32, (uint64_t(1) << 32) + 10},
        {(uint64_t(1) << 32) - 10, (uint64_t(1) << 32) + 10},
        {(uint64_t(1) << 32) + 2, (uint64_t(1) << 32) - 2}};
    for (const auto &range : ranges) {
        uint64_t min = range.first;
        uint64_t max = range.second;
        Roaring64Map r1;
        r1.addRangeClosed(min, max);
        Roaring64Map r2;
        for (uint64_t v = min; v <= max; ++v) {
            r2.add(v);
        }
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_cpp_remove_range_64) {
    {
        // 32-bit integers
        Roaring64Map r1 =
            Roaring64Map::bitmapOf(3, uint64_t(1), uint64_t(2), uint64_t(4));
        r1.removeRangeClosed(uint32_t(2), uint32_t(3));
        Roaring64Map r2 = Roaring64Map::bitmapOf(2, uint64_t(1), uint64_t(4));
        assert_true(r1 == r2);
    }
    {
        // min < r1.minimum, max > r1.maximum
        Roaring64Map r1 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        r1.removeRangeClosed(uint64_t(0), uint64_t(5) << 32);
        assert_true(r1.isEmpty());
    }
    {
        // min < r1.minimum, max < r1.maximum, max does not exactly match an
        // element
        Roaring64Map r1 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        r1.removeRangeClosed(uint64_t(0), uint64_t(3) << 32);
        Roaring64Map r2 = Roaring64Map::bitmapOf(1, uint64_t(4) << 32);
        assert_true(r1 == r2);
    }
    {
        // min < r1.minimum, max < r1.maximum, max exactly matches the high bits
        // of an element
        Roaring64Map r1 =
            Roaring64Map::bitmapOf(4, uint64_t(1) << 32, uint64_t(2) << 32,
                                   (uint64_t(2) << 32) + 1, uint64_t(4) << 32);
        r1.removeRangeClosed(uint64_t(0), uint64_t(2) << 32);
        Roaring64Map r2 = Roaring64Map::bitmapOf(2, (uint64_t(2) << 32) + 1,
                                                 uint64_t(4) << 32);
        assert_true(r1 == r2);
    }
    {
        // min > r1.minimum, max > r1.maximum, min does not exactly match an
        // element
        Roaring64Map r1 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        r1.removeRangeClosed(uint64_t(3) << 32, uint64_t(5) << 32);
        Roaring64Map r2 =
            Roaring64Map::bitmapOf(2, uint64_t(1) << 32, uint64_t(2) << 32);
        assert_true(r1 == r2);
    }
    {
        // min > r1.minimum, max > r1.maximum, min exactly matches the high bits
        // of an element
        Roaring64Map r1 =
            Roaring64Map::bitmapOf(4, uint64_t(1) << 32, uint64_t(2) << 32,
                                   (uint64_t(2) << 32) + 1, uint64_t(4) << 32);
        r1.removeRangeClosed((uint64_t(2) << 32) + 1, uint64_t(5) << 32);
        Roaring64Map r2 =
            Roaring64Map::bitmapOf(2, uint64_t(1) << 32, uint64_t(2) << 32);
        assert_true(r1 == r2);
    }
    {
        // min > r1.minimum, max < r1.maximum, no elements between min and max
        Roaring64Map r1 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        r1.removeRangeClosed(uint64_t(3) << 32, (uint64_t(3) << 32) + 1);
        Roaring64Map r2 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        assert_true(r1 == r2);
    }
    {
        // max < r1.minimum
        Roaring64Map r1 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        r1.removeRangeClosed(uint64_t(1), uint64_t(2));
        Roaring64Map r2 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        assert_true(r1 == r2);
    }
    {
        // min > r1.maximum
        Roaring64Map r1 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        r1.removeRangeClosed(uint64_t(5) << 32, uint64_t(6) << 32);
        Roaring64Map r2 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        assert_true(r1 == r2);
    }
    {
        // min > max
        Roaring64Map r1 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        r1.removeRangeClosed(uint64_t(2) << 32, uint64_t(1) << 32);
        Roaring64Map r2 = Roaring64Map::bitmapOf(
            3, uint64_t(1) << 32, uint64_t(2) << 32, uint64_t(4) << 32);
        assert_true(r1 == r2);
    }
}

std::pair<doublechecked::Roaring64Map, doublechecked::Roaring64Map>
    make_two_big_roaring64_maps() {
    // Insert a large number of pseudorandom numbers into two sets.
    const uint32_t randomSeed = 0xdeadbeef;
    const size_t numValues = 1000000;  // 1 million

    doublechecked::Roaring64Map roaring1;
    doublechecked::Roaring64Map roaring2;

    std::default_random_engine engine(randomSeed);
    std::uniform_int_distribution<uint64_t> rng;

    for (size_t i = 0; i < numValues; ++i) {
        auto value = rng(engine);
        auto choice = rng(engine) % 4;
        switch (choice) {
            case 0: {
                // Value is added only to set 1.
                roaring1.add(value);
                break;
            }

            case 1: {
                // Value is added only to set 2.
                roaring2.add(value);
                break;
            }

            case 2: {
                // Value is added to both sets.
                roaring1.add(value);
                roaring2.add(value);
                break;
            }

            case 3: {
                // Value is added to set 1, and a slightly different value
                // is added to set 2. This makes it likely that they are in
                // the same "outer" bin, but at a different "inner" position.
                roaring1.add(value);
                roaring2.add(value + 1);
                break;
            }

            default:
                assert_true(false);
        }
    }
    return std::make_pair(std::move(roaring1), std::move(roaring2));
}

DEFINE_TEST(test_cpp_union_64) {
    auto two_maps = make_two_big_roaring64_maps();

    auto &lhs = two_maps.first;
    const auto &rhs = two_maps.second;

    lhs |= rhs;
    assert_true(lhs.does_std_set_match_roaring());
}

DEFINE_TEST(test_cpp_intersect_64) {
    auto two_maps = make_two_big_roaring64_maps();

    auto &lhs = two_maps.first;
    const auto &rhs = two_maps.second;

    lhs &= rhs;
    assert_true(lhs.does_std_set_match_roaring());
}

DEFINE_TEST(test_cpp_difference_64) {
    auto two_maps = make_two_big_roaring64_maps();

    auto &lhs = two_maps.first;
    const auto &rhs = two_maps.second;

    lhs -= rhs;
    assert_true(lhs.does_std_set_match_roaring());
}

DEFINE_TEST(test_cpp_xor_64) {
    auto two_maps = make_two_big_roaring64_maps();

    auto &lhs = two_maps.first;
    const auto &rhs = two_maps.second;

    lhs ^= rhs;
    assert_true(lhs.does_std_set_match_roaring());
}

DEFINE_TEST(test_cpp_clear_64) {
    Roaring64Map roaring;

    uint64_t values64[4] = {123ULL, 0xA00000000AULL, 0xAFFFFFFF7ULL,
                            0xFFFFFFFFFULL};
    for (int i = 0; i < 4; ++i) {
        assert_true(roaring.addChecked(values64[i]));
    }

    roaring.clear();

    assert_true(roaring.isEmpty());
}

DEFINE_TEST(test_cpp_move_64) {
    Roaring64Map roaring;

    uint64_t values64[4] = {123ULL, 0xA00000000AULL, 0xAFFFFFFF7ULL,
                            0xFFFFFFFFFULL};
    for (int i = 0; i < 4; ++i) {
        assert_true(roaring.addChecked(values64[i]));
    }

    Roaring64Map::const_iterator i(roaring);
    i.move(123ULL);
    assert_true(*i == 123ULL);
    i.move(0xAFFFFFFF8ULL);
    assert_true(*i == 0xFFFFFFFFFULL);
    assert_false(i.move(0xFFFFFFFFFFULL));
}

DEFINE_TEST(test_cpp_bidirectional_iterator_64) {
    Roaring64Map roaring;

    uint64_t values64[4] = {123ULL, 0xA00000000AULL, 0xAFFFFFFF7ULL,
                            0xFFFFFFFFFULL};
    for (int i = 0; i < 4; ++i) {
        assert_true(roaring.addChecked(values64[i]));
    }

    Roaring64Map::const_bidirectional_iterator i(roaring);
    i = roaring.begin();
    assert_true(*i++ == 123ULL);
    assert_true(*i++ == 0xAFFFFFFF7ULL);
    assert_true(*i++ == 0xFFFFFFFFFULL);
    assert_true(*i++ == 0xA00000000AULL);
    assert_true(i == roaring.end());
    assert_true(*--i == 0xA00000000AULL);
    assert_true(*--i == 0xFFFFFFFFFULL);
    assert_true(*--i == 0xAFFFFFFF7ULL);
    assert_true(*--i == 123ULL);
    assert_true(i == roaring.begin());
    i = roaring.end();
    i--;
    assert_true(*i-- == 0xA00000000AULL);
    assert_true(*i-- == 0xFFFFFFFFFULL);
    assert_true(*i-- == 0xAFFFFFFF7ULL);
    assert_true(*i == 123ULL);
    assert_true(i == roaring.begin());
}

DEFINE_TEST(test_cpp_frozen) {
    const uint64_t s = 65536;

    Roaring r1;
    r1.add(0);
    r1.add(UINT32_MAX);
    r1.add(1000);
    r1.add(2000);
    r1.add(100000);
    r1.add(200000);
    r1.addRange(s * 10 + 100, s * 13 - 100);
    for (uint64_t i = 0; i < s * 3; i += 2) {
        r1.add(s * 20 + i);
    }
    r1.runOptimize();

    // allocate a buffer and serialize to it
    size_t num_bytes = r1.getFrozenSizeInBytes();
    char *buf = (char *)roaring_aligned_malloc(32, num_bytes);
    r1.writeFrozen(buf);

    // ensure the frozen bitmap is the same as the original
    const Roaring r2 = Roaring::frozenView(buf, num_bytes);
    assert_true(r1 == r2);

    {
        Roaring r;
        r.addRange(0, 100000);
        r.flip(90000, 91000);
        r.runOptimize();

        // allocate a buffer and serialize to it
        size_t num_bytes1 = r.getFrozenSizeInBytes();
        char *buf1 = (char *)roaring_aligned_malloc(32, num_bytes1);
        r.writeFrozen(buf1);

        // ensure the frozen bitmap is the same as the original
        const Roaring rr = Roaring::frozenView(buf1, num_bytes1);
        assert_true(r == rr);
        roaring_aligned_free(buf1);
    }
#if ROARING_EXCEPTIONS
    // try viewing a misaligned/invalid buffer
    try {
        Roaring::frozenView(buf + 1, num_bytes - 1);
        assert(false);
    } catch (...) {
    }
#endif

    // copy constructor
    {
        Roaring tmp(r2);
        assert_true(tmp == r1);
    }

    // copy operator
    {
        Roaring tmp;
        tmp = r2;
        assert_true(tmp == r1);
    }

    // move constructor
    {
        Roaring a = Roaring::frozenView(buf, num_bytes);
        Roaring b(std::move(a));
        assert_true(b == r1);
    }

    // move assignment operator
    {
        Roaring a = Roaring::frozenView(buf, num_bytes);
        Roaring b;
        b = std::move(a);
        assert_true(b == r1);
    }

    roaring_aligned_free(buf);
}

DEFINE_TEST(test_cpp_frozen_64) {
    const uint64_t s = 65536;

    Roaring64Map r1;
    r1.add((uint64_t)0);
    r1.add((uint64_t)UINT32_MAX);
    r1.add((uint64_t)1000);
    r1.add((uint64_t)2000);
    r1.add((uint64_t)100000);
    r1.add((uint64_t)200000);
    r1.add((uint64_t)5);
    r1.add((uint64_t)1ull);
    r1.add((uint64_t)2ull);
    r1.add((uint64_t)234294967296ull);
    r1.add((uint64_t)195839473298ull);
    r1.add((uint64_t)14000000000000000100ull);
    for (uint64_t i = s * 10 + 100; i < s * 13 - 100; i++) {
        r1.add(i);
    }
    // r1.addRange(s * 10 + 100, s * 13 - 100);
    for (uint64_t i = 0; i < s * 3; i += 2) {
        r1.add(s * 20 + i);
    }
    r1.runOptimize();

    size_t num_bytes = r1.getFrozenSizeInBytes();
    char *buf = (char *)roaring_aligned_malloc(32, num_bytes);
    r1.writeFrozen(buf);

    const Roaring64Map r2 = Roaring64Map::frozenView(buf);
    assert_true(r1 == r2);

    // copy constructor
    {
        Roaring64Map tmp(r2);
        assert_true(tmp == r1);
    }

    // copy operator
    {
        Roaring64Map tmp;
        tmp = r2;
        assert_true(tmp == r1);
    }

    // move constructor
    {
        Roaring64Map a = Roaring64Map::frozenView(buf);
        Roaring64Map b(std::move(a));
        assert_true(b == r1);
    }

    // move assignment operator
    {
        Roaring64Map a = Roaring64Map::frozenView(buf);
        Roaring64Map b;
        b = std::move(a);
        assert_true(b == r1);
    }

    roaring_aligned_free(buf);
}

DEFINE_TEST(test_cpp_flip) {
    {
        // nothing is affected outside of the given range
        Roaring r1 = Roaring::bitmapOf(3, 1, 3, 6);
        r1.flip(2, 5);
        Roaring r2 = Roaring::bitmapOf(4, 1, 2, 4, 6);
        assert_true(r1 == r2);
    }
    {
        // given range can go outside of existing range
        Roaring r1 = Roaring::bitmapOf(2, 1, 3);
        r1.flip(0, 5);
        Roaring r2 = Roaring::bitmapOf(3, 0, 2, 4);
        assert_true(r1 == r2);
    }
    {
        // range end is exclusive
        Roaring r1 = Roaring::bitmapOf(2, 1, 3);
        r1.flip(1, 3);
        Roaring r2 = Roaring::bitmapOf(2, 2, 3);
        assert_true(r1 == r2);
    }
    {
        // uint32 max can be flipped
        Roaring r1 =
            Roaring::bitmapOf(1, (std::numeric_limits<uint32_t>::max)());
        r1.flip(
            (std::numeric_limits<uint32_t>::max)(),
            static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)()) + 1);
        assert_true(r1.isEmpty());
    }
    {
        // empty range does nothing
        Roaring r1 = Roaring::bitmapOf(2, 2, 3);
        Roaring r2 = r1;
        r1.flip(2, 2);
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_cpp_flip_64) {
    {
        // nothing is affected outside of the given range
        Roaring64Map r1 = Roaring64Map::bitmapOf(3, (((uint64_t)1) << 32) - 3, ((uint64_t)1) << 32,
                                                 (((uint64_t)1) << 32) + 3);
        r1.flip((((uint64_t)1) << 32) - 2, (((uint64_t)1) << 32) + 2);
        Roaring64Map r2 = Roaring64Map::bitmapOf(
            5, (((uint64_t)1) << 32) - 3, (((uint64_t)1) << 32) - 2, (((uint64_t)1) << 32) - 1,
            (((uint64_t)1) << 32) + 1, (((uint64_t)1) << 32) + 3);
        assert_true(r1 == r2);
    }
    {
        // given range can go outside of existing range
        Roaring64Map r1 = Roaring64Map::bitmapOf(2, (((uint64_t)1) << 32) - 2, ((uint64_t)1) << 32);
        r1.flip((((uint64_t)1) << 32) - 3, (((uint64_t)1) << 32) + 2);
        Roaring64Map r2 = Roaring64Map::bitmapOf(
            3, (((uint64_t)1) << 32) - 3, (((uint64_t)1) << 32) - 1, (((uint64_t)1) << 32) + 1);
        assert_true(r1 == r2);
    }
    {
        // range end is exclusive
        Roaring64Map r1 =
            Roaring64Map::bitmapOf(2, (((uint64_t)2) << 32) - 1, (((uint64_t)2) << 32) + 2);
        r1.flip((((uint64_t)2) << 32) - 1, (((uint64_t)2) << 32) + 2);
        Roaring64Map r2;
        for (uint64_t i = (((uint64_t)2) << 32); i <= (((uint64_t)2) << 32) + 2; ++i) {
            r2.add(i);
        }
        assert_true(r1 == r2);
    }
    {
        // uint32 max can be flipped
        Roaring64Map r1 =
            Roaring64Map::bitmapOf(1, static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)()));
        r1.flip(
            (std::numeric_limits<uint32_t>::max)(),
            static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)()) + 1);
        assert_true(r1.isEmpty());
    }
    {
        // empty range does nothing
        Roaring64Map r1 = Roaring64Map::bitmapOf(2, (((uint64_t)1) << 32) - 1, ((uint64_t)1) << 32);
        Roaring64Map r2 = r1;
        r1.flip((((uint64_t)1) << 32) - 1, (((uint64_t)1) << 32) - 1);
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_cpp_is_subset_64) {
  Roaring64Map r1 = Roaring64Map::bitmapOf(1, uint64_t(1));
  Roaring64Map r2 = Roaring64Map::bitmapOf(1, uint64_t(1) << 32);
  Roaring64Map r3 = r1 & r2;
  assert_true(r3.isSubset(r1));
  assert_true(r3.isSubset(r2));
}

DEFINE_TEST(test_cpp_remove_run_compression) {
  Roaring r;
  uint32_t max = (std::numeric_limits<uint32_t>::max)();
  for (uint32_t i = max - 10; i != 0; ++i) {
    r.add(i);
  }
  r.runOptimize();
  r.removeRunCompression();
}

// Returns true on success, false on exception.
bool test64Deserialize(const std::string& filename) {
    std::ifstream in(TEST_DATA_DIR + filename, std::ios::binary);
    std::vector<char> buf1(std::istreambuf_iterator<char>(in), {});
    printf("Reading %lu bytes\n", buf1.size());
    Roaring64Map roaring;
#if ROARING_EXCEPTIONS
    try {
        roaring = Roaring64Map::readSafe(buf1.data(), buf1.size());
    } catch (...) {
        return false;
    }
#else
    roaring = Roaring64Map::readSafe(buf1.data(), buf1.size());
#endif
    std::vector<char> buf2(roaring.getSizeInBytes());
    assert_true(buf1.size() == buf2.size());
    assert_true(roaring.write(buf2.data()) == buf2.size());
    for (size_t i = 0; i < buf1.size(); ++i) {
        assert_true(buf1[i] == buf2[i]);
    }
    return true;
}

// The valid files were created with cpp_unit_util.cpp.
DEFINE_TEST(test_cpp_deserialize_64_empty) {
  assert_true(test64Deserialize("64mapempty.bin"));
}

DEFINE_TEST(test_cpp_deserialize_64_32bit_vals) {
  assert_true(test64Deserialize("64map32bitvals.bin"));
}

DEFINE_TEST(test_cpp_deserialize_64_spread_vals) {
  assert_true(test64Deserialize("64mapspreadvals.bin"));
}

DEFINE_TEST(test_cpp_deserialize_64_high_vals) {
  assert_true(test64Deserialize("64maphighvals.bin"));
}

#if ROARING_EXCEPTIONS
DEFINE_TEST(test_cpp_deserialize_64_empty_input) {
  assert_false(test64Deserialize("64mapemptyinput.bin"));
}

DEFINE_TEST(test_cpp_deserialize_64_size_too_small) {
  assert_false(test64Deserialize("64mapsizetoosmall.bin"));
}

DEFINE_TEST(test_cpp_deserialize_64_invalid_size) {
  assert_false(test64Deserialize("64mapinvalidsize.bin"));
}

DEFINE_TEST(test_cpp_deserialize_64_key_too_small) {
  assert_false(test64Deserialize("64mapkeytoosmall.bin"));
}
#endif

int main() {
    roaring::misc::tellmeall();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(serial_test),
        cmocka_unit_test(test_example_true),
        cmocka_unit_test(test_example_false),
        cmocka_unit_test(test_example_cpp_true),
        cmocka_unit_test(test_example_cpp_false),
        cmocka_unit_test(test_example_cpp_64_true),
        cmocka_unit_test(test_example_cpp_64_false),
        cmocka_unit_test(test_cpp_add_remove_checked),
        cmocka_unit_test(test_cpp_add_remove_checked_64),
        cmocka_unit_test(test_cpp_add_range),
        cmocka_unit_test(test_cpp_remove_range),
        cmocka_unit_test(test_cpp_add_range_64),
        cmocka_unit_test(test_cpp_remove_range_64),
        cmocka_unit_test(test_run_compression_cpp_64_true),
        cmocka_unit_test(test_run_compression_cpp_64_false),
        cmocka_unit_test(test_run_compression_cpp_true),
        cmocka_unit_test(test_run_compression_cpp_false),
        cmocka_unit_test(test_cpp_union_64),
        cmocka_unit_test(test_cpp_intersect_64),
        cmocka_unit_test(test_cpp_difference_64),
        cmocka_unit_test(test_cpp_xor_64),
        cmocka_unit_test(test_cpp_clear_64),
        cmocka_unit_test(test_cpp_move_64),
        cmocka_unit_test(test_roaring64_iterate_multi_roaring),
        cmocka_unit_test(test_cpp_bidirectional_iterator_64),
        cmocka_unit_test(test_cpp_frozen),
        cmocka_unit_test(test_cpp_frozen_64),
        cmocka_unit_test(test_cpp_flip),
        cmocka_unit_test(test_cpp_flip_64),
        cmocka_unit_test(test_cpp_deserialize_64_empty),
        cmocka_unit_test(test_cpp_deserialize_64_32bit_vals),
        cmocka_unit_test(test_cpp_deserialize_64_spread_vals),
        cmocka_unit_test(test_cpp_deserialize_64_high_vals),
#if ROARING_EXCEPTIONS
        cmocka_unit_test(test_cpp_deserialize_64_empty_input),
        cmocka_unit_test(test_cpp_deserialize_64_size_too_small),
        cmocka_unit_test(test_cpp_deserialize_64_invalid_size),
        cmocka_unit_test(test_cpp_deserialize_64_key_too_small),
#endif
        cmocka_unit_test(issue316),
        cmocka_unit_test(test_issue304),
        cmocka_unit_test(issue_336),
        cmocka_unit_test(issue_372),
        cmocka_unit_test(test_cpp_is_subset_64),
        cmocka_unit_test(test_cpp_remove_run_compression),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
