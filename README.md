# CRoaring [![Build Status](https://travis-ci.org/RoaringBitmap/CRoaring.png)](https://travis-ci.org/RoaringBitmap/CRoaring)
Roaring bitmaps in C (and C++)

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


(c) 2016 The CRoaring authors.



# Requirements

- The library should build on a  Linux-like operating system (including MacOS).
- We also support Microsoft Visual studio, see https://github.com/mrboojum/CRoaring4VS .
- Though most reasonable processors should be supported, we expect a recent Intel processor: Haswell (2013) or better but support all x64/x86 processors. The library should build without problem on ARM processors.
- Recent C compiler (GCC 4.8 or better), there is also an optional C++ class that requires a C++ compiler
- CMake (to contribute to the project, users can rely on amalgamation/unity builds)
- clang-format (optional)

Serialization on big endian hardware may not be compatible with serialization on little endian hardware.

# Amalgamation/Unity Build

The CRoaring library can be amalgamated into a single source file that makes it easier
for integration into other projects. Moreover, by making it possible to compile
all the critical code into one compilation unit, it can improve the performance. For
the rationale, please see the SQLite documentation, https://www.sqlite.org/amalgamation.html,
or the corresponding Wikipedia entry (https://en.wikipedia.org/wiki/Single_Compilation_Unit).
Users who choose this route, do not need to rely on CRoaring's build system (based on CMake).

To generate the amalgamated files, you can invoke a bash script...

```bash
./amalgamation.sh
```

It is not necessary to invoke the script in the CRoaring directory. You can invoke
it from any directory where you want the amalgamation files to be written.

It will generate three files for C users: ``roaring.h``, ``roaring.c`` and ``almagamation_demo.c``... as well as some brief instructions. The ``almagamation_demo.c`` file is a short example, whereas ``roaring.h`` and ``roaring.c`` are "amalgamated" files (including all source and header files for the project). This means that you can simply copy the files ``roaring.h`` and ``roaring.c`` into your project and be ready to go! No need to produce a library! See the ``almagamation_demo.c`` file.

The script will also generate C++ files for C++ users, including an example.

If you prefer a silent output, you can use the following command to redirect ``stdout`` :

```bash
./amalgamation.sh > /dev/null
```

# API

The interface is found in the file ``include/roaring/roaring.h``.

# Example (C)

```c
////
//// #include <roaring/roaring.h>
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
uint64_t card1 = roaring_bitmap_get_cardinality(r1);
uint32_t *arr1 = (uint32_t *) malloc(card1 * sizeof(uint32_t));
assert(arr1  != NULL);
roaring_bitmap_to_uint32_array(r1, arr1);
roaring_bitmap_t *r1f = roaring_bitmap_of_ptr(card1, arr1);
free(arr1);
assert(roaring_bitmap_equals(r1, r1f));  // what we recover is equal
roaring_bitmap_free(r1f);

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
 * bool roaring_iterator_sumall(uint32_t value, void *param) {
 *        *(uint32_t *) param += value;
 *        return true; //iterate till the end
 *  }
 *
 */

roaring_bitmap_free(r1);
roaring_bitmap_free(r2);
roaring_bitmap_free(r3);
```

# Example (C++)

```c++
////
//// #include "roaring.hh" from cpp directory
////
Roaring r1;
for (uint32_t i = 100; i < 1000; i++) {
  r1.add(i);
}

// check whether a value is contained
assert(r1.contains(500));

// compute how many bits there are:
uint32_t cardinality = r1.cardinality();

// if your bitmaps have long runs, you can compress them by calling
// run_optimize
uint32_t size = r1.getSizeInBytes();
r1.runOptimize();

// you can enable "copy-on-write" for fast and shallow copies
r1.setCopyOnWrite(true);


uint32_t compact_size = r1.getSizeInBytes();
std::cout << "size before run optimize " << size << " bytes, and after "
            <<  compact_size << " bytes." << std::endl;


// create a new bitmap with varargs
Roaring r2 = Roaring::bitmapOf(5, 1, 2, 3, 5, 6);

r2.printf();
printf("\n");

// we can also create a bitmap from a pointer to 32-bit integers
const uint32_t values[] = {2, 3, 4};
Roaring r3(3, values);

// we can also go in reverse and go from arrays to bitmaps
uint64_t card1 = r1.cardinality();
uint32_t *arr1 = new uint32_t[card1];
r1.toUint32Array(arr1);
Roaring r1f(card1, arr1);
delete[] arr1;

// bitmaps shall be equal
assert(r1 == r1f);

// we can copy and compare bitmaps
Roaring z (r3);
assert(r3 == z);

// we can compute union two-by-two
Roaring r1_2_3 = r1 | r2;
r1_2_3 |= r3;

// we can compute a big union
const Roaring *allmybitmaps[] = {&r1, &r2, &r3};
Roaring bigunion = Roaring::fastunion(3, allmybitmaps);
assert(r1_2_3 == bigunion);

// we can compute intersection two-by-two
Roaring i1_2 = r1 & r2;

// we can write a bitmap to a pointer and recover it later
uint32_t expectedsize = r1.getSizeInBytes();
char *serializedbytes = new char [expectedsize];
r1.write(serializedbytes);
Roaring t = Roaring::read(serializedbytes);
assert(r1 == t);
delete[] serializedbytes;

// we can iterate over all values using custom functions
uint32_t counter = 0;
r1.iterate(roaring_iterator_sumall, &counter);
    /**
     * bool roaring_iterator_sumall(uint32_t value, void *param) {
     *        *(uint32_t *) param += value;
     *        return true; // iterate till the end
     *  }
     *
     */
```



# Building

CRoaring follows the standard cmake workflow. Starting from the root directory of
the project (CRoaring), you can do:

```
mkdir -p build
cd build
cmake ..
make
# follow by 'make test' if you want to test.
# you can also type 'make install' to install the library on your system
```
(You can replace the ``build`` directory with any other directory name.)

If wish to build an x64 version while disabling AVX2 and BMI2 support at the expense of performance, you can do the following :

````
mkdir -p buildnoavx
cd buildnoavx
cmake -DDISABLE_AVX=ON ..
make
```

If you have x64 hardware, but you wish to disable all x64-specific optimizations (including AVX), then you can
do the following...

````
mkdir -p buildnox64
cd buildnoavx
cmake -DDISABLE_X64=ON ..
make
```


For a debug release, starting from the root directory of the project (CRoaring), try

```
mkdir -p debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON ..
make
```

(Again, you can use the ``-DDISABLE_AVX=ON`` flag if you need it.)

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

# Python Wrapper

Tom Cornebize wrote a Python wrapper available at https://github.com/Ezibenroc/PyRoaringBitMap

# C# Wrapper

Brandon Smith wrote a C# wrapper available at https://github.com/RogueException/CRoaring.Net (works for Windows and Linux under x64 processors)

# C++ Wrapper for Visual Studio

There is C++ wrapper for Microsoft Visual Studio available at  https://github.com/mrboojum/CRoaring4VS (works under x86 and x64)

# Go (golang) Wrapper

There is a Go (golang) wrapper available at https://github.com/RoaringBitmap/gocroaring

# Rust Wrapper

Saulius Grigaliunas wrote a Rust wrapper available at https://github.com/saulius/croaring-rs

# References and further reading

-  Array layouts for comparison-based searching http://arxiv.org/pdf/1509.05053.pdf
-  Schlegel et al., Fast Sorted-Set Intersection using SIMD Instructions

### References about Roaring

-  Samy Chambi, Daniel Lemire, Owen Kaser, Robert Godin,
Better bitmap performance with Roaring bitmaps,
Software: Practice and Experience Volume 46, Issue 5, pages 709–719, May 2016
http://arxiv.org/abs/1402.6407 This paper used data from http://lemire.me/data/realroaring2014.html
- Daniel Lemire, Gregory Ssi-Yan-Kai, Owen Kaser, Consistently faster and smaller compressed bitmaps with Roaring, Software: Practice and Experience (accepted in 2016, to appear) http://arxiv.org/abs/1603.06549
- Samy Chambi, Daniel Lemire, Robert Godin, Kamel Boukhalfa, Charles Allen, Fangjin Yang, Optimizing Druid with Roaring bitmaps, IDEAS 2016, 2016. http://r-libre.teluq.ca/950/
