/*
 * array.c
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>

#include "run.h"

extern bool run_container_is_full(const run_container_t *run);
extern bool run_container_nonzero_cardinality(const run_container_t *r);
extern void run_container_clear(run_container_t *run);
extern int32_t run_container_serialized_size_in_bytes(int32_t num_runs);

enum { DEFAULT_INIT_SIZE = 4 };

/* Create a new run container. Return NULL in case of failure. */
run_container_t *run_container_create_given_capacity(int32_t size) {
    run_container_t *run;
    /* Allocate the run container itself. */
    if ((run = malloc(sizeof(run_container_t))) == NULL) {
        return NULL;
    }
    if ((run->runs = malloc(sizeof(rle16_t) * size)) == NULL) {
        free(run);
        return NULL;
    }
    run->capacity = size;
    run->n_runs = 0;
    return run;
}

/* Create a new run container. Return NULL in case of failure. */
run_container_t *run_container_create() {
    return run_container_create_given_capacity(DEFAULT_INIT_SIZE);
}

run_container_t *run_container_clone(const run_container_t *src) {
    run_container_t *run = run_container_create_given_capacity(src->capacity);
    if (run == NULL) return NULL;
    run->capacity = src->capacity;
    run->n_runs = src->n_runs;
    memcpy(run->runs, src->runs, src->n_runs * sizeof(rle16_t));
    return run;
}

/* Free memory. */
void run_container_free(run_container_t *run) {
    free(run->runs);
    run->runs = NULL;  // pedantic
    free(run);
}

/* Get the cardinality of `run'. Requires an actual computation. */
int run_container_cardinality(const run_container_t *run) {
    const int32_t n_runs = run->n_runs;
    const rle16_t *runs = run->runs;

    /* by initializing with n_runs, we omit counting the +1 for each pair. */
    int sum = n_runs;
    for (int k = 0; k < n_runs; ++k) {
        sum += runs[k].length;
    }

    return sum;
}

// with some luck: sizeof(struct valuelength_s) = 2 *sizeof(uint16_t) = 4
_Static_assert(sizeof(rle16_t) == 2 * sizeof(uint16_t),
               "Bad struct size");  // part of C standard

// TODO: could be more efficient
void run_container_append(run_container_t *run, rle16_t vl) {
    if (run->n_runs == 0) {
        run->runs[run->n_runs] = vl;
        run->n_runs++;
        return;
    }
    const uint32_t previousend =
        run->runs[run->n_runs - 1].value + run->runs[run->n_runs - 1].length;
    if (vl.value > previousend + 1) {  // we add a new one
        run->runs[run->n_runs] = vl;
        run->n_runs++;
        return;
    }
    uint32_t newend = vl.value + vl.length + UINT32_C(1);
    if (newend > previousend) {  // we merge
        run->runs[run->n_runs - 1].length =
            newend - 1 - run->runs[run->n_runs - 1].value;
    }
}

void run_container_append_value(run_container_t *run, uint16_t val) {
    if (run->n_runs == 0) {
        run->runs[run->n_runs] = (rle16_t){.value = val, .length = 0};
        run->n_runs++;
        return;
    }
    const uint32_t previousend =
        run->runs[run->n_runs - 1].value + run->runs[run->n_runs - 1].length;
    if (val > previousend + 1) {  // we add a new one
        run->runs[run->n_runs] = (rle16_t){.value = val, .length = 0};
        run->n_runs++;
        return;
    }
    if (val == previousend + 1) {  // we merge
        run->runs[run->n_runs - 1].length++;
    }
}

void run_container_grow(run_container_t *run, int32_t min, bool copy) {
    int32_t newCapacity =
        (run->capacity == 0)
            ? DEFAULT_INIT_SIZE
            : run->capacity < 64 ? run->capacity * 2
                                 : run->capacity < 1024 ? run->capacity * 3 / 2
                                                        : run->capacity * 5 / 4;
    if (newCapacity < min) newCapacity = min;
    run->capacity = newCapacity;
    assert(run->capacity >= min);
    if (copy) {
        rle16_t *oldruns = run->runs;
        run->runs = realloc(oldruns, run->capacity * sizeof(rle16_t));
        if (run->runs == NULL) free(oldruns);
    } else {
        free(run->runs);
        run->runs = malloc(run->capacity * sizeof(rle16_t));
    }
    // TODO: handle the case where realloc fails
    if (run->runs == NULL) {
        printf(
            "Well, that's unfortunate. Did I say you could use this code in "
            "production?\n");
    }
}
static inline void makeRoomAtIndex(run_container_t *run, uint16_t index) {
    /* This function calls realloc + memmove sequentially to move by one index.
     * Potentially copying twice the array.
     */
    if (run->n_runs + 1 > run->capacity)
        run_container_grow(run, run->n_runs + 1, true);
    memmove(run->runs + 1 + index, run->runs + index,
            (run->n_runs - index) * sizeof(rle16_t));
    run->n_runs++;
}

static inline void recoverRoomAtIndex(run_container_t *run, uint16_t index) {
    memmove(run->runs + index, run->runs + (1 + index),
            (run->n_runs - index - 1) * sizeof(rle16_t));
    run->n_runs--;
}

/* copy one container into another */
void run_container_copy(const run_container_t *src, run_container_t *dst) {
    const int32_t n_runs = src->n_runs;
    if (src->n_runs > dst->capacity) {
        run_container_grow(dst, n_runs, false);
    }
    dst->n_runs = n_runs;
    memcpy(dst->runs, src->runs, sizeof(rle16_t) * n_runs);
}

#ifdef RUNBRANCHLESSBINSEARCH

/**
* the branchless approach is inspired by
*  Array layouts for comparison-based searching
*  http://arxiv.org/pdf/1509.05053.pdf
*/
// could potentially use SIMD-based bin. search
// values are interleaved with lengths
static int32_t interleavedBinarySearch(const rle16_t *source, int32_t n,
                                       uint16_t target) {
    const rle16_t *base = source;
    if (n == 0) return -1;
    if (target > source[n - 1].value)
        return -n - 1;  // without this, buffer overrun
    while (n > 1) {
        int32_t half = n >> 1;
        base = (base[half].value < target) ? base + half : base;
        n -= half;
    }
    base += (base->value < target);
    return (base->value == target) ? (base - source) : (source - base) - 1;
}
#else
// good old bin. search
static int32_t interleavedBinarySearch(const rle16_t *array, int32_t lenarray,
                                       uint16_t ikey) {
    int32_t low = 0;
    int32_t high = lenarray - 1;
    while (low <= high) {
        int32_t middleIndex = (low + high) >> 1;
        uint16_t middleValue = array[middleIndex].value;
        if (middleValue < ikey) {
            low = middleIndex + 1;
        } else if (middleValue > ikey) {
            high = middleIndex - 1;
        } else {
            return middleIndex;
        }
    }
    return -(low + 1);
}
#endif

/* Add `pos' to `run'. Returns true if `pos' was not present. */
bool run_container_add(run_container_t *run, uint16_t pos) {
    int32_t index = interleavedBinarySearch(run->runs, run->n_runs, pos);
    if (index >= 0) return false;  // already there
    index = -index - 2;            // points to preceding value, possibly -1
    if (index >= 0) {              // possible match
        int32_t offset = pos - run->runs[index].value;
        int32_t le = run->runs[index].length;
        if (offset <= le) return false;  // already there
        if (offset == le + 1) {
            // we may need to fuse
            if (index + 1 < run->n_runs) {
                if (run->runs[index + 1].value == pos + 1) {
                    // indeed fusion is needed
                    run->runs[index].length = run->runs[index + 1].value +
                                              run->runs[index + 1].length -
                                              run->runs[index].value;
                    recoverRoomAtIndex(run, index + 1);
                    return true;
                }
            }
            run->runs[index].length++;
            return true;
        }
        if (index + 1 < run->n_runs) {
            // we may need to fuse
            if (run->runs[index + 1].value == pos + 1) {
                // indeed fusion is needed
                run->runs[index + 1].value = pos;
                run->runs[index + 1].length = run->runs[index + 1].length + 1;
                return true;
            }
        }
    }
    if (index == -1) {
        // we may need to extend the first run
        if (0 < run->n_runs) {
            if (run->runs[0].value == pos + 1) {
                run->runs[0].length++;
                run->runs[0].value--;
                return true;
            }
        }
    }
    makeRoomAtIndex(run, index + 1);
    run->runs[index + 1].value = pos;
    run->runs[index + 1].length = 0;
    return true;
}

/* Remove `pos' from `run'. Returns true if `pos' was present. */
bool run_container_remove(run_container_t *run, uint16_t pos) {
    int32_t index = interleavedBinarySearch(run->runs, run->n_runs, pos);
    if (index >= 0) {
        int32_t le = run->runs[index].length;
        if (le == 0) {
            recoverRoomAtIndex(run, index);
        } else {
            run->runs[index].value++;
            run->runs[index].length--;
        }
        return true;
    }
    index = -index - 2;  // points to preceding value, possibly -1
    if (index >= 0) {    // possible match
        int32_t offset = pos - run->runs[index].value;
        int32_t le = run->runs[index].length;
        if (offset < le) {
            // need to break in two
            run->runs[index].length = offset - 1;
            // need to insert
            uint16_t newvalue = pos + 1;
            int32_t newlength = le - offset - 1;
            makeRoomAtIndex(run, index + 1);
            run->runs[index + 1].value = newvalue;
            run->runs[index + 1].length = newlength;
            return true;

        } else if (offset == le) {
            run->runs[index].length--;
            return true;
        }
    }
    // no match
    return false;
}

/* Check whether `pos' is present in `run'.  */
bool run_container_contains(const run_container_t *run, uint16_t pos) {
    int32_t index = interleavedBinarySearch(run->runs, run->n_runs, pos);
    if (index >= 0) return true;
    index = -index - 2;  // points to preceding value, possibly -1
    if (index != -1) {   // possible match
        int32_t offset = pos - run->runs[index].value;
        int32_t le = run->runs[index].length;
        if (offset <= le) return true;
    }
    return false;
}

/* Compute the union of `src_1' and `src_2' and write the result to `dst'
 * It is assumed that `dst' is distinct from both `src_1' and `src_2'. */
void run_container_union(const run_container_t *src_1,
                         const run_container_t *src_2, run_container_t *dst) {
    // TODO: this could be a lot more efficient

    // we start out with inexpensive checks
    const bool if1 = run_container_is_full(src_1);
    const bool if2 = run_container_is_full(src_2);
    if (if1 || if2) {
        if (if1) {
            run_container_copy(src_1, dst);
            return;
        }
        if (if2) {
            run_container_copy(src_2, dst);
            return;
        }
    }
    const int32_t neededcapacity = src_1->n_runs + src_2->n_runs;
    if (dst->capacity < neededcapacity)
        run_container_grow(dst, neededcapacity, false);
    dst->n_runs = 0;
    int32_t rlepos = 0;
    int32_t xrlepos = 0;

    while ((xrlepos < src_2->n_runs) && (rlepos < src_1->n_runs)) {
        if (src_1->runs[rlepos].value <= src_2->runs[xrlepos].value) {
            run_container_append(dst, src_1->runs[rlepos]);
            rlepos++;
        } else {
            run_container_append(dst, src_2->runs[xrlepos]);
            xrlepos++;
        }
    }
    while (xrlepos < src_2->n_runs) {
        run_container_append(dst, src_2->runs[xrlepos]);
        xrlepos++;
    }
    while (rlepos < src_1->n_runs) {
        run_container_append(dst, src_1->runs[rlepos]);
        rlepos++;
    }
}

/* Compute the union of `src_1' and `src_2' and write the result to `src_1'
 */
void run_container_union_inplace(run_container_t *src_1,
                                 const run_container_t *src_2) {
    // TODO: this could be a lot more efficient

    // we start out with inexpensive checks
    const bool if1 = run_container_is_full(src_1);
    const bool if2 = run_container_is_full(src_2);
    if (if1 || if2) {
        if (if1) {
            return;
        }
        if (if2) {
            run_container_copy(src_2, src_1);
            return;
        }
    }
    // we move the data to the end of the current array
    const int32_t maxoutput = src_1->n_runs + src_2->n_runs;
    const int32_t neededcapacity = maxoutput + src_1->n_runs;
    if (src_1->capacity < neededcapacity)
        run_container_grow(src_1, neededcapacity, true);
    memmove(src_1->runs + maxoutput, src_1->runs,
            src_1->n_runs * sizeof(rle16_t));
    rle16_t *inputsrc1 = src_1->runs + maxoutput;
    const int32_t input1nruns = src_1->n_runs;
    src_1->n_runs = 0;
    int32_t rlepos = 0;
    int32_t xrlepos = 0;

    while ((xrlepos < src_2->n_runs) && (rlepos < input1nruns)) {
        if (inputsrc1[rlepos].value <= src_2->runs[xrlepos].value) {
            run_container_append(src_1, inputsrc1[rlepos]);
            rlepos++;
        } else {
            run_container_append(src_1, src_2->runs[xrlepos]);
            xrlepos++;
        }
    }
    while (xrlepos < src_2->n_runs) {
        run_container_append(src_1, src_2->runs[xrlepos]);
        xrlepos++;
    }
    while (rlepos < input1nruns) {
        run_container_append(src_1, inputsrc1[rlepos]);
        rlepos++;
    }
}

/* Compute the intersection of src_1 and src_2 and write the result to
 * dst. It is assumed that dst is distinct from both src_1 and src_2. */
void run_container_intersection(const run_container_t *src_1,
                                const run_container_t *src_2,
                                run_container_t *dst) {
    const bool if1 = run_container_is_full(src_1);
    const bool if2 = run_container_is_full(src_2);
    if (if1 || if2) {
        if (if1) {
            run_container_copy(src_2, dst);
            return;
        }
        if (if2) {
            run_container_copy(src_1, dst);
            return;
        }
    }
    // TODO: this could be a lot more efficient, could use SIMD optimizations
    const int32_t neededcapacity = src_1->n_runs + src_2->n_runs;
    if (dst->capacity < neededcapacity)
        run_container_grow(dst, neededcapacity, false);
    dst->n_runs = 0;
    int32_t rlepos = 0;
    int32_t xrlepos = 0;
    int32_t start = src_1->runs[rlepos].value;
    int32_t end = start + src_1->runs[rlepos].length + 1;
    int32_t xstart = src_2->runs[xrlepos].value;
    int32_t xend = xstart + src_2->runs[xrlepos].length + 1;
    while ((rlepos < src_1->n_runs) && (xrlepos < src_2->n_runs)) {
        if (end <= xstart) {
            ++rlepos;
            if (rlepos < src_1->n_runs) {
                start = src_1->runs[rlepos].value;
                end = start + src_1->runs[rlepos].length + 1;
            }
        } else if (xend <= start) {
            ++xrlepos;
            if (xrlepos < src_2->n_runs) {
                xstart = src_2->runs[xrlepos].value;
                xend = xstart + src_2->runs[xrlepos].length + 1;
            }
        } else {  // they overlap
            const int32_t lateststart = start > xstart ? start : xstart;
            int32_t earliestend;
            if (end == xend) {  // improbable
                earliestend = end;
                rlepos++;
                xrlepos++;
                if (rlepos < src_1->n_runs) {
                    start = src_1->runs[rlepos].value;
                    end = start + src_1->runs[rlepos].length + 1;
                }
                if (xrlepos < src_2->n_runs) {
                    xstart = src_2->runs[xrlepos].value;
                    xend = xstart + src_2->runs[xrlepos].length + 1;
                }
            } else if (end < xend) {
                earliestend = end;
                rlepos++;
                if (rlepos < src_1->n_runs) {
                    start = src_1->runs[rlepos].value;
                    end = start + src_1->runs[rlepos].length + 1;
                }

            } else {  // end > xend
                earliestend = xend;
                xrlepos++;
                if (xrlepos < src_2->n_runs) {
                    xstart = src_2->runs[xrlepos].value;
                    xend = xstart + src_2->runs[xrlepos].length + 1;
                }
            }
            dst->runs[dst->n_runs].value = lateststart;
            dst->runs[dst->n_runs].length = (earliestend - lateststart - 1);
            dst->n_runs++;
        }
    }
}

int run_container_to_uint32_array(uint32_t *out, const run_container_t *cont,
                                  uint32_t base) {
    int outpos = 0;
    for (int i = 0; i < cont->n_runs; ++i) {
        uint32_t run_start = base + cont->runs[i].value;
        uint16_t le = cont->runs[i].length;
        for (int j = 0; j <= le; ++j) out[outpos++] = run_start + j;
    }
    return outpos;
}

/*
 * Print this container using printf (useful for debugging).
 */
void run_container_printf(const run_container_t *cont) {
    for (int i = 0; i < cont->n_runs; ++i) {
        uint16_t run_start = cont->runs[i].value;
        uint16_t le = cont->runs[i].length;
        printf("[%d,%d]", run_start, run_start + le);
    }
}

/*
 * Print this container using printf as a comma-separated list of 32-bit
 * integers starting at base.
 */
void run_container_printf_as_uint32_array(const run_container_t *cont,
                                          uint32_t base) {
    if (cont->n_runs == 0) return;
    {
        uint32_t run_start = base + cont->runs[0].value;
        uint16_t le = cont->runs[0].length;
        printf("%d", run_start);
        for (uint32_t j = 1; j <= le; ++j) printf(",%d", run_start + j);
    }
    for (int32_t i = 1; i < cont->n_runs; ++i) {
        uint32_t run_start = base + cont->runs[i].value;
        uint16_t le = cont->runs[i].length;
        for (uint32_t j = 0; j <= le; ++j) printf(",%d", run_start + j);
    }
}

int32_t run_container_serialize(run_container_t *container, char *buf) {
    int32_t l, off;

    memcpy(buf, &container->n_runs, off = sizeof(container->n_runs));
    memcpy(&buf[off], &container->capacity, sizeof(container->capacity));
    off += sizeof(container->capacity);

    l = sizeof(rle16_t) * container->n_runs;
    memcpy(&buf[off], container->runs, l);
    return (off + l);
}

int32_t run_container_write(run_container_t *container, char *buf) {
    if (IS_BIG_ENDIAN) {
        // forcing little endian (could be faster)
        buf[0] = (uint8_t)(container->n_runs);
        buf[1] = (uint8_t)(container->n_runs >> 8);
        for (int32_t i = 0; i < container->n_runs; i++) {
            rle16_t val = container->runs[i];
            buf[2 + 4 * i] = (uint8_t)(val.value);
            buf[2 + 4 * i + 1] = (uint8_t)(val.value >> 8);
            buf[2 + 4 * i + 2] = (uint8_t)(val.length);
            buf[2 + 4 * i + 3] = (uint8_t)(val.length >> 8);
        }
    } else {
        memcpy(buf, &container->n_runs, sizeof(uint16_t));
        memcpy(buf + sizeof(uint16_t), container->runs,
               container->n_runs * sizeof(rle16_t));
    }
    return run_container_size_in_bytes(container);
}

int32_t run_container_read(int32_t cardinality, run_container_t *container,
                           const char *buf) {
    (void)cardinality;
    assert(!IS_BIG_ENDIAN);  // TODO: Implement
    memcpy(&container->n_runs, buf, sizeof(uint16_t));
    if (container->n_runs > container->capacity)
        run_container_grow(container, container->n_runs, false);
    memcpy(container->runs, buf + sizeof(uint16_t),
           container->n_runs * sizeof(rle16_t));
    return run_container_size_in_bytes(container);
}

uint32_t run_container_serialization_len(run_container_t *container) {
    return (sizeof(container->n_runs) + sizeof(container->capacity) +
            sizeof(rle16_t) * container->n_runs);
}

void *run_container_deserialize(const char *buf, size_t buf_len) {
    run_container_t *ptr;

    if (buf_len < 8 /* n_runs + capacity */)
        return (NULL);
    else
        buf_len -= 8;

    if ((ptr = malloc(sizeof(run_container_t))) != NULL) {
        size_t len;
        int32_t off;

        memcpy(&ptr->n_runs, buf, off = 4);
        memcpy(&ptr->capacity, &buf[off], 4);
        off += 4;

        len = sizeof(rle16_t) * ptr->n_runs;

        if (len != buf_len) {
            free(ptr);
            return (NULL);
        }

        if ((ptr->runs = malloc(len)) == NULL) {
            free(ptr);
            return (NULL);
        }

        memcpy(ptr->runs, &buf[off], len);

        /* Check if returned values are monotonically increasing */
        for (int32_t i = 0, j = 0; i < ptr->n_runs; i++) {
            if (ptr->runs[i].value < j) {
                free(ptr->runs);
                free(ptr);
                return (NULL);
            } else
                j = ptr->runs[i].value;
        }
    }

    return (ptr);
}

void run_container_iterate(const run_container_t *cont, uint32_t base,
                           roaring_iterator iterator, void *ptr) {
    for (int i = 0; i < cont->n_runs; ++i) {
        uint32_t run_start = base + cont->runs[i].value;
        uint16_t le = cont->runs[i].length;

        for (int j = 0; j <= le; ++j) iterator(run_start + j, ptr);
    }
}

bool run_container_equals(run_container_t *container1,
                          run_container_t *container2) {
    if (container1->n_runs != container2->n_runs) {
        return false;
    }
    for (int32_t i = 1; i < container1->n_runs; ++i) {
        if ((container1->runs[i].value != container2->runs[i].value) ||
            (container1->runs[i].length != container2->runs[i].length))
            return false;
    }
    return true;
}
