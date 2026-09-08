#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include <roaring/memory.h>
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

    roaring_bitmap_t *r1a = roaring_bitmap_copy(r1);
    roaring_bitmap_t *r1b = roaring_bitmap_copy(r1);

    roaring_bitmap_t *r2a = roaring_bitmap_copy(r2);
    roaring_bitmap_t *r2b = roaring_bitmap_copy(r2);

    roaring_bitmap_t *r3a = roaring_bitmap_copy(r3);
    roaring_bitmap_t *r3b = roaring_bitmap_copy(r3);

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

namespace {

// Regression test for https://github.com/RoaringBitmap/CRoaring/issues/876:
// cloning a shared container must not race with another thread releasing its
// own reference to it. The memory hooks let us force the interleaving: the
// cloning thread is paused when the clone allocates, and the other thread
// frees its bitmap at that exact moment.

std::mutex race_mutex;
std::condition_variable race_cv;
bool freer_may_go = false;
bool freer_done = false;
bool clone_intercepted = false;
thread_local bool intercept_armed = false;

const std::chrono::seconds race_timeout(30);

void *race_malloc(size_t size) {
    if (intercept_armed && !clone_intercepted) {
        clone_intercepted = true;  // intercept the first allocation only
        std::unique_lock<std::mutex> lock(race_mutex);
        freer_may_go = true;
        race_cv.notify_all();
        race_cv.wait_for(lock, race_timeout, [] { return freer_done; });
    }
    return malloc(size);
}

void *race_aligned_malloc(size_t alignment, size_t size) {
    void *p;
#ifdef _MSC_VER
    p = _aligned_malloc(size, alignment);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    p = __mingw_aligned_malloc(size, alignment);
#else
    if (posix_memalign(&p, alignment, size) != 0) return NULL;
#endif
    return p;
}

void race_aligned_free(void *p) {
#ifdef _MSC_VER
    _aligned_free(p);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    __mingw_aligned_free(p);
#else
    free(p);
#endif
}

}  // namespace

bool run_shared_container_race_test() {
    roaring_memory_t hooks = {
        race_malloc,         realloc,          calloc, free,
        race_aligned_malloc, race_aligned_free};
    roaring_init_memory_hook(hooks);

    roaring_bitmap_t *r1 = roaring_bitmap_create();
    for (uint32_t i = 0; i < 10000; i++) {
        roaring_bitmap_add(r1, 2 * i + 1);
    }
    roaring_bitmap_set_copy_on_write(r1, true);
    roaring_bitmap_t *r2 = roaring_bitmap_copy(r1);

    std::thread modifier([r1] {
        intercept_armed = true;
        roaring_bitmap_add(r1, 2);  // clones the shared container
        intercept_armed = false;
    });
    std::thread freer([r2] {
        std::unique_lock<std::mutex> lock(race_mutex);
        race_cv.wait_for(lock, race_timeout, [] { return freer_may_go; });
        lock.unlock();
        roaring_bitmap_free(r2);  // drops the other reference
        lock.lock();
        freer_done = true;
        race_cv.notify_all();
    });
    modifier.join();
    freer.join();

    bool is_ok = true;
    if (!clone_intercepted) {
        printf("the shared container was never cloned, test is ineffective\n");
        is_ok = false;
    }
    if (!roaring_bitmap_contains(r1, 2) ||
        roaring_bitmap_get_cardinality(r1) != 10001) {
        printf("the bitmap was not correctly modified\n");
        is_ok = false;
    }
    const char *reason = NULL;
    if (!roaring_bitmap_internal_validate(r1, &reason)) {
        printf("the bitmap is invalid: %s\n", reason);
        is_ok = false;
    }
    roaring_bitmap_free(r1);
    return is_ok;
}

int main() {
    roaring::misc::tellmeall();
    bool is_ok = run_threads_unit_tests();
    is_ok = run_shared_container_race_test() && is_ok;
    if (is_ok) {
        printf("code run completed.\n");
    }
    return is_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
