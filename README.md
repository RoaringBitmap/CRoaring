# CRoaring
Roaring bitmaps in C

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

The primary goal of the CRoaring is to provide a high performance low-level implementation that fully take advantage
of the latest hardware.

# Requirements

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

To run unit tests:

```
make test
```

To run real-data benchmark

```
./real_bitmaps_benchmark ../benchmarks/realdata/census1881
```

To check that your code abides by the style convention (make sure that ``clang-format`` is installed):

```
./tools/clang-format-check.sh
```

To reformat your code according to the style convention (make sure that ``clang-format`` is installed):

```
./tools/clang-format.sh
```

# sanity todo
- get the code to compile cleanly with -Wconversion and possibly -Weverything
- get everything to work with valgrind cleanly
- get everything to work cleanly with other static checkers, sanitizers and so forth

```
-fsanitize=address -fno-omit-frame-pointer
-fsanitize=memory  -fno-omit-frame-pointer
-fsanitize=undefined
-fsanitize=dataflow
-fsanitize=cfi -flto
-fsanitize=safe-stack
```
- Daniel


# todo

* consider LTO (Link Time Optimization)

# References and further reading

-  Array layouts for comparison-based searching http://arxiv.org/pdf/1509.05053.pdf
-  Schlegel et al., Fast Sorted-Set Intersection using SIMD Instructions

# Issues to consider

AVX operations take a while before they warm up to their best speed 
as documented by Agner Fog and others.

There is a trade-off between throughput and latency. For example,
prefetching might improve latency, but at the expense of throughput
on a multicore system.

Some instructions, like POPCNT,  take a serious hit under hyperthreading.
