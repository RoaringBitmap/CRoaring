/*
 * mixed_xor.h
 *
 */

#ifndef INCLUDE_CONTAINERS_MIXED_XOR_H_
#define INCLUDE_CONTAINERS_MIXED_XOR_H_

/* These functions appear to exclude cases where the
 * inputs have the same type and the output is guaranteed
 * to have the same type as the inputs.  Eg, bitset unions
 */

/*
 * Java implementation (as of May 2016) for array_run, run_run
 * and  bitset_run don't do anything different for inplace.
 * (They are not truly in place.)
 */

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/run.h>

//#include "containers.h"

#ifdef __cplusplus
extern "C" { namespace roaring { namespace internal {
#endif

/* NON-LAZY XOR FUNCTIONS
 *
 * These functions compute the xor of src_1 and src_2 and return the result.
 * result_type will be the type of the newly generated container.
 *
 * They are "non-lazy" because they *do* compact the result container to a
 * minimal size, and ensure the cardinality of bitsets has been precalculated.
 */

container_t *array_bitset_container_xor(
        const array_container_t *ac1, const bitset_container_t *bc2,
        uint8_t *result_type);

container_t *bitset_bitset_container_xor(
        const bitset_container_t *bc1, const bitset_container_t *bc2,
        uint8_t *result_type);

container_t *run_bitset_container_xor(
        const run_container_t *rc1, const bitset_container_t *bc2,
        uint8_t *result_type);

container_t *array_run_container_xor(
        const array_container_t *ac1, const run_container_t *rc2,
        uint8_t *result_type);

container_t *array_array_container_xor(
        const array_container_t *ac1, const array_container_t *ac2,
        uint8_t *result_type);

container_t *run_run_container_xor(
        const run_container_t *rc1, const run_container_t *rc2,
        uint8_t *result_type);


/* LAZY XOR OPERATIONS
 *
 * Bitset results will be left as a bitset container, even if actual
 * cardinality would dictate an array container.  Cardinality may be left
 * as BITSET_UNKNOWN_CARDINALITY to be calculated on demand.
 *
 * Run results will be left as arun container, even if other options are
 * smaller.
 */

container_t *bitset_bitset_container_lazy_xor(
        const bitset_container_t *bc1, const bitset_container_t *bc2,
        uint8_t *result_type);

container_t *array_bitset_container_lazy_xor(
        const array_container_t *ac1, const bitset_container_t *bc2,
        uint8_t *result_type);

container_t *run_bitset_container_lazy_xor(
        const run_container_t *rc1, const bitset_container_t *bc2,
        uint8_t *result_type);

container_t *array_array_container_lazy_xor(
        const array_container_t *ac1, const array_container_t *ac2,
        uint8_t *result_type);

run_container_t *array_run_container_lazy_xor(
        const array_container_t *ac1, const run_container_t *rc2,
        uint8_t *result_type);

#define run_run_container_lazy_xor \
        run_run_container_xor  // no `lazy` version yet


/* EQUIVALENCIES since order does not matter for non-inplace xor.
 */

#define bitset_run_container_xor(c1,c2,t) \
    run_bitset_container_xor(c2,c1,t)

#define run_array_container_xor(c1,c2,t) \
    array_run_container_xor(c2,c1,t)



/* INPLACE versions (initial implementation may not exploit all inplace
 * opportunities (if any...)
 *
 * Compute the xor of src_1 and src_2 and write the result to src_1.
 * type1 should be the correct type of src1 to start with.
 * The type may be modified.
 */

bool bitset_array_container_ixor(
        container_t **c1, uint8_t *type1,
        const array_container_t *ac2);

bool bitset_bitset_container_ixor(
        container_t **c1, uint8_t *type1,
        const bitset_container_t *bc2);

bool array_bitset_container_ixor(
        container_t **c1, uint8_t *type1,
        const bitset_container_t *bc2);

bool run_bitset_container_ixor(
        container_t **c1, uint8_t *type1,
        const bitset_container_t *bc2);

bool bitset_run_container_ixor(
        container_t **c1, uint8_t *type1,
        const run_container_t *rc2);

bool array_run_container_ixor(
        container_t **c1, uint8_t *type1,
        const run_container_t *rc2);

bool run_array_container_ixor(
        container_t **c1, uint8_t *type1,
        const array_container_t *ac2);

bool array_array_container_ixor(
        container_t **c1, uint8_t *type1,
        const array_container_t *ac2);

bool run_run_container_ixor(
        container_t **c1, uint8_t *type1,
        const run_container_t *ac2);


#ifdef __cplusplus
} } }  // extern "C" { namespace roaring { namespace internal {
#endif

#endif
