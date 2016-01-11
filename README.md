# CRoaring
Roaring bitmaps in C

# todo

- consider LTO (Link Time Optimization)


# Building

You can use ```make NOAVXTUNING=1``` to build the code
without too much hand-tuning, relying instead of what
the compiler could produce (e.g., auto-vectorization).
