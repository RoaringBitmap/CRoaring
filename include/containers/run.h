/*
 * run.h
 *
 */

#ifndef INCLUDE_CONTAINERS_RUN_H_
#define INCLUDE_CONTAINERS_RUN_H_

#include <stdint.h>
#include <stdbool.h>

struct run_container_s {
    int32_t nbrruns;// how many runs, this number should fit in 16 bits.
    int32_t capacity;// how many runs we could store in valueslength, should be no smaller than nbrruns.
    uint16_t *valueslength; // we interleave values and lengths, so
    // that if you have the values 11,12,13,14,15, you store that as 11,4 where 4 means that beyond 11 itself, there are
    // 4 contiguous values that follows.
    // Other example: e.g., 1, 10, 20,0, 31,2 would be a concise representation of  1, 2, ..., 11, 20, 31, 32, 33

};

typedef struct run_container_s run_container_t;

/* Create a new run container. Return NULL in case of failure. */
run_container_t *run_container_create();

/* Free memory owned by `run'. */
void run_container_free(run_container_t *run);

/* Add `pos' to `run'. Returns true if `pos' was not present. */
bool run_container_add(run_container_t *run, uint16_t pos);

/* Remove `pos' from `run'. Returns true if `pos' was present. */
bool run_container_remove(run_container_t *run, uint16_t pos);

/* Check whether `pos' is present in `run'.  */
bool run_container_contains(const run_container_t *run, uint16_t pos);

/* Get the cardinality of `run'. Requires an actual computation. */
int run_container_cardinality(const run_container_t *run);

/* Set the cardinality to zero (does not release memory). */
inline void run_container_clear(run_container_t *run) {
    run->nbrruns = 0;
}

/* Compute the union of `src_1' and `src_2' and write the result to `dst'
 * It is assumed that `dst' is distinct from both `src_1' and `src_2'. */
//void run_container_union(const run_container_t *src_1,
  //                         const run_container_t *src_2,
    //                       run_container_t *dst);

/* Compute the intersection of src_1 and src_2 and write the result to
 * dst. It is assumed that dst is distinct from both src_1 and src_2. */
//void run_container_intersection(const run_container_t *src_1,
  //                                const run_container_t *src_2,
    //                              run_container_t *dst);

#endif /* INCLUDE_CONTAINERS_RUN_H_ */
