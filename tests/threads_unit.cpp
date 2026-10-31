#include <atomic>
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

// Operations that share a copy-on-write input's containers must leave the
// slots that are already shared untouched. Then a bitmap whose containers are
// all shared can be read while another thread computes from it; thread
// sanitizer reports a race if any of these operations writes to it.
bool run_shared_input_test() {
    // One value in each container with a key in [begin, end).
    auto bitmap_with_keys = [](uint32_t begin, uint32_t end) {
        roaring_bitmap_t *r = roaring_bitmap_create();
        for (uint32_t key = begin; key < end; key++) {
            roaring_bitmap_add(r, key * 65536 + 1);
        }
        roaring_bitmap_set_copy_on_write(r, true);
        return r;
    };
    roaring_bitmap_t *r = bitmap_with_keys(0, 10);
    roaring_bitmap_t *copy = roaring_bitmap_copy(r);  // shares every container
    // Keys only in r come before, between and after the keys of other.
    roaring_bitmap_t *other = bitmap_with_keys(3, 8);
    // Many more containers than r, for the in-place union of a few containers.
    roaring_bitmap_t *large = bitmap_with_keys(3, 400);
    roaring_bitmap_t *empty = bitmap_with_keys(0, 0);

    // The reader runs for as long as the other thread computes.
    std::atomic<bool> reading(false);
    std::atomic<bool> done(false);
    bool reader_ok = true;
    std::thread reader([r, &reading, &done, &reader_ok] {
        reading = true;
        do {
            if (roaring_bitmap_get_cardinality(r) != 10) {
                reader_ok = false;
            }
        } while (!done);
    });
    std::thread computer([r, other, large, empty, &reading, &done] {
        while (!reading) {
        }
        for (size_t i = 0; i < 100; i++) {
            roaring_bitmap_t *results[] = {
                // Copying and empty-input paths also share through ra_overwrite.
                roaring_bitmap_copy(r),
                roaring_bitmap_or(r, empty),
                roaring_bitmap_or(empty, r),
                roaring_bitmap_xor(r, empty),
                roaring_bitmap_xor(empty, r),
                roaring_bitmap_andnot(r, empty),
                roaring_bitmap_lazy_or(r, empty, false),
                roaring_bitmap_lazy_or(empty, r, false),
                roaring_bitmap_lazy_xor(r, empty),
                roaring_bitmap_lazy_xor(empty, r),
                roaring_bitmap_or(r, other),
                roaring_bitmap_or(other, r),
                roaring_bitmap_xor(r, other),
                roaring_bitmap_xor(other, r),
                roaring_bitmap_lazy_or(r, other, false),
                roaring_bitmap_lazy_or(other, r, false),
                roaring_bitmap_lazy_xor(r, other),
                roaring_bitmap_lazy_xor(other, r),
                roaring_bitmap_andnot(r, other),
                roaring_bitmap_flip(r, 5 * 65536, 6 * 65536),
                roaring_bitmap_add_offset(r, 65536),
            };
            for (roaring_bitmap_t *result : results) {
                roaring_bitmap_free(result);
            }
            roaring_bitmap_t *targets[] = {
                roaring_bitmap_copy(other), roaring_bitmap_copy(other),
                roaring_bitmap_copy(other), roaring_bitmap_copy(other),
                roaring_bitmap_copy(large), roaring_bitmap_create(),
            };
            roaring_bitmap_or_inplace(targets[0], r);
            roaring_bitmap_xor_inplace(targets[1], r);
            roaring_bitmap_lazy_or_inplace(targets[2], r, false);
            roaring_bitmap_lazy_xor_inplace(targets[3], r);
            roaring_bitmap_or_inplace(targets[4], r);
            roaring_bitmap_overwrite(targets[5], r);
            for (roaring_bitmap_t *target : targets) {
                roaring_bitmap_free(target);
            }
        }
        done = true;
    });
    reader.join();
    computer.join();

    bool is_ok = true;
    if (!reader_ok) {
        printf("the shared bitmap changed while it was read\n");
        is_ok = false;
    }
    const char *reason = NULL;
    if (!roaring_bitmap_internal_validate(r, &reason)) {
        printf("the shared bitmap is invalid: %s\n", reason);
        is_ok = false;
    }
    roaring_bitmap_free(r);
    roaring_bitmap_free(copy);
    roaring_bitmap_free(other);
    roaring_bitmap_free(large);
    roaring_bitmap_free(empty);
    return is_ok;
}

int main() {
    roaring::misc::tellmeall();
    bool is_ok = run_threads_unit_tests();
    is_ok = run_shared_container_race_test() && is_ok;
    is_ok = run_shared_input_test() && is_ok;
    if (is_ok) {
        printf("code run completed.\n");
    }
    return is_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
