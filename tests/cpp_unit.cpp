/**
 * The purpose of this test is to check that we can call CRoaring from C++
 */

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <type_traits>
#include <vector>

#include <roaring/misc/configreport.h>
#include <roaring/roaring.h>  // access to pure C exported API for testing

#include "config.h"
#include "roaring.hh"
using roaring::Roaring;  // the C++ wrapper class

#include "roaring64map.hh"
using roaring::Roaring64Map;  // C++ class extended for 64-bit numbers

#include "roaring64map_checked.hh"
#include "test.h"

static_assert(std::is_nothrow_move_constructible<Roaring>::value,
              "Expected Roaring to be no except move constructable");

namespace {
// We put std::numeric_limits<>::max in parentheses to avoid a
// clash with the Windows.h header under Windows.
const auto uint32_max = (std::numeric_limits<uint32_t>::max)();
const auto uint64_max = (std::numeric_limits<uint64_t>::max)();
}  // namespace

bool roaring_iterator_sumall(uint32_t value, void *param) {
    *(uint32_t *)param += value;
    return true;  // we always process all values
}

bool roaring_iterator_sumall64(uint64_t value, void *param) {
    *(uint64_t *)param += value;
    return true;  // we always process all values
}

DEFINE_TEST(fuzz_001) {
    roaring::Roaring b;
    b.addRange(173, 0);
    assert_true(b.cardinality() == 0);
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
    roaring_bitmap_t *r2 = roaring_bitmap_from(1, 2, 3, 5, 6);
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

namespace {
bool roaringEqual(const Roaring64Map &actual,
                  std::initializer_list<uint64_t> expected) {
    return expected.size() == actual.cardinality() &&
           std::equal(expected.begin(), expected.end(), actual.begin());
}
}  // namespace

DEFINE_TEST(test_roaring64_remove_32) {
    Roaring64Map roaring;

    // A specific test to make sure we don't get slots confused.
    // Specifically, we make Roaring64Map with only one slot (namely slot 5)
    // with values {100, 200, 300} in its inner bitmap. Then we do a 32-bit
    // remove of 100 from slot 0. A correct implementation of 'remove' would
    // be a no-op.
    const uint64_t b5 = uint64_t(5) << 32;
    Roaring64Map r;
    r.add(b5 + 100);
    r.add(b5 + 200);
    r.add(b5 + 300);
    r.remove(uint32_t(100));

    // No change
    assert_true(roaringEqual(r, {b5 + 100, b5 + 200, b5 + 300}));
}

DEFINE_TEST(test_roaring64_add_and_remove) {
    Roaring64Map r;

    const uint64_t b5 = uint64_t(5) << 32;

    // 32-bit adds
    r.add(300u);
    r.add(200u);
    r.add(100u);
    assert_true(roaringEqual(r, {100, 200, 300}));

    // 64-bit adds
    r.add(uint64_t(200));  // Duplicate
    r.add(uint64_t(400));  // New
    r.add(b5 + 400);       // All new
    r.add(b5 + 300);
    r.add(b5 + 200);
    r.add(b5 + 100);
    assert_true(roaringEqual(
        r, {100, 200, 300, 400, b5 + 100, b5 + 200, b5 + 300, b5 + 400}));

    // 32-bit removes
    r.remove(200u);  // Exists.
    r.remove(500u);  // Doesn't exist
    assert_true(roaringEqual(
        r, {100, 300, 400, b5 + 100, b5 + 200, b5 + 300, b5 + 400}));

    // 64-bit removes
    r.remove(b5 + 100);  // Exists.
    r.remove(b5 + 500);  // Doesn't exist
    assert_true(roaringEqual(r, {100, 300, 400, b5 + 200, b5 + 300, b5 + 400}));
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
    // create a new bitmap with initializer list
    Roaring r2i = Roaring::bitmapOfList({1, 2, 3, 5, 6});

    assert_true(r2i == r2);

    // create a new bitmap directly from initializer list
    Roaring r2id = {1, 2, 3, 5, 6};

    assert_true(r2id == r2);

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

        // Our move semantics allow moved-from objects to continue to be used
        // normally (they are reset to empty Roarings).
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

        // Our move semantics allow moved-from objects to continue to be used
        // normally (they are reset to empty Roarings).
        assert_int_equal(0, b.cardinality());
    }

    // test initializer lists
    {
        Roaring a;
        a.add(10);
        a.add(20);

        // construction
        Roaring b({10, 20});
        assert_true(a == b);

        a.add(30);
        // assignment
        b = {10, 20, 30};
        assert_true(a == b);
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
    // create a new bitmap with initializer list
    Roaring64Map r2i = Roaring64Map::bitmapOfList(
        {1, 2, 234294967296, 195839473298, 14000000000000000100ull});
    assert_true(r2i == r2);

    // create a new bitmap directly from initializer list
    Roaring64Map r2id = {1, 2, 234294967296, 195839473298,
                         14000000000000000100ull};
    assert_true(r2id == r2);

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

#if !CROARING_IS_BIG_ENDIAN
DEFINE_TEST(test_example_cpp_64_true) { test_example_cpp_64(true); }

DEFINE_TEST(test_example_cpp_64_false) { test_example_cpp_64(false); }
#endif

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

DEFINE_TEST(test_cpp_add_bulk) {
    std::vector<uint32_t> values = {9999, 123, 0xFFFFFFFF, 0xFFFFFFF7, 9999};
    Roaring r1;
    Roaring r2;
    roaring::BulkContext bulk_context;
    for (const auto value : values) {
        r1.addBulk(bulk_context, value);
        r2.add(value);
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_cpp_contains_bulk) {
    std::vector<uint32_t> values_exists = {9999, 123, 0xFFFFFFFF, 0xFFFFFFF7};
    std::vector<uint32_t> values_not_exists = {10,        12,         2000,
                                               0xFFFFFFF, 0xFFFFFFF9, 2048};
    Roaring r;
    r.addMany(values_exists.size(), values_exists.data());
    roaring::BulkContext bulk_context;
    for (const auto value : values_exists) {
        assert_true(r.containsBulk(bulk_context, value));
    }
    for (const auto value : values_not_exists) {
        assert_false(r.containsBulk(bulk_context, value));
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

DEFINE_TEST(test_cpp_add_range_closed_64) {
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
    auto b1 = uint64_t(1) << 32;
    std::vector<std::pair<uint64_t, uint64_t>> ranges = {
        {b1, b1 + 10},
        {b1 + 100, b1 + 100},  // one element
        {b1 - 10, b1 + 10},
        {b1 + 2, b1 - 2}};
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
DEFINE_TEST(test_bitmap_of_32) {
    Roaring r1 = Roaring::bitmapOfList({1, 2, 4});
    r1.printf();
    printf("\n");
    Roaring r2 = Roaring::bitmapOf(3, 1, 2, 4);
    r2.printf();
    printf("\n");
    assert_true(r1 == r2);

    Roaring r1d = {1, 2, 4};
    assert_true(r1 == r1d);

    Roaring r3a = Roaring::bitmapOfList({7, 8, 9});
    r3a = {1, 2, 4};  // overwrite with assignment operator
    assert_true(r1 == r3a);
}

DEFINE_TEST(test_bitmap_of_64) {
    Roaring64Map r1 = Roaring64Map::bitmapOfList({1, 2, 4});
    r1.printf();
    Roaring64Map r2 =
        Roaring64Map::bitmapOf(3, uint64_t(1), uint64_t(2), uint64_t(4));
    r2.printf();
    assert_true(r1 == r2);

    Roaring64Map r1d = {1, 2, 4};
    assert_true(r1 == r1d);

    Roaring64Map r3a = Roaring64Map::bitmapOfList({7, 8, 9});
    r3a = {1, 2, 4};  // overwrite with assignment operator
    assert_true(r1 == r3a);
}

DEFINE_TEST(test_cpp_add_range_open_64) {
    {
        // 32-bit integers
        Roaring64Map r1;
        r1.addRange(uint32_t(1), uint32_t(5));
        Roaring64Map r2;
        for (uint32_t v = 1; v < 5; ++v) {
            r2.add(v);
        }
        assert_true(r1 == r2);
    }
    auto b1 = uint64_t(1) << 32;
    std::vector<std::pair<uint64_t, uint64_t>> ranges = {
        {b1, b1 + 10},
        {b1 - 10, b1 + 10},
        {b1 + 100, b1 + 100},  // empty
        {b1 + 2, b1 - 2}};
    for (const auto &range : ranges) {
        uint64_t min = range.first;
        uint64_t max = range.second;
        Roaring64Map r1;
        r1.addRange(min, max);
        Roaring64Map r2;
        for (uint64_t v = min; v < max; ++v) {
            r2.add(v);
        }
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_cpp_add_range_closed_large_64) {
    uint32_t start_high = 300;
    for (uint32_t end_high = start_high; end_high != 305; ++end_high) {
        auto begin = (uint64_t(start_high) << 32) + 0x01234567;
        auto end = (uint64_t(end_high) << 32) + 0x89abcdef;
        Roaring64Map r1;
        r1.addRangeClosed(begin, end);
        auto size = end - begin + 1;
        assert_true(r1.cardinality() == size);
    }
}

DEFINE_TEST(test_cpp_add_range_open_large_64) {
    uint32_t start_high = 300;
    for (uint32_t end_high = start_high; end_high != 305; ++end_high) {
        auto begin = (uint64_t(start_high) << 32) + 0x01234567;
        auto end = (uint64_t(end_high) << 32) + 0x89abcdef;
        Roaring64Map r1;
        r1.addRange(begin, end);
        auto size = end - begin;
        assert_true(r1.cardinality() == size);
    }
}

DEFINE_TEST(test_cpp_add_many) {
    std::vector<uint32_t> values = {9999, 123, 0xFFFFFFFF, 0xFFFFFFF7, 9999};
    Roaring r1;
    r1.addMany(values.size(), values.data());
    Roaring r2;
    for (const auto value : values) {
        r2.add(value);
    }
    assert_true(r1 == r2);
}

DEFINE_TEST(test_cpp_rank_many) {
    std::vector<uint32_t> values = {123, 9999, 9999, 0xFFFFFFF7, 0xFFFFFFFF};
    Roaring r1;
    r1.addMany(values.size(), values.data());

    std::vector<uint64_t> ranks(values.size());
    r1.rank_many(values.data(), values.data() + values.size(), ranks.data());
    std::vector<uint64_t> expect_ranks{1, 2, 2, 3, 4};
    assert_true(ranks == expect_ranks);
}

DEFINE_TEST(test_cpp_add_many_64) {
    {
        // 32-bit integers
        std::vector<uint32_t> values = {9999,       123, 0xFFFFFFFF,
                                        0xFFFFFFF7, 0,   9999};
        Roaring64Map r1;
        r1.addMany(values.size(), values.data());
        Roaring64Map r2;
        for (const auto value : values) {
            r2.add(value);
        }
        assert_true(r1 == r2);
    }

    auto b1 = uint64_t(1) << 32;
    auto b555 = uint64_t(555) << 32;

    std::vector<uint64_t> values = {
        b555 + 9999,       b1 + 123, b1 + 0xFFFFFFFF,
        b555 + 0xFFFFFFF7, 0,        b555 + 9999};
    Roaring64Map r1;
    r1.addMany(values.size(), values.data());
    Roaring64Map r2;
    for (const auto value : values) {
        r2.add(value);
    }
    assert_true(r1 == r2);
}

DEFINE_TEST(test_cpp_add_range_closed_combinatoric_64) {
    // Given 'num_slots_to_test' outer slots, we repeatedly seed a Roaring64Map
    // with all combinations of present and absent outer slots (basically the
    // powerset of {0...num_slots_to_test - 1}), then we add_range_closed
    // and see if the cardinality is what we expect.
    //
    // For example (assuming num_slots_to_test = 5), the iterations of the outer
    // loop represent these sets:
    // 1. {}
    // 2. {0}
    // 3. {1}
    // 4. {0, 1}
    // 5. {2}
    // 6. {0, 2}
    // 7. {1, 2}
    // 8. {0, 1, 2}
    // 9. {3}
    // and so forth...
    //
    // For example, in step 6 (representing set {0, 2}) we set a bit somewhere
    // in slot 0 and we set another bit somehwere in slot 2. The purpose of this
    // is to make sure 'addRangeClosed' does the right thing when it encounters
    // an arbitrary mix of present and absent slots. Then we call
    // 'addRangeClosed' over the whole range and confirm that the cardinality
    // is what we expect.
    const uint32_t num_slots_to_test = 5;
    const uint32_t base_slot = 50;

    const uint32_t bitmask_limit = 1 << num_slots_to_test;

    for (uint32_t bitmask = 0; bitmask < bitmask_limit; ++bitmask) {
        Roaring64Map roaring;

        // The 1-bits in 'bitmask' indicate which slots we want to seed
        // with a value.
        for (uint32_t bit_index = 0; bit_index < num_slots_to_test;
             ++bit_index) {
            if ((bitmask & (1 << bit_index)) == 0) {
                continue;
            }
            auto slot = base_slot + bit_index;
            auto value = (uint64_t(slot) << 32) + bit_index;
            roaring.add(value);
        }

        auto first_bucket = uint64_t(base_slot) << 32;
        auto last_bucket = uint64_t(base_slot + num_slots_to_test - 1) << 32;

        roaring.addRangeClosed(first_bucket, last_bucket + uint32_max);

        auto expected_cardinality = num_slots_to_test * (uint64_t(1) << 32);
        assert_int_equal(expected_cardinality, roaring.cardinality());
    }
}

DEFINE_TEST(test_cpp_remove_range_closed_64) {
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

DEFINE_TEST(test_cpp_remove_range_64) {
    // Because removeRange delegates to removeRangeClosed, we do most of the
    // unit testing in test_cpp_remove_range_closed_64(). We just do a couple of
    // sanity checks here.
    Roaring64Map r1;
    auto b5 = uint64_t(5) << 32;

    r1.add(0u);         // 32-bit add
    r1.add(b5 + 1000);  // arbitrary 64 bit add
    r1.add(b5 + 1001);  // arbitrary 64 bit add
    r1.add(uint64_max - 1000);
    r1.add(uint64_max);  // highest possible bit

    // Half-open interval: result should be the set {0, maxUint64}
    r1.removeRange(1, uint64_max);

    Roaring64Map r2 = Roaring64Map::bitmapOf(2, uint64_t(0), uint64_max);
    assert_true(r1 == r2);
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
    r1.add(uint32_max);
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
    r1.add((uint64_t)uint32_max);
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

DEFINE_TEST(test_cpp_frozen_portable) {
    const uint64_t s = 65536;

    Roaring r1;
    r1.add(0);
    r1.add(uint32_max);
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
    size_t num_bytes = r1.getSizeInBytes(true);
    char *buf = (char *)malloc(num_bytes);
    r1.write(buf, true);

    // ensure the frozen bitmap is the same as the original
    const Roaring r2 = Roaring::portableDeserializeFrozen(buf);
    assert_true(r1 == r2);

    {
        Roaring r;
        r.addRange(0, 100000);
        r.flip(90000, 91000);
        r.runOptimize();

        // allocate a buffer and serialize to it
        size_t num_bytes1 = r.getSizeInBytes(true);
        char *buf1 = (char *)malloc(num_bytes1);
        r.write(buf1, true);

        // ensure the frozen bitmap is the same as the original
        const Roaring rr = Roaring::portableDeserializeFrozen(buf1);
        assert_true(r == rr);
        free(buf1);
    }

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
        Roaring a = Roaring::portableDeserializeFrozen(buf);
        Roaring b(std::move(a));
        assert_true(b == r1);
    }

    // move assignment operator
    {
        Roaring a = Roaring::portableDeserializeFrozen(buf);
        Roaring b;
        b = std::move(a);
        assert_true(b == r1);
    }

    free(buf);
}

DEFINE_TEST(test_cpp_frozen_64_portable) {
    const uint64_t s = 65536;

    Roaring64Map r1;
    r1.add((uint64_t)0);
    r1.add((uint64_t)uint32_max);
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

    size_t num_bytes = r1.getSizeInBytes(true);
    char *buf = (char *)malloc(num_bytes);
    r1.write(buf, true);

    const Roaring64Map r2 = Roaring64Map::portableDeserializeFrozen(buf);
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
        Roaring64Map a = Roaring64Map::portableDeserializeFrozen(buf);
        Roaring64Map b(std::move(a));
        assert_true(b == r1);
    }

    // move assignment operator
    {
        Roaring64Map a = Roaring64Map::portableDeserializeFrozen(buf);
        Roaring64Map b;
        b = std::move(a);
        assert_true(b == r1);
    }

    free(buf);
}

DEFINE_TEST(test_cpp_flip) {
    {
        // flipping an empty map works as expected
        Roaring r1;
        r1.flip(2, 5);
        Roaring r2 = Roaring::bitmapOf(3, 2, 3, 4);
        assert_true(r1 == r2);
    }
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
        Roaring r1 = Roaring::bitmapOf(1, uint32_max);
        r1.flip(uint32_max, static_cast<uint64_t>(uint32_max) + 1);
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

DEFINE_TEST(test_cpp_flip_closed) {
    {
        // flipping an empty map works as expected
        Roaring r1;
        r1.flipClosed(2, 5);
        Roaring r2 = Roaring::bitmapOf(4, 2, 3, 4, 5);
        assert_true(r1 == r2);
    }
    {
        // nothing is affected outside of the given range
        Roaring r1 = Roaring::bitmapOf(3, 1, 3, 6);
        r1.flipClosed(2, 4);
        Roaring r2 = Roaring::bitmapOf(4, 1, 2, 4, 6);
        assert_true(r1 == r2);
    }
    {
        // given range can go outside of existing range
        Roaring r1 = Roaring::bitmapOf(2, 1, 3);
        r1.flipClosed(0, 4);
        Roaring r2 = Roaring::bitmapOf(3, 0, 2, 4);
        assert_true(r1 == r2);
    }
    {
        // range end is inclusive
        Roaring r1 = Roaring::bitmapOf(2, 1, 3);
        r1.flipClosed(1, 2);
        Roaring r2 = Roaring::bitmapOf(2, 2, 3);
        assert_true(r1 == r2);
    }
    {
        // uint32 max can be flipped
        Roaring r1 = Roaring::bitmapOf(1, uint32_max);
        r1.flipClosed(uint32_max, uint32_max);
        assert_true(r1.isEmpty());
    }
    {
        // empty range does nothing
        Roaring r1 = Roaring::bitmapOf(2, 2, 3);
        Roaring r2 = r1;
        r1.flipClosed(2, 1);
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_cpp_flip_64) {
    {
        // 32-bit test
        {
            // flipping an empty map works as expected
            Roaring64Map r1;
            r1.flip(2, 5);
            auto r2 = Roaring64Map::bitmapOf(3, uint64_t(2), uint64_t(3),
                                             uint64_t(4));
            assert_true(r1 == r2);
        }
        {
            // nothing is affected outside of the given range
            auto r1 = Roaring64Map::bitmapOf(3, uint64_t(1), uint64_t(3),
                                             uint64_t(6));
            r1.flip(uint32_t(2), uint32_t(5));
            Roaring64Map r2 = Roaring64Map::bitmapOf(
                4, uint64_t(1), uint64_t(2), uint64_t(4), uint64_t(6));
            assert_true(r1 == r2);
        }
        {
            // given range can go outside of existing range
            auto r1 = Roaring64Map::bitmapOf(2, uint64_t(1), uint64_t(3));
            r1.flip(uint32_t(0), uint32_t(5));
            auto r2 = Roaring64Map::bitmapOf(3, uint64_t(0), uint64_t(2),
                                             uint64_t(4));
            assert_true(r1 == r2);
        }
        {
            // range end is exclusive
            auto r1 = Roaring64Map::bitmapOf(2, uint64_t(1), uint64_t(3));
            r1.flip(uint32_t(1), uint32_t(3));
            auto r2 = Roaring64Map::bitmapOf(2, uint64_t(2), uint64_t(3));
            assert_true(r1 == r2);
        }
        {
            // uint32 max can be flipped
            auto r1 = Roaring64Map::bitmapOf(1, uint64_t(uint32_max));
            r1.flip(uint32_max, uint64_t(uint32_max) + 1);
            assert_true(r1.isEmpty());
        }
        {
            // empty range does nothing
            auto r1 = Roaring64Map::bitmapOf(2, uint64_t(2), uint64_t(3));
            auto r2 = r1;
            r1.flip(uint32_t(2), uint32_t(2));
            assert_true(r1 == r2);
        }
    }

    const auto b1 = uint64_t(1) << 32;
    const auto b2 = uint64_t(2) << 32;

    {
        // nothing is affected outside of the given range
        Roaring64Map r1 = Roaring64Map::bitmapOf(3, b1 - 3, b1, b1 + 3);
        r1.flip(b1 - 2, b1 + 2);
        Roaring64Map r2 =
            Roaring64Map::bitmapOf(5, b1 - 3, b1 - 2, b1 - 1, b1 + 1, b1 + 3);
        assert_true(r1 == r2);
    }
    {
        // given range can go outside of existing range
        Roaring64Map r1 = Roaring64Map::bitmapOf(2, b1 - 2, b1);
        r1.flip(b1 - 3, b1 + 2);
        Roaring64Map r2 = Roaring64Map::bitmapOf(3, b1 - 3, b1 - 1, b1 + 1);
        assert_true(r1 == r2);
    }
    {
        // range end is exclusive
        Roaring64Map r1 = Roaring64Map::bitmapOf(2, b2 - 1, b2 + 2);
        r1.flip(b2 - 1, b2 + 2);
        Roaring64Map r2;
        for (uint64_t i = b2; i <= b2 + 2; ++i) {
            r2.add(i);
        }
        assert_true(r1 == r2);
    }
    {
        // uint32 max can be flipped
        Roaring64Map r1 =
            Roaring64Map::bitmapOf(1, static_cast<uint64_t>(uint32_max));
        r1.flip(uint32_max, static_cast<uint64_t>(uint32_max) + 1);
        assert_true(r1.isEmpty());
    }
    {
        // empty range does nothing
        Roaring64Map r1 = Roaring64Map::bitmapOf(2, b1 - 1, b1);
        Roaring64Map r2 = r1;
        r1.flip(b1 - 1, b1 - 1);
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_cpp_flip_closed_64) {
    {
        // 32-bit test
        {
            // flipping an empty map works as expected
            Roaring64Map r1;
            r1.flipClosed(uint32_t(2), uint32_t(5));
            auto r2 = Roaring64Map::bitmapOf(4, uint64_t(2), uint64_t(3),
                                             uint64_t(4), uint64_t(5));
            assert_true(r1 == r2);
        }
        {
            // nothing is affected outside of the given range
            auto r1 = Roaring64Map::bitmapOf(3, uint64_t(1), uint64_t(3),
                                             uint64_t(6));
            r1.flipClosed(uint32_t(2), uint32_t(4));
            Roaring64Map r2 = Roaring64Map::bitmapOf(
                4, uint64_t(1), uint64_t(2), uint64_t(4), uint64_t(6));
            assert_true(r1 == r2);
        }
        {
            // given range can go outside of existing range
            auto r1 = Roaring64Map::bitmapOf(2, uint64_t(1), uint64_t(3));
            r1.flipClosed(uint32_t(0), uint32_t(4));
            auto r2 = Roaring64Map::bitmapOf(3, uint64_t(0), uint64_t(2),
                                             uint64_t(4));
            assert_true(r1 == r2);
        }
        {
            // range end is inclusive
            auto r1 = Roaring64Map::bitmapOf(2, uint64_t(1), uint64_t(3));
            r1.flipClosed(uint32_t(1), uint32_t(2));
            auto r2 = Roaring64Map::bitmapOf(2, uint64_t(2), uint64_t(3));
            assert_true(r1 == r2);
        }
        {
            // uint32 max can be flipped
            auto r1 = Roaring64Map::bitmapOf(1, uint64_t(uint32_max));
            r1.flipClosed(uint32_max, uint32_max);
            assert_true(r1.isEmpty());
        }
        {
            // empty range does nothing
            auto r1 = Roaring64Map::bitmapOf(2, uint64_t(2), uint64_t(3));
            auto r2 = r1;
            r1.flipClosed(uint32_t(2), uint32_t(1));
            assert_true(r1 == r2);
        }
    }

    const auto b1 = uint64_t(1) << 32;
    const auto b2 = uint64_t(2) << 32;

    {
        // nothing is affected outside of the given range
        Roaring64Map r1 = Roaring64Map::bitmapOf(3, b1 - 3, b1, b1 + 3);
        r1.flipClosed(b1 - 2, b1 + 1);
        Roaring64Map r2 =
            Roaring64Map::bitmapOf(5, b1 - 3, b1 - 2, b1 - 1, b1 + 1, b1 + 3);
        assert_true(r1 == r2);
    }
    {
        // given range can go outside of existing range
        Roaring64Map r1 = Roaring64Map::bitmapOf(2, b1 - 2, b1);
        r1.flipClosed(b1 - 3, b1 + 1);
        Roaring64Map r2 = Roaring64Map::bitmapOf(3, b1 - 3, b1 - 1, b1 + 1);
        assert_true(r1 == r2);
    }
    {
        // range end is inclusive
        Roaring64Map r1 = Roaring64Map::bitmapOf(2, b2 - 1, b2 + 2);
        r1.flipClosed(b2 - 1, b2 + 1);
        Roaring64Map r2;
        for (uint64_t i = b2; i <= b2 + 2; ++i) {
            r2.add(i);
        }
        assert_true(r1 == r2);
    }
    {
        // uint32 max can be flipped
        Roaring64Map r1 =
            Roaring64Map::bitmapOf(1, static_cast<uint64_t>(uint32_max));
        r1.flipClosed(uint32_max, uint32_max);
        assert_true(r1.isEmpty());
    }
    {
        // empty range does nothing
        Roaring64Map r1 = Roaring64Map::bitmapOf(2, b1 - 1, b1);
        Roaring64Map r2 = r1;
        r1.flipClosed(b1 - 1, b1 - 2);
        assert_true(r1 == r2);
    }
}

DEFINE_TEST(test_combinatoric_flip_many_64) {
    // Given 'num_slots_to_test' outer slots, we repeatedly seed a Roaring64Map
    // with all combinations of present and absent outer slots (basically the
    // powerset of {0...num_slots_to_test - 1}), then we add_range_closed
    // and see if the cardinality is what we expect.
    //
    // For example (assuming num_slots_to_test = 5), the iterations of the outer
    // loop represent these sets:
    // 1. {}
    // 2. {0}
    // 3. {1}
    // 4. {0, 1}
    // 5. {2}
    // 6. {0, 2}
    // 7. {1, 2}
    // 8. {0, 1, 2}
    // 9. {3}
    // and so forth...
    //
    // For example, in step 6 (representing set {0, 2}) we set a bit somewhere
    // in slot 0 and we set another bit somehwere in slot 2. The purpose of this
    // is to make sure 'flipClosed' does the right thing when it encounters
    // an arbitrary mix of present and absent slots. Then we call
    // 'flipClosed' over the whole range and confirm that the cardinality
    // is what we expect.
    const uint32_t num_slots_to_test = 5;
    const uint32_t base_slot = 50;

    const uint32_t bitmask_limit = 1 << num_slots_to_test;

    for (uint32_t bitmask = 0; bitmask < bitmask_limit; ++bitmask) {
        Roaring64Map roaring;
        uint32_t num_one_bits = 0;

        // The 1-bits in 'bitmask' indicate which slots we want to seed
        // with a value.
        for (uint32_t bit_index = 0; bit_index < num_slots_to_test;
             ++bit_index) {
            if ((bitmask & (1 << bit_index)) == 0) {
                continue;
            }
            auto slot = base_slot + bit_index;
            auto value = (uint64_t(slot) << 32) + 0x1234567 + bit_index;
            roaring.add(value);
            ++num_one_bits;
        }

        auto first_bucket = uint64_t(base_slot) << 32;
        auto last_bucket = uint64_t(base_slot + num_slots_to_test - 1) << 32;

        roaring.flipClosed(first_bucket, last_bucket + uint32_max);

        // Slots not initalized with a bit will now have cardinality 2^32
        // Slots initialized with a bit will have cardinality 2^32 - 1
        auto expected_cardinality =
            num_slots_to_test * (uint64_t(1) << 32) - num_one_bits;
        assert_int_equal(expected_cardinality, roaring.cardinality());
    }
}

DEFINE_TEST(test_cpp_is_subset_64) {
    Roaring64Map r1 = Roaring64Map::bitmapOf(1, uint64_t(1));
    Roaring64Map r2 = Roaring64Map::bitmapOf(1, uint64_t(1) << 32);
    Roaring64Map r3 = r1 & r2;
    assert_true(r3.isSubset(r1));
    assert_true(r3.isSubset(r2));
}

DEFINE_TEST(test_cpp_fast_union_64) {
    auto update = [](Roaring64Map *dest, uint32_t bitmask, uint32_t offset) {
        for (uint32_t i = 0; i != 32; ++i) {
            if ((bitmask & (1 << i)) != 0) {
                dest->add(offset + i);
            }
        }
    };

    // Generate three Roaring64Maps that have a variety of combinations of
    // present and absent slots and calculate their union with fastunion.
    const uint32_t num_slots_to_test = 4;
    const uint32_t bitmask_limit = 1 << num_slots_to_test;

    for (size_t r0_bitmask = 0; r0_bitmask != bitmask_limit; ++r0_bitmask) {
        for (size_t r1_bitmask = 0; r1_bitmask != bitmask_limit; ++r1_bitmask) {
            for (size_t r2_bitmask = 0; r2_bitmask != bitmask_limit;
                 ++r2_bitmask) {
                Roaring64Map r0_map, r1_map, r2_map;
                update(&r0_map, r0_bitmask, 0);
                update(&r1_map, r1_bitmask, 0x1000);
                update(&r2_map, r2_bitmask, 0x2000);

                const Roaring64Map *maps[] = {&r0_map, &r1_map, &r2_map};
                auto actual = Roaring64Map::fastunion(3, maps);

                Roaring64Map expected;
                update(&expected, r0_bitmask, 0);
                update(&expected, r1_bitmask, 0x1000);
                update(&expected, r2_bitmask, 0x2000);

                assert_true(expected == actual);
            }
        }
    }
}

DEFINE_TEST(test_cpp_to_string) {
    // test toString
    const auto b5 = uint64_t(5) << 32;

    {
        // 32-bit test.
        Roaring a;
        assert_string_equal("{}", a.toString().c_str());

        a.add(1);
        assert_string_equal("{1}", a.toString().c_str());

        a.add(2);
        a.add(3);
        a.add(uint32_max);
        assert_string_equal("{1,2,3,4294967295}", a.toString().c_str());
    }

    {
        // 64-bit test.
        Roaring64Map r;
        assert_string_equal("{}", r.toString().c_str());

        r.add(b5 + 100);
        assert_string_equal("{21474836580}", r.toString().c_str());

        r.add(1u);
        r.add(2u);
        r.add(uint32_max);
        r.add(uint64_max);
        assert_string_equal("{1,2,4294967295,21474836580,18446744073709551615}",
                            r.toString().c_str());
    }
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
bool test64Deserialize(const std::string &filename) {
#if CROARING_IS_BIG_ENDIAN
    (void)filename;
    printf("Big-endian IO unsupported.\n");
#else  // CROARING_IS_BIG_ENDIAN
    std::ifstream in(TEST_DATA_DIR + filename, std::ios::binary);
    std::vector<char> buf1(std::istreambuf_iterator<char>(in), {});
    printf("Reading %lu bytes\n", (unsigned long)buf1.size());
    Roaring64Map roaring;
#if ROARING_EXCEPTIONS
    try {
        roaring = Roaring64Map::readSafe(buf1.data(), buf1.size());
    } catch (...) {
        return false;
    }
#else   // ROARING_EXCEPTIONS
    roaring = Roaring64Map::readSafe(buf1.data(), buf1.size());
#endif  // ROARING_EXCEPTIONS
    std::vector<char> buf2(roaring.getSizeInBytes());
    assert_true(buf1.size() == buf2.size());
    assert_true(roaring.write(buf2.data()) == buf2.size());
    for (size_t i = 0; i < buf1.size(); ++i) {
        assert_true(buf1[i] == buf2[i]);
    }
#endif  // CROARING_IS_BIG_ENDIAN
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

DEFINE_TEST(test_cpp_deserialize_add_offset) {
    std::string filename = "addoffsetinput.bin";
    std::ifstream in(TEST_DATA_DIR + filename, std::ios::binary);
    std::vector<char> buf1(std::istreambuf_iterator<char>(in), {});
    printf("Reading %lu bytes\n", (unsigned long)buf1.size());
    Roaring r0 = Roaring::readSafe(buf1.data(), buf1.size());

    const uint32_t offset = 4107040;
    const uint64_t cardinality = r0.cardinality();

    Roaring r1(roaring_bitmap_add_offset(&r0.roaring, offset));

    std::vector<char> buf2(r1.getSizeInBytes());
    r1.write(buf2.data());
    Roaring r2 = Roaring::readSafe(buf2.data(), buf2.size());

    assert_int_equal(r0.cardinality(), r1.cardinality());
    assert_int_equal(r0.cardinality(), r2.cardinality());

    std::vector<uint32_t> numbers0(cardinality);
    std::vector<uint32_t> numbers1(cardinality);
    std::vector<uint32_t> numbers2(cardinality);

    r0.toUint32Array(numbers0.data());
    r1.toUint32Array(numbers1.data());
    r2.toUint32Array(numbers2.data());

    for (uint32_t i = 0; i < cardinality; ++i) {
        assert_int_equal(numbers0[i] + offset, numbers1[i]);
    }
    assert_true(numbers1 == numbers2);
    assert_true(r1 == r2);
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

DEFINE_TEST(test_cpp_contains_range_interleaved_containers) {
    Roaring roaring;
    // Range from last position in first container up to second position in 3rd
    // container.
    roaring.addRange(0xFFFF, 0x1FFFF + 2);
    // Query from last position in 2nd container up to second position in 4th
    // container. There is no 4th container in the bitmap.
    roaring.containsRange(0x1FFFF, 0x2FFFF + 2);
}

int main() {
    roaring::misc::tellmeall();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(fuzz_001),
        cmocka_unit_test(test_bitmap_of_32),
        cmocka_unit_test(test_bitmap_of_64),
        cmocka_unit_test(serial_test),
#if !CROARING_IS_BIG_ENDIAN
        cmocka_unit_test(test_example_true),
        cmocka_unit_test(test_example_false),
        cmocka_unit_test(test_example_cpp_true),
        cmocka_unit_test(test_example_cpp_false),
        cmocka_unit_test(test_example_cpp_64_true),
        cmocka_unit_test(test_example_cpp_64_false),
#endif
        cmocka_unit_test(test_cpp_add_remove_checked),
        cmocka_unit_test(test_cpp_add_remove_checked_64),
        cmocka_unit_test(test_cpp_add_range),
        cmocka_unit_test(test_cpp_remove_range),
        cmocka_unit_test(test_cpp_add_range_closed_64),
        cmocka_unit_test(test_cpp_add_range_open_64),
        cmocka_unit_test(test_cpp_add_range_closed_large_64),
        cmocka_unit_test(test_cpp_add_range_open_large_64),
        cmocka_unit_test(test_cpp_add_many),
        cmocka_unit_test(test_cpp_add_many_64),
        cmocka_unit_test(test_cpp_add_range_closed_combinatoric_64),
        cmocka_unit_test(test_cpp_add_bulk),
        cmocka_unit_test(test_cpp_contains_bulk),
        cmocka_unit_test(test_cpp_rank_many),
        cmocka_unit_test(test_cpp_remove_range_closed_64),
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
        cmocka_unit_test(test_roaring64_remove_32),
        cmocka_unit_test(test_roaring64_add_and_remove),
        cmocka_unit_test(test_cpp_bidirectional_iterator_64),
        cmocka_unit_test(test_cpp_frozen),
        cmocka_unit_test(test_cpp_frozen_64),
        cmocka_unit_test(test_cpp_frozen_portable),
        cmocka_unit_test(test_cpp_frozen_64_portable),
        cmocka_unit_test(test_cpp_flip),
        cmocka_unit_test(test_cpp_flip_closed),
        cmocka_unit_test(test_cpp_flip_64),
        cmocka_unit_test(test_cpp_flip_closed_64),
        cmocka_unit_test(test_combinatoric_flip_many_64),
#if !CROARING_IS_BIG_ENDIAN
        cmocka_unit_test(test_cpp_deserialize_64_empty),
        cmocka_unit_test(test_cpp_deserialize_64_32bit_vals),
        cmocka_unit_test(test_cpp_deserialize_64_spread_vals),
        cmocka_unit_test(test_cpp_deserialize_64_high_vals),
        cmocka_unit_test(test_cpp_deserialize_add_offset),
#if ROARING_EXCEPTIONS
        cmocka_unit_test(test_cpp_deserialize_64_empty_input),
        cmocka_unit_test(test_cpp_deserialize_64_size_too_small),
        cmocka_unit_test(test_cpp_deserialize_64_invalid_size),
        cmocka_unit_test(test_cpp_deserialize_64_key_too_small),
#endif
#endif  // !CROARING_IS_BIG_ENDIAN
        cmocka_unit_test(issue316),
        cmocka_unit_test(test_issue304),
        cmocka_unit_test(issue_336),
        cmocka_unit_test(issue_372),
        cmocka_unit_test(test_cpp_is_subset_64),
        cmocka_unit_test(test_cpp_fast_union_64),
        cmocka_unit_test(test_cpp_to_string),
        cmocka_unit_test(test_cpp_remove_run_compression),
        cmocka_unit_test(test_cpp_contains_range_interleaved_containers),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
