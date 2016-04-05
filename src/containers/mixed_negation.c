/*
 * mixed_negation.c
 *
 */

#include "mixed_negation.h"
#include <assert.h>

/* commented out region to quiet warnings in master */
/* //#include "array_util.h" */
/* //#include "bitset_util.h" */
/* //#include "convert.h" */

/* // TODO: make simplified and optimized negation code across */
/* // the full range. */

/* /\* Negation across the entire range of the container. */
/*  * Compute the  negation of src  and write the result */
/*  * to *dst. The complement of a */
/*  * sufficiently sparse set will always be dense and a hence a bitmap */
/*  * We assume that dst is pre-allocated and a valid bitset container */
/*  * There can be no in-place version. */
/*  *\/ */
/* void array_container_negation(const array_container_t *src, */
/*                               bitset_container_t *dst) { */
/*     assert(0);  // TODO write it */
/* } */

/* /\* Negation across the entire range of the container */
/*  * Compute the  negation of src  and write the result */
/*  * to *dst.  A true return value indicates a bitset result, */
/*  * otherwise the result is an array container. */
/*  *  We assume that dst is not pre-allocated. In */
/*  * case of failure, *dst will be NULL. */
/*  *\/ */
/* bool bitset_container_negation(const bitset_container_t *src, void **dst) {
 */
/*     return bitset_container_negation_range(src, (uint16_t)0,
 * (uint16_t)0xFFFF, */
/*                                            dst); */
/* } */

/* /\* inplace version *\/ */
/* /\* */
/*  * Same as bitset_container_negation except that if the output is to */
/*  * be a */
/*  * bitset_container_t, then src is modified and no allocation is made. */
/*  * If the output is to be an array_container_t, then caller is responsible */
/*  * to free the container. */
/*  * In all cases, the result is in *dst. */
/*  *\/ */
/* bool bitset_container_negation_inplace(bitset_container_t *src, void **dst) {
 */
/*     return bitset_container_negation_range_inplace(src, (uint16_t)0, */
/*                                                    (uint16_t)0xFFFF, dst); */
/* } */

/* /\* Negation across the entire range of container */
/*  * Compute the  negation of src  and write the result */
/*  * to *dst.  A return value of 0 indicates an array result, */
/*  * while a 1 indicates an bitset result, and 2 indicates a */
/*  * run result. */
/*  *  We assume that dst is not pre-allocated. In */
/*  * case of failure, *dst will be NULL. */
/*  *\/ */
/* int run_container_negation(const run_container_t *src, void **dst) { */
/*     return run_container_negation_range(src, (uint16_t)0, (uint16_t)0xFFFF,
 */
/*                                         dst); */
/* } */

/* /\* */
/*  * Same as run_container_negation except that if the output is to */
/*  * be a */
/*  * run_container_t, and has the capacity to hold the result, */
/*  * then src is modified and no allocation is made. */
/*  * In all cases, the result is in *dst. */
/*  *\/ */
/* int run_container_negation_inplace(run_container_t *src, void **dst) { */
/*     return run_container_negation_range(src, (uint16_t)0, (uint16_t)0xFFFF,
 */
/*                                         dst); */
/* } */

/* /\* Negation across a range of the container. */
/*  * Compute the  negation of src  and write the result */
/*  * to *dst. Returns true if the result is a bitset container */
/*  * and false for an array container.  *dst is not preallocated. */
/*  *\/ */
/* bool array_container_negation_range(const array_container_t *src, */
/*                                     const uint16_t range_start, */
/*                                     const uint16_t range_end_inclusive, */
/*                                     void **dst) { */
/*     // TODO WRITE ME */
/*     return true; */
/* } */

/* /\* Even when the result would fit, it is unclear how to make an */
/*  * inplace version without inefficient copying. */
/*  *\/ */

/* /\* Negation across a range of the container */
/*  * Compute the  negation of src  and write the result */
/*  * to *dst.  A true return value indicates a bitset result, */
/*  * otherwise the result is an array container. */
/*  *  We assume that dst is not pre-allocated. In */
/*  * case of failure, *dst will be NULL. */
/*  *\/ */
/* bool bitset_container_negation_range(const bitset_container_t *src, */
/*                                      const uint16_t range_start, */
/*                                      const uint16_t range_end_inclusive, */
/*                                      void **dst) { */
/*     return true; */
/*     // TODO WRITE ME */
/* } */

/* /\* inplace version *\/ */
/* /\* */
/*  * Same as bitset_container_negation except that if the output is to */
/*  * be a */
/*  * bitset_container_t, then src is modified and no allocation is made. */
/*  * If the output is to be an array_container_t, then caller is responsible */
/*  * to free the container. */
/*  * In all cases, the result is in *dst. */
/*  *\/ */
/* bool bitset_container_negation_range_inplace(bitset_container_t *src, */
/*                                              const uint16_t range_start, */
/*                                              const uint16_t
 * range_end_inclusive, */
/*                                              void **dst) { */
/*     return true; */
/*     // TODO WRITE */
/* } */

/* /\* Negation across a range of container */
/*  * Compute the  negation of src  and write the result */
/*  * to *dst.  A return value of 0 indicates an array result, */
/*  * while a 1 indicates a bitset result, and 2 indicates a */
/*  * run result. */
/*  *  We assume that dst is not pre-allocated. In */
/*  * case of failure, *dst will be NULL. */
/*  *\/ */
/* int run_container_negation_range(const run_container_t *src, */
/*                                  const uint16_t range_start, */
/*                                  const uint16_t range_end_inclusive, */
/*                                  void **dst) {  // TODO WRITE ME */
/*     return 0; */
/* } */

/* /\* */
/*  * Same as run_container_negation except that if the output is to */
/*  * be a */
/*  * run_container_t, and has the capacity to hold the result, */
/*  * then src is modified and no allocation is made. */
/*  * In all cases, the result is in *dst. */
/*  *\/ */
/* int run_container_negation_range_inplace(run_container_t *src, */
/*                                          const uint16_t range_start, */
/*                                          const uint16_t range_end_inclusive,
 */
/*                                          void **dst) { */
/*     // TODO WRITE */
/*     return 0; */
/* } */
