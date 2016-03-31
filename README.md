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
- Recent Intel processor: Haswell (2013) or better.
- Recent C compiler (GCC 4.8 or better)
- CMake
- clang-format (optional)

Support for legacy hardware and compiler might be added later.

# Building

CRoaring follows the standard cmake workflow:

```
mkdir build
cd build
cmake ..
make
```

For debug release, try

```
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

To turn on sanitizer flags, try
```
cmake -DSANITIZE=1
```


To run unit tests:

```
make test
```

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
Software: Practice and Experience (accepted in 2015, to appear)
http://arxiv.org/abs/1402.6407 This paper used data from http://lemire.me/data/realroaring2014.html
- Daniel Lemire, Gregory Ssi-Yan-Kai, Owen Kaser, Consistently faster and smaller compressed bitmaps with Roaring, Software: Practice and Experience (accepted in 2016, to appear) http://arxiv.org/abs/1603.06549

# Issues to consider

AVX operations take a while before they warm up to their best speed
as documented by Agner Fog and others.

There is a trade-off between throughput and latency. For example,
prefetching might improve latency, but at the expense of throughput
on a multicore system.

Some instructions, like POPCNT,  take a serious hit under hyperthreading.
