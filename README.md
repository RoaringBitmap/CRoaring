# CRoaring
Roaring bitmaps in C

# Current status

This library is being actively developed. For the time being, this library is not **usable for anything but
 _research_**. **Do not attempt to use this library in production systems**. See http://roaringbitmap.org for
a list of supported libraries.

# Introduction

Bitsets, also called bitmaps, are commonly used as fast data structures. Unfortunately, they can use too much memory.
 To compensate, we often use compressed bitmaps.

Roaring bitmaps are compressed bitmaps which tend to outperform conventional compressed bitmaps such as WAH, EWAH or Concise.
They are used by several major systems such as [Apache Lucene][lucene] and derivative systems such as [Solr][solr] and
[Elasticsearch][elasticsearch], [Metamarkets' Druid][druid], [Apache Spark][spark], [Whoosh][whoosh] and eBay's [Apache Kylin][kylin].

[lucene]: https://lucene.apache.org/
[solr]: https://lucene.apache.org/solr/
[elasticsearch]: https://www.elastic.co/products/elasticsearch
[druid]: http://druid.io/
[spark]: https://spark.apache.org/
[whoosh]: https://bitbucket.org/mchaput/whoosh/wiki/Home
[kylin]: http://kylin.apache.org/

# Objective

The primary goal of the CRoaring is to provide a high performance low-level implementation that fully take advantage
of the latest hardware. Roaring bitmaps are already available on a variety of platform through Java, Go, Rust... implementations. CRoaring is a library that seeks to achieve superior performance by staying close to the latest hardware.

# Requirements

- 64-bit Linux-like operating system (including MacOS)
- Recent Intel processor: Haswell (2013) or better. For legacy Intel processors without AVX support, build the project with ``-DAVX_TUNING=OFF``.
- Recent C compiler (GCC 4.8 or better)
- CMake
- clang-format (optional)

Support for non-Intel hardware  and other compilers might be added later. Contributions are invited.

# Example

```c
////
//// #include "roaring.h"
////

// create a new empty bitmap
roaring_bitmap_t *r1 = roaring_bitmap_create();
// then we can add values
for (uint32_t i = 100; i < 1000; i++) roaring_bitmap_add(r1, i);
// check whether a value is contained
assert(roaring_bitmap_contains(r1, 500));
// compute how many bits there are:
uint32_t cardinality = roaring_bitmap_get_cardinality(r1);
printf("Cardinality = %d \n", cardinality);

// if your bitmaps have long runs, you can compress them by calling
// run_optimize
uint32_t expectedsizebasic = roaring_bitmap_portable_size_in_bytes(r1);
roaring_bitmap_run_optimize(r1);
uint32_t expectedsizerun = roaring_bitmap_portable_size_in_bytes(r1);
printf("size before run optimize %d bytes, and after %d bytes\n",
       expectedsizebasic, expectedsizerun);

// create a new bitmap containing the values {1,2,3,5,6}
roaring_bitmap_t *r2 = roaring_bitmap_of(5, 1, 2, 3, 5, 6);
roaring_bitmap_printf(r2);  // print it

// we can also create a bitmap from a pointer to 32-bit integers
uint32_t somevalues[] = {2, 3, 4};
roaring_bitmap_t *r3 = roaring_bitmap_of_ptr(3, somevalues);

// we can also go in reverse and go from arrays to bitmaps
uint32_t card1;
uint32_t *arr1 = roaring_bitmap_to_uint32_array(r1, &card1);
roaring_bitmap_t *r1f = roaring_bitmap_of_ptr(card1, arr1);
free(arr1);
assert(roaring_bitmap_equals(r1, r1f));  // what we recover is equal

// we can copy and compare bitmaps
roaring_bitmap_t *z = roaring_bitmap_copy(r3);
assert(roaring_bitmap_equals(r3, z));  // what we recover is equal
roaring_bitmap_free(z);

// we can compute union two-by-two
roaring_bitmap_t *r1_2_3 = roaring_bitmap_or(r1, r2);
roaring_bitmap_or_inplace(r1_2_3, r3);

// we can compute a big union
const roaring_bitmap_t *allmybitmaps[] = {r1, r2, r3};
roaring_bitmap_t *bigunion = roaring_bitmap_or_many(3, allmybitmaps);
assert(
    roaring_bitmap_equals(r1_2_3, bigunion));  // what we recover is equal
// can also do the big union with a heap
roaring_bitmap_t *bigunionheap = roaring_bitmap_or_many_heap(3, allmybitmaps);
assert_true(roaring_bitmap_equals(r1_2_3, bigunionheap));

roaring_bitmap_free(r1_2_3);
roaring_bitmap_free(bigunion);
roaring_bitmap_free(bigunionheap);

// we can compute intersection two-by-two
roaring_bitmap_t *i1_2 = roaring_bitmap_and(r1, r2);
roaring_bitmap_free(i1_2);

// we can write a bitmap to a pointer and recover it later
uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
char *serializedbytes = malloc(expectedsize);
roaring_bitmap_portable_serialize(r1, serializedbytes);
roaring_bitmap_t *t = roaring_bitmap_portable_deserialize(serializedbytes);
assert(roaring_bitmap_equals(r1, t));  // what we recover is equal
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
```

# Building

CRoaring follows the standard cmake workflow. Starting from the root directory of
the project (CRoaring), you can do:

```
mkdir -p build
cd build
cmake ..
make
```
(You can replace the ``build`` directory with any other directory name.)

For a debug release, starting from the root directory of the project (CRoaring), try

```
mkdir -p debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON ..
make
```

(Of course you can replace the ``debug`` directory with any other directory name.)


To run unit tests (you must first run ``make``):

```
make test
```

The detailed output of the tests can be found in ``Testing/Temporary/LastTest.log``.

To run real-data benchmark

```
./real_bitmaps_benchmark ../benchmarks/realdata/census1881
```
where you must adjust the path "../benchmarks/realdata/census1881" so that it points to one of the directories in the benchmarks/realdata directory.


To check that your code abides by the style convention (make sure that ``clang-format`` is installed):

```
./tools/clang-format-check.sh
```

To reformat your code according to the style convention (make sure that ``clang-format`` is installed):

```
./tools/clang-format.sh
```

# References and further reading

-  Array layouts for comparison-based searching http://arxiv.org/pdf/1509.05053.pdf
-  Schlegel et al., Fast Sorted-Set Intersection using SIMD Instructions

### References about Roaring

-  Samy Chambi, Daniel Lemire, Owen Kaser, Robert Godin,
Better bitmap performance with Roaring bitmaps,
Software: Practice and Experience Volume 46, Issue 5, pages 709–719, May 2016
http://arxiv.org/abs/1402.6407 This paper used data from http://lemire.me/data/realroaring2014.html
- Daniel Lemire, Gregory Ssi-Yan-Kai, Owen Kaser, Consistently faster and smaller compressed bitmaps with Roaring, Software: Practice and Experience (accepted in 2016, to appear) http://arxiv.org/abs/1603.06549

# Issues to consider

AVX operations take a while before they warm up to their best speed
as documented by Agner Fog and others.

There is a trade-off between throughput and latency. For example,
prefetching might improve latency, but at the expense of throughput
on a multicore system.

Some instructions, like POPCNT,  take a serious hit under hyperthreading.
