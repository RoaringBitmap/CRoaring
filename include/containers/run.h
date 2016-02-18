/*
 * run.h
 *
 */

#ifndef INCLUDE_CONTAINERS_RUN_H_
#define INCLUDE_CONTAINERS_RUN_H_

#include <stdint.h>
#include <stdbool.h>

struct valuelength_s {
    uint16_t value;   // start value of the run
    uint16_t length;  // length+1 is the length of the run
};

typedef struct valuelength_s valuelength_t;

struct run_container_s {
    int32_t nbrruns;   // how many runs, this number should fit in 16 bits.
    int32_t capacity;  // how many runs we could store in valueslength, should
                       // be no smaller than nbrruns.
    valuelength_t *valueslength;  // we interleave values and lengths, so
    // that if you have the values 11,12,13,14,15, you store that as 11,4 where
    // 4 means that beyond 11 itself, there are
    // 4 contiguous values that follows.
    // Other example: e.g., 1, 10, 20,0, 31,2 would be a concise representation
    // of  1, 2, ..., 11, 20, 31, 32, 33
};

typedef struct run_container_s run_container_t;

/* Create a new run container. Return NULL in case of failure. */
run_container_t *run_container_create();

/* Free memory owned by `run'. */
void run_container_free(run_container_t *run);

/* Duplicate container */
run_container_t *run_container_clone(run_container_t *src);

/* Add `pos' to `run'. Returns true if `pos' was not present. */
bool run_container_add(run_container_t *run, uint16_t pos);

/* Remove `pos' from `run'. Returns true if `pos' was present. */
bool run_container_remove(run_container_t *run, uint16_t pos);

/* Check whether `pos' is present in `run'.  */
bool run_container_contains(const run_container_t *run, uint16_t pos);

/* Get the cardinality of `run'. Requires an actual computation. */
int run_container_cardinality(const run_container_t *run);

/* Card > 0? */
inline bool run_container_nonzero_cardinality(run_container_t *r) {
    return r->nbrruns > 0;  // runs never empty
}

/* Copy one container into another. We assume that they are distinct. */
void run_container_copy(run_container_t *source, run_container_t *dest);

/* Set the cardinality to zero (does not release memory). */
inline void run_container_clear(run_container_t *run) { run->nbrruns = 0; }

/* Check whether the container spans the whole chunk (cardinality = 1<<16).
 * This check can be done in constant time (inexpensive). */
inline bool run_container_is_full(run_container_t *run) {
    valuelength_t vl = run->valueslength[0];
    return (run->nbrruns == 1) && (vl.value == 0) && (vl.length == 0xFFFF);
}

/* Compute the union of `src_1' and `src_2' and write the result to `dst'
 * It is assumed that `dst' is distinct from both `src_1' and `src_2'. */
void run_container_union(run_container_t *src_1, run_container_t *src_2,
                         run_container_t *dst);

/* Compute the intersection of src_1 and src_2 and write the result to
 * dst. It is assumed that dst is distinct from both src_1 and src_2. */
void run_container_intersection(run_container_t *src_1, run_container_t *src_2,
                                run_container_t *dst);

/*
 * Write out the 16-bit integers contained in this container as a list of 32-bit
 * integers using base
 * as the starting value (it might be expected that base has zeros in its 16
 * least significant bits).
 * The function returns the number of values written.
 * The caller is responsible for allocating enough memory in out.
 */
int run_container_to_uint32_array(uint32_t *out, const run_container_t *cont,
                                  uint32_t base);

/*
 * Print this container using printf (useful for debugging).
 */
void run_container_printf(const run_container_t *v);

/*
 * Print this container using printf as a comma-separated list of 32-bit
 * integers starting at base.
 */
void run_container_printf_as_uint32_array(const run_container_t *v,
                                          uint32_t base);

#endif /* INCLUDE_CONTAINERS_RUN_H_ */
