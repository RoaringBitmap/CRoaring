#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/misc/configreport.h>
#include <roaring/roaring.h>

#include "test.h"

bool roaring_iterator_sumall(uint32_t value, void *param) {
    *(uint32_t *)param += value;
    return true;  // iterate till the end
}

int main() {
    tellmeall();
    // create a new empty bitmap
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    // then we can add values
    for (uint32_t i = 100; i < 1000; i++) roaring_bitmap_add(r1, i);
    // check whether a value is contained
    assert_true(roaring_bitmap_contains(r1, 500));
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
    roaring_bitmap_t *r2 = roaring_bitmap_from(1, 2, 3, 5, 6);
    roaring_bitmap_printf(r2);  // print it

    // we can also create a bitmap from a pointer to 32-bit integers
    uint32_t somevalues[] = {2, 3, 4};
    roaring_bitmap_t *r3 = roaring_bitmap_of_ptr(3, somevalues);

    // we can also go in reverse and go from arrays to bitmaps
    uint64_t card1 = roaring_bitmap_get_cardinality(r1);
    uint32_t *arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    assert_true(arr1 != NULL);
    roaring_bitmap_to_uint32_array(r1, arr1);
    roaring_bitmap_t *r1f = roaring_bitmap_of_ptr(card1, arr1);
    free(arr1);
    assert_true(roaring_bitmap_equals(r1, r1f));  // what we recover is equal
    roaring_bitmap_free(r1f);

    // we can go from arrays to bitmaps from "offset" by "limit"
    size_t offset = 100;
    size_t limit = 1000;
    uint32_t *arr3 = (uint32_t *)malloc(limit * sizeof(uint32_t));
    assert_true(arr3 != NULL);
    roaring_bitmap_range_uint32_array(r1, offset, limit, arr3);
    free(arr3);

    // we can copy and compare bitmaps
    roaring_bitmap_t *z = roaring_bitmap_copy(r3);
    assert_true(roaring_bitmap_equals(r3, z));  // what we recover is equal
    roaring_bitmap_free(z);

    // we can compute union two-by-two
    roaring_bitmap_t *r1_2_3 = roaring_bitmap_or(r1, r2);
    roaring_bitmap_or_inplace(r1_2_3, r3);

    // we can compute a big union
    const roaring_bitmap_t *allmybitmaps[] = {r1, r2, r3};
    roaring_bitmap_t *bigunion = roaring_bitmap_or_many(3, allmybitmaps);
    assert_true(
        roaring_bitmap_equals(r1_2_3, bigunion));  // what we recover is equal
    // can also do the big union with a heap
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
    uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r1);
    char *serializedbytes = (char *)malloc(expectedsize);
    roaring_bitmap_portable_serialize(r1, serializedbytes);
    roaring_bitmap_t *t =
        roaring_bitmap_portable_deserialize_safe(serializedbytes, expectedsize);
    if (t == NULL) {
        return EXIT_FAILURE;
    }
    const char *reason = NULL;
    if (!roaring_bitmap_internal_validate(t, &reason)) {
        return EXIT_FAILURE;
    }
    assert_true(roaring_bitmap_equals(r1, t));  // what we recover is equal
    roaring_bitmap_free(t);
    // we can also check whether there is a bitmap at a memory location without
    // reading it
    size_t sizeofbitmap =
        roaring_bitmap_portable_deserialize_size(serializedbytes, expectedsize);
    printf("\nsizeofbitmap = %zu \n", sizeofbitmap);
    assert_true(
        sizeofbitmap ==
        expectedsize);  // sizeofbitmap would be zero if no bitmap were found
    // we can also read the bitmap "safely" by specifying a byte size limit:
    t = roaring_bitmap_portable_deserialize_safe(serializedbytes, expectedsize);
    if (t == NULL) {
        printf("Problem during deserialization.\n");
        // We could clear any memory and close any file here.
        return EXIT_FAILURE;
    }
    // We can validate the bitmap we recovered to make sure it is proper.
    const char *reason_failure = NULL;
    if (!roaring_bitmap_internal_validate(t, &reason_failure)) {
        printf("safely deserialized invalid bitmap: %s\n", reason_failure);
        // We could clear any memory and close any file here.
        return EXIT_FAILURE;
    }
    assert_true(roaring_bitmap_equals(r1, t));  // what we recover is equal
    roaring_bitmap_free(t);

    free(serializedbytes);

    // we can iterate over all values using custom functions
    uint32_t counter = 0;
    roaring_iterate(r1, roaring_iterator_sumall, &counter);

    // we can also create iterator structs
    counter = 0;
    roaring_uint32_iterator_t *i = roaring_iterator_create(r1);
    while (i->has_value) {
        counter++;  // could use    i->current_value
        roaring_uint32_iterator_advance(i);
    }
    // you can skip over values and move the iterator with
    // roaring_uint32_iterator_move_equalorlarger(i,someintvalue)

    roaring_uint32_iterator_free(i);
    // roaring_bitmap_get_cardinality(r1) == counter

    // for greater speed, you can iterate over the data in bulk
    i = roaring_iterator_create(r1);
    uint32_t buffer[256];
    while (1) {
        uint32_t ret = roaring_uint32_iterator_read(i, buffer, 256);
        for (uint32_t j = 0; j < ret; j++) {
            counter += buffer[j];
        }
        if (ret < 256) {
            break;
        }
    }
    roaring_uint32_iterator_free(i);

    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r3);
    printf("Success.\n");
    return EXIT_SUCCESS;
}