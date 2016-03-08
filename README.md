# CRoaring
Roaring bitmaps in C

Bitsets, also called bitmaps, are commonly used as fast data structures. Unfortunately, they can use too much memory.
 To compensate, we often use compressed bitmaps.

Roaring bitmaps are compressed bitmaps which tend to outperform conventional compressed bitmaps such as WAH, EWAH or Concise.
They are used by several major systems such as Apache Lucene and derivative systems such as Solr and Elastic,
Metamarkets' Druid, Apache Spark, Whoosh and eBay's Apache Kylin.

The primary goal of the CRoaring is to provide a high performance low-level implementation that fully take advantage
of the latest hardware.

# Requirements

- Recent Intel processor: Haswell (2013) or better.
- Recent C compiler (GCC 4.8 or better)

Support for legacy hardware and compiler might be added later.

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

- consider LTO (Link Time Optimization)
- implement SIMD galloping in arrays
- implement SIMD bit decoding for bitset
- implement SIMD binary search

# Building

You can use ```make NOAVXTUNING=1``` to build the code
without too much hand-tuning, relying instead of what
the compiler could produce (e.g., auto-vectorization).

# References and further reading

Branchless bin. search can outdo branchy bin. search 
as long as you can add prefetching.

See  Array layouts for comparison-based searching http://arxiv.org/pdf/1509.05053.pdf


String SIMD instructions can be used to intersect arrays of
16-bit integers.

See Schlegel et al., Fast Sorted-Set Intersection using SIMD Instructions

# Issues to consider

There is a trade-off between throughput and latency. For example, 
prefetching might improve latency, but at the expense of throughput
on a multicore system.

Some instructions, like POPCNT, can run on only one core which means
that they can take a serious hit under hyperthreading.


