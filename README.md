# CRoaring
Roaring bitmaps in C

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


