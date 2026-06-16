// This file is the "How to use the library? / The C API" example from the
// project README.md. It is part of the test suite so that the documented
// example is guaranteed to compile and run. Keep it in sync with the README.
//
// We undefine NDEBUG so that the assert() checks below are always active,
// even when the test suite is built in release mode.
#undef NDEBUG

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/roaring.h>
#include <roaring/roaring64.h>

int main(void) {
    // --- 32-bit bitmaps ---
    // Create an empty bitmap and add a few values.
    roaring_bitmap_t *bitmap = roaring_bitmap_create();
    roaring_bitmap_add(bitmap, 1);
    roaring_bitmap_add(bitmap, 100);
    roaring_bitmap_add(bitmap, 1000);
    roaring_bitmap_add_range(bitmap, 10,
                             20);  // adds the half-open range [10, 20)

    // Query the bitmap.
    assert(roaring_bitmap_contains(bitmap, 100));
    assert(!roaring_bitmap_contains(bitmap, 50));
    printf("32-bit cardinality = %d\n",
           (int)roaring_bitmap_get_cardinality(bitmap));

    // Optionally compress runs of consecutive values for a smaller footprint.
    roaring_bitmap_run_optimize(bitmap);

    // Set operations return a new bitmap that you own and must free.
    roaring_bitmap_t *other = roaring_bitmap_from(100, 1000, 5000);
    roaring_bitmap_t *intersection = roaring_bitmap_and(bitmap, other);
    assert(roaring_bitmap_get_cardinality(intersection) == 2);  // {100, 1000}

    // Iterate over the values in sorted (increasing) order.
    roaring_uint32_iterator_t *it = roaring_iterator_create(bitmap);
    while (it->has_value) {
        // do something with it->current_value
        roaring_uint32_iterator_advance(it);
    }
    roaring_uint32_iterator_free(it);

    roaring_bitmap_free(intersection);
    roaring_bitmap_free(other);
    roaring_bitmap_free(bitmap);

    // --- 64-bit bitmaps (same ideas, but with 64-bit values) ---
    roaring64_bitmap_t *big = roaring64_bitmap_create();
    roaring64_bitmap_add(big, 1);
    roaring64_bitmap_add(big, 0xFFFFFFFFFFULL);  // a value beyond 32 bits
    assert(roaring64_bitmap_contains(big, 0xFFFFFFFFFFULL));
    printf("64-bit cardinality = %d\n",
           (int)roaring64_bitmap_get_cardinality(big));
    roaring64_bitmap_free(big);

    return EXIT_SUCCESS;
}
