# CRoaring [![Build Status](https://travis-ci.org/RoaringBitmap/CRoaring.png)](https://travis-ci.org/RoaringBitmap/CRoaring)
Roaring bitmaps in C (and C++)

# Introduction

Bitsets, also called bitmaps, are commonly used as fast data structures. Unfortunately, they can use too much memory.
 To compensate, we often use compressed bitmaps.

Roaring bitmaps are compressed bitmaps which tend to outperform conventional compressed bitmaps such as WAH, EWAH or Concise.
They are used by several major systems such as [Apache Lucene][lucene] and derivative systems such as [Solr][solr] and
[Elasticsearch][elasticsearch], [Metamarkets' Druid][druid], [LinkedIn Pinot][pinot], [Netflix Atlas][atlas],  [Apache Spark][spark], [OpenSearchServer][opensearchserver], [Cloud Torrent][cloudtorrent], [Whoosh][whoosh],  [Pilosa][pilosa],  [Microsoft Visual Studio Team Services (VSTS)][vsts], and eBay's [Apache Kylin][kylin].

[lucene]: https://lucene.apache.org/
[solr]: https://lucene.apache.org/solr/
[elasticsearch]: https://www.elastic.co/products/elasticsearch
[druid]: http://druid.io/
[spark]: https://spark.apache.org/
[opensearchserver]: http://www.opensearchserver.com
[cloudtorrent]: https://github.com/jpillora/cloud-torrent
[whoosh]: https://bitbucket.org/mchaput/whoosh/wiki/Home
[pilosa]: https://www.pilosa.com/
[kylin]: http://kylin.apache.org/
[pinot]: http://github.com/linkedin/pinot/wiki
[vsts]: https://www.visualstudio.com/team-services/
[atlas]: https://github.com/Netflix/atlas

Roaring bitmaps are found to work well in many important applications:

> Use Roaring for bitmap compression whenever possible. Do not use other bitmap compression methods ([Wang et al., SIGMOD 2017](http://db.ucsd.edu/wp-content/uploads/2017/03/sidm338-wangA.pdf))


There is a serialized format specification for interoperability between implementations: https://github.com/RoaringBitmap/RoaringFormatSpec/

# Objective

The primary goal of the CRoaring is to provide a high performance low-level implementation that fully take advantage
of the latest hardware. Roaring bitmaps are already available on a variety of platform through Java, Go, Rust... implementations. CRoaring is a library that seeks to achieve superior performance by staying close to the latest hardware.


(c) 2016-2017 The CRoaring authors.



# Requirements

- The library should build on a  Linux-like operating system (including MacOS).
- We also support Microsoft Visual studio.
- Though most reasonable processors should be supported, we expect a recent Intel processor: Haswell (2013) or better but support all x64/x86 processors. The library builds without problem on ARM processors.
- Recent C compiler supporting the C11 standard (GCC 4.8 or better or clang), there is also an optional C++ class that requires a C++ compiler supporting the C++11 standard.
- CMake (to contribute to the project, users can rely on amalgamation/unity builds).
- clang-format (optional).

Serialization on big endian hardware may not be compatible with serialization on little endian hardware.

# Amalgamation/Unity Build

The CRoaring library can be amalgamated into a single source file that makes it easier
for integration into other projects. Moreover, by making it possible to compile
all the critical code into one compilation unit, it can improve the performance. For
the rationale, please see the SQLite documentation, https://www.sqlite.org/amalgamation.html,
or the corresponding Wikipedia entry (https://en.wikipedia.org/wiki/Single_Compilation_Unit).
Users who choose this route, do not need to rely on CRoaring's build system (based on CMake).

We maintain pre-generated amalgamated files at https://github.com/lemire/CRoaringUnityBuild for your convenience.

To generate the amalgamated files yourself, you can invoke a bash script...

```bash
./amalgamation.sh
```

(Bash shells are standard under Linux and macOS. Bash shells are available under Windows as part of the  [GitHub Desktop](https://desktop.github.com/) under the name ``Git Shell``. So if you have cloned the ``CRoaring`` GitHub repository from within the GitHub Desktop, you can right-click on ``CRoaring``, select ``Git Shell`` and then enter the above commands.)

It is not necessary to invoke the script in the CRoaring directory. You can invoke
it from any directory where you want the amalgamation files to be written.

It will generate three files for C users: ``roaring.h``, ``roaring.c`` and ``amalgamation_demo.c``... as well as some brief instructions. The ``amalgamation_demo.c`` file is a short example, whereas ``roaring.h`` and ``roaring.c`` are "amalgamated" files (including all source and header files for the project). This means that you can simply copy the files ``roaring.h`` and ``roaring.c`` into your project and be ready to go! No need to produce a library! See the ``amalgamation_demo.c`` file.

For example, you can use the C code as follows:
```
#include <stdio.h>
#include "roaring.c"
int main() {
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  for (uint32_t i = 100; i < 1000; i++) roaring_bitmap_add(r1, i);
  printf("cardinality = %d\n", (int) roaring_bitmap_get_cardinality(r1));
  roaring_bitmap_free(r1);
  return 0;
}
```

The script will also generate C++ files for C++ users, including an example. You can use the C++ as follows.

```
#include <iostream>
#include "roaring.hh"
#include "roaring.c"
int main() {
  Roaring r1;
  for (uint32_t i = 100; i < 1000; i++) {
    r1.add(i);
  }
  std::cout << "cardinality = " << r1.cardinality() << std::endl;

  Roaring64Map r2;
  for (uint64_t i = 18000000000000000100ull; i < 18000000000000001000ull; i++) {
    r2.add(i);
  }
  std::cout << "cardinality = " << r2.cardinality() << std::endl;
  return 0;
}
```

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
// we can also create iterator structs
counter = 0;
roaring_uint32_iterator_t *  i = roaring_create_iterator(r1);
while(i->has_value) {
   counter++; // could use    i->current_value
   roaring_advance_uint32_iterator(i);
}
roaring_free_uint32_iterator(i);
// roaring_bitmap_get_cardinality(r1) == counter

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

// we can also iterate the C++ way
counter = 0;
for(Roaring::const_iterator i = t.begin() ; i != t.end() ; i++) {
   ++counter;
}
// counter == t.cardinality()

```



# Building with cmake (Linux and macOS, Visual Studio users should see below)

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

By default, on all platforms, we build a dynamic library. You can generate a static library by adding ``-DBUILD_STATIC=ON`` to the command line.

As with all ``cmake`` projects, you can specify the compilers you wish to use by adding (for example) ``-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++`` to the ``cmake`` command line.



If wish to build an x64 version while disabling AVX2 and BMI2 support at the expense of performance, you can do the following :

```
mkdir -p buildnoavx
cd buildnoavx
cmake -DDISABLE_AVX=ON ..
make
```

The reverse is also possible. Some compilers may not enable AVX2 support, but you can force it in the following manner:

```
mkdir -p buildwithavx
cd buildwithavx
cmake -DFORCE_AVX=ON ..
make
```


If you have x64 hardware, but you wish to disable all x64-specific optimizations (including AVX), then you can
do the following...

```
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

# Building (Visual Studio under Windows)

We are assuming that you have a common Windows PC with at least Visual Studio 2015, and an x64 processor.

To build with at least Visual Studio 2015 from the command line:
- Grab the CRoaring code from GitHub, e.g., by cloning it using [GitHub Desktop](https://desktop.github.com/).
- Install [CMake](https://cmake.org/download/). When you install it, make sure to ask that ``cmake`` be made available from the command line.
- Create a subdirectory within CRoaring, such as ``VisualStudio``.
- Using a shell, go to this newly created directory. For example, within GitHub Desktop, you can right-click on  ``CRoaring`` in your GitHub repository list, and select ``Open in Git Shell``, then type ``cd VisualStudio`` in the newly created shell.
- Type ``cmake -DCMAKE_GENERATOR_PLATFORM=x64 ..`` in the shell while in the ``VisualStudio`` repository. (Alternatively, if you want to build a static library, you may use the command line ``cmake -DCMAKE_GENERATOR_PLATFORM=x64 -DBUILD_STATIC=ON  ..``.)
- This last command created a Visual Studio solution file in the newly created directory (e.g., ``RoaringBitmap.sln``). Open this file in Visual Studio. You should now be able to build the project and run the tests. For example, in the ``Solution Explorer`` window (available from the ``View`` menu), right-click ``ALL_BUILD`` and select ``Build``. To test the code, still in the ``Solution Explorer`` window, select ``RUN_TESTS`` and select ``Build``.

To build with at least Visual Studio 2017 directly in the IDE:
- Grab the CRoaring code from GitHub, e.g., by cloning it using [GitHub Desktop](https://desktop.github.com/).
- Select the ``Visual C++ tools for CMake`` optional component when installing the C++ Development Workload within Visual Studio.
- Within Visual Studio use ``File > Open > Folder...`` to open the CRoaring folder.
- Right click on ``CMakeLists.txt`` in the parent directory within ``Solution Explorer`` and select ``Build`` to build the project.
- For testing, in the Standard toolbar, drop the ``Select Startup Item...`` menu and choose one of the tests. Run the test by pressing the button to the left of the dropdown.


We have optimizations specific to AVX2 in the code, and they are turned only if the ``__AVX2__`` macro is defined. In turn, these optimizations should only be enabled if you know that your target machines will support AVX2. Given that all recent Intel and AMD processors support AVX2, you may want to make this assumption. Thankfully, Visual Studio does define the ``__AVX2__`` macro whenever the ``/arch:AVX2`` compiler option is set. Unfortunately, this option might not be set by default. Thankfully, you can enable it with CMake by adding the ``-DFORCE_AVX=ON`` flag (e.g., type ``cmake -DFORCE_AVX=ON -DCMAKE_GENERATOR_PLATFORM=x64 ..`` instead of  ``cmake -DCMAKE_GENERATOR_PLATFORM=x64 ..``). If you are building directly in the IDE (with at least Visual Studio 2017 and the Visual C++ tools for CMake component), then right click on ``CMakeLists.txt`` and select "Change CMake Settings". This opens a JSON file called ``CMakeSettings.json``. This file allows you to add CMake flags by editing the ``"cmakeCommandArgs"`` keys. [E.g., you can modify the lines that read ``"cmakeCommandArgs" : ""`` so that they become ``"cmakeCommandArgs" : "-DFORCE_AVX=ON"``.](https://goo.gl/photos/XH7peTKYRCSxWzph9) The relevant part of the JSON file might look at follows:

      {
        "name": "x64-Debug",
        "generator": "Visual Studio 15 2017 Win64",
        "configurationType": "Debug",
        "buildRoot": "${env.LOCALAPPDATA}\\CMakeBuild\\${workspaceHash}\\build\\${name}",
        "cmakeCommandArgs": "-DFORCE_AVX=ON",
        "buildCommandArgs": "-m -v:minimal"
      },
      {
        "name": "x64-Release",
        "generator": "Visual Studio 15 2017 Win64",
        "configurationType" : "Release",
        "buildRoot":  "${env.LOCALAPPDATA}\\CMakeBuild\\${workspaceHash}\\build\\${name}",
        "cmakeCommandArgs":  "-DFORCE_AVX=ON",
        "buildCommandArgs": "-m -v:minimal"
       }

After this modification, the output of CMake should include a line such as this one:

       CMAKE_C_FLAGS:   /arch:AVX2  -Wall

You must understand that this implies that the produced binaries will not run on hardware that does not support AVX2. However, you might get better performance.

We have additionnal optimizations that use inline assembly. However, Visual Studio does not support inline assembly so you cannot benefit from these optimizations under Visual Studio.


# Thread safety

Like, for example, STL containers or Java's default data structures, the CRoaring library has no built-in thread support. Thus whenever you modify a bitmap in one thread, it is unsafe to query it in others. It is safe however to query bitmaps (without modifying them) from several distinct threads,  as long as you do not use the copy-on-write attribute. For example, you can safely copy a bitmap and use both copies in concurrently. One should probably avoid the use of the copy-on-write attribute in a threaded environment.


# Python Wrapper

Tom Cornebize wrote a Python wrapper available at https://github.com/Ezibenroc/PyRoaringBitMap

# C# Wrapper

Brandon Smith wrote a C# wrapper available at https://github.com/RogueException/CRoaring.Net (works for Windows and Linux under x64 processors)


# Go (golang) Wrapper

There is a Go (golang) wrapper available at https://github.com/RoaringBitmap/gocroaring

# Rust Wrapper

Saulius Grigaliunas wrote a Rust wrapper available at https://github.com/saulius/croaring-rs


# Redis Module

Antonio Guilherme Ferreira Viggiano wrote a Redis Module available at https://github.com/aviggiano/redis-roaring


# References and further reading

-  Array layouts for comparison-based searching http://arxiv.org/pdf/1509.05053.pdf
-  Schlegel et al., Fast Sorted-Set Intersection using SIMD Instructions


# Mailing list/discussion group

https://groups.google.com/forum/#!forum/roaring-bitmaps

# References about Roaring

-  Samy Chambi, Daniel Lemire, Owen Kaser, Robert Godin,
Better bitmap performance with Roaring bitmaps,
Software: Practice and Experience Volume 46, Issue 5, pages 709–719, May 2016
http://arxiv.org/abs/1402.6407 This paper used data from http://lemire.me/data/realroaring2014.html
- Daniel Lemire, Gregory Ssi-Yan-Kai, Owen Kaser, Consistently faster and smaller compressed bitmaps with Roaring, Software: Practice and Experience (accepted in 2016, to appear) http://arxiv.org/abs/1603.06549
- Samy Chambi, Daniel Lemire, Robert Godin, Kamel Boukhalfa, Charles Allen, Fangjin Yang, Optimizing Druid with Roaring bitmaps, IDEAS 2016, 2016. http://r-libre.teluq.ca/950/
