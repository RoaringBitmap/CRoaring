#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include <roaring/misc/configreport.h>
#include <roaring/roaring.h>

// We are mostly running this test to check for data races using thread
// sanitizer.
void run(roaring_bitmap_t **rarray) {
    for (size_t i = 0; i < 100; i++) {
        roaring_bitmap_t *r1 = roaring_bitmap_copy(rarray[0]);
        roaring_bitmap_t *r2 = roaring_bitmap_copy(rarray[1]);
        roaring_bitmap_t *r3 = roaring_bitmap_copy(rarray[2]);
        roaring_bitmap_and_inplace(r1, r2);
        roaring_bitmap_andnot_inplace(r1, r3);
        roaring_bitmap_free(r1);
        roaring_bitmap_free(r2);
        roaring_bitmap_free(r3);
    }
}

bool run_threads_unit_tests() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();

    for (uint32_t i = 0; i < 50000; i++) {
        if (i != 300) {
            roaring_bitmap_add(r1, 65536 + i);
        }
    }
    for (uint32_t i = 50000; i < 150000; i++) {
        if ((i % 500) == 0) {
            roaring_bitmap_add(r1, i);
        }
    }
    for (uint32_t i = 150000; i < 200000; i++) {
        if ((i % 2) == 0) {
            roaring_bitmap_add(r1, i);
        }
    }

    roaring_bitmap_set_copy_on_write(r1, true);
    roaring_bitmap_run_optimize(r1);
    roaring_bitmap_t *r2 =
        roaring_bitmap_from(10010, 10020, 10030, 10040, 10050);
    roaring_bitmap_set_copy_on_write(r2, true);
    roaring_bitmap_t *r3 = roaring_bitmap_copy(r1);
    roaring_bitmap_set_copy_on_write(r3, true);

    roaring_bitmap_t * r1a = roaring_bitmap_copy(r1);
    roaring_bitmap_t * r1b = roaring_bitmap_copy(r1);

    roaring_bitmap_t * r2a = roaring_bitmap_copy(r2);
    roaring_bitmap_t * r2b = roaring_bitmap_copy(r2);

    roaring_bitmap_t * r3a = roaring_bitmap_copy(r3);
    roaring_bitmap_t * r3b = roaring_bitmap_copy(r3);

    roaring_bitmap_t *rarray1[3] = {r1a, r2a, r3a};
    roaring_bitmap_t *rarray2[3] = {r1b, r2b, r3b};
    std::thread thread1(run, rarray1);
    std::thread thread2(run, rarray2);
    thread1.join();
    thread2.join();
    roaring_bitmap_free(r1);
    roaring_bitmap_free(r2);
    roaring_bitmap_free(r3);
    roaring_bitmap_free(r1a);
    roaring_bitmap_free(r2a);
    roaring_bitmap_free(r3a);
    roaring_bitmap_free(r1b);
    roaring_bitmap_free(r2b);
    roaring_bitmap_free(r3b);
    return true;
}

int main() {
    roaring::misc::tellmeall();
    bool is_ok = run_threads_unit_tests();
    if (is_ok) {
        printf("code run completed.\n");
    }
    return is_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
