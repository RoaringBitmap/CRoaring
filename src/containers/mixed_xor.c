/*
 * mixed_xor.c
 */

#include <assert.h>
#include <string.h>

#include <roaring/bitset_util.h>
#include <roaring/containers/containers.h>
#include <roaring/containers/convert.h>
#include <roaring/containers/mixed_xor.h>
#include <roaring/containers/perfparameters.h>

#ifdef __cplusplus
extern "C" { namespace roaring { namespace internal {
#endif


container_t *array_bitset_container_xor(
    const array_container_t *ac1, const bitset_container_t *bc2,
    uint8_t *result_type
){
    bitset_container_t *new_bc = bitset_container_create();
    bitset_container_copy(bc2, new_bc);
    new_bc->cardinality = (int32_t)bitset_flip_list_withcard(
                                        new_bc->array, new_bc->cardinality,
                                        ac1->array, ac1->cardinality);

    if (new_bc->cardinality <= DEFAULT_MAX_SIZE) {  // compact into array
        array_container_t *new_ac = array_container_from_bitset(new_bc);
        bitset_container_free(new_bc);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    }

    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}

/* Compute the xor of ac1 and bc2.  This version does not update the
 * cardinality of dst (it is set to BITSET_UNKNOWN_CARDINALITY).
 */
container_t *array_bitset_container_lazy_xor(
    const array_container_t *ac1, const bitset_container_t *bc2,
    uint8_t *result_type
){
    bitset_container_t *new_bc = bitset_container_create();

    bitset_container_copy(bc2, new_bc);
    bitset_flip_list(new_bc->array, ac1->array, ac1->cardinality);
    new_bc->cardinality = BITSET_UNKNOWN_CARDINALITY;

    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}


container_t *run_bitset_container_xor(
    const run_container_t *rc1, const bitset_container_t *bc2,
    uint8_t *result_type  // type of the newly created container returned
){
    bitset_container_t *new_bc = bitset_container_create();

    bitset_container_copy(bc2, new_bc);
    for (int32_t rlepos = 0; rlepos < rc1->n_runs; ++rlepos) {
        rle16_t rle = rc1->runs[rlepos];
        bitset_flip_range(new_bc->array, rle.value,
                          rle.value + rle.length + UINT32_C(1));
    }
    new_bc->cardinality = bitset_container_compute_cardinality(new_bc);

    if (new_bc->cardinality <= DEFAULT_MAX_SIZE) {
        array_container_t *new_ac = array_container_from_bitset(new_bc);
        bitset_container_free(new_bc);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    }

    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}

/* lazy xor. Result is left as a bitset container, even if actual
 * cardinality would dictate an array container.
 */
container_t *run_bitset_container_lazy_xor(
    const run_container_t *rc1, const bitset_container_t *bc2,
    uint8_t *result_type
){
    bitset_container_t *new_bc = bitset_container_create();
    bitset_container_copy(bc2, new_bc);

    for (int32_t rlepos = 0; rlepos < rc1->n_runs; ++rlepos) {
        rle16_t rle = rc1->runs[rlepos];
        bitset_flip_range(new_bc->array, rle.value,
                          rle.value + rle.length + UINT32_C(1));
    }
    new_bc->cardinality = BITSET_UNKNOWN_CARDINALITY;

    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}

container_t *array_run_container_xor(
    const array_container_t *ac1, const run_container_t *rc2,
    uint8_t *result_type
){
    // semi following Java XOR implementation as of May 2016
    // the C OR implementation works quite differently and can return a run
    // container
    // TODO could optimize for full run containers.

    // use of lazy following Java impl.
    const int arbitrary_threshold = 32;
    if (ac1->cardinality < arbitrary_threshold) {
        run_container_t *new_rc = array_run_container_lazy_xor(ac1, rc2,
                                  result_type);  // keeps runs
        assert(*result_type == RUN_CONTAINER_TYPE);
        return convert_run_to_efficient_container_and_free(new_rc, result_type);
    }

    int card = run_container_cardinality(rc2);
    if (card <= DEFAULT_MAX_SIZE) {
        // Java implementation works with the array, xoring the run elements via
        // iterator
        array_container_t *temp = array_container_from_run(rc2);
        container_t *ans = array_array_container_xor(temp, ac1, result_type);
        array_container_free(temp);
        return ans;
    }
    
    // guess that it will end up as a bitset
    container_t *ans = bitset_container_from_run(rc2);
    *result_type = BITSET_CONTAINER_TYPE;
    bitset_array_container_ixor(&ans, result_type, ac1);
    return ans;
}

/* Leaves result as run container, even if other options are
 * smaller.
 */
run_container_t *array_run_container_lazy_xor(
    const array_container_t *ac1, const run_container_t *rc2,
    uint8_t *result_type  // currently always RUN_CONTAINER_TYPE
){
    run_container_t *new_rc = run_container_create();
    run_container_grow(new_rc, ac1->cardinality + rc2->n_runs, false);

    int32_t rlepos = 0;
    int32_t arraypos = 0;
    new_rc->n_runs = 0;

    while ((rlepos < rc2->n_runs) && (arraypos < ac1->cardinality)) {
        if (rc2->runs[rlepos].value <= ac1->array[arraypos]) {
            run_container_smart_append_exclusive(new_rc,
                                                 rc2->runs[rlepos].value,
                                                 rc2->runs[rlepos].length);
            rlepos++;
        } else {
            run_container_smart_append_exclusive(new_rc, ac1->array[arraypos],
                                                 0);
            arraypos++;
        }
    }
    while (arraypos < ac1->cardinality) {
        run_container_smart_append_exclusive(new_rc, ac1->array[arraypos], 0);
        arraypos++;
    }
    while (rlepos < rc2->n_runs) {
        run_container_smart_append_exclusive(new_rc, rc2->runs[rlepos].value,
                                             rc2->runs[rlepos].length);
        rlepos++;
    }

    // Since we are lazy, `convert_run_to_efficient_container` is not called.
    // Caller must do it if non-lazy.
    //
    *result_type = RUN_CONTAINER_TYPE;
    return new_rc;
}


container_t *run_run_container_xor(
    const run_container_t *rc1, const run_container_t *rc2,
    uint8_t *result_type
){
    run_container_t *new_rc = run_container_create();
    run_container_xor(rc1, rc2, new_rc);
    return convert_run_to_efficient_container_and_free(new_rc, result_type);
}

container_t *array_array_container_xor(
    const array_container_t *ac1, const array_container_t *ac2,
    uint8_t *result_type
){
    int ub_card = ac1->cardinality + ac2->cardinality;  // upper bound
    if (ub_card <= DEFAULT_MAX_SIZE) {
        array_container_t *new_ac =
                array_container_create_given_capacity(ub_card);
        array_container_xor(ac1, ac2, new_ac);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    }
    bitset_container_t *new_bc = bitset_container_from_array(ac1);
    new_bc->cardinality = (uint32_t)bitset_flip_list_withcard(
                                        new_bc->array, new_bc->cardinality,
                                        ac2->array, ac2->cardinality);

    if (new_bc->cardinality <= DEFAULT_MAX_SIZE) {  // need to convert!
        array_container_t *new_ac = array_container_from_bitset(new_bc);
        bitset_container_free(new_bc);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    }

    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}

container_t *bitset_bitset_container_lazy_xor(
    const bitset_container_t *bc1, const bitset_container_t *bc2,
    uint8_t *result_type
){
    bitset_container_t *new_bc = bitset_container_create();
    bitset_container_xor_nocard(bc1, bc2, new_bc);  // is lazy
    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}

container_t *array_array_container_lazy_xor(
    const array_container_t *ac1, const array_container_t *ac2,
    uint8_t *result_type
){
    int ub_card = ac1->cardinality + ac2->cardinality;
    // upper bound, but probably poor estimate for xor
    if (ub_card <= ARRAY_LAZY_LOWERBOUND) {
        array_container_t *new_ac =
                array_container_create_given_capacity(ub_card);
        if (new_ac == NULL)
            return NULL;
        array_container_xor(ac1, ac2, new_ac);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    }

    bitset_container_t *new_bc = bitset_container_from_array(ac1);
    if (new_bc == NULL)
        return NULL;
    bitset_flip_list(new_bc->array, ac2->array, ac2->cardinality);
    new_bc->cardinality = BITSET_UNKNOWN_CARDINALITY;

    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}


container_t *bitset_bitset_container_xor(
    const bitset_container_t *bc1, const bitset_container_t *bc2,
    uint8_t *result_type
){
    bitset_container_t *new_bc = bitset_container_create();
    int card = bitset_container_xor(bc1, bc2, new_bc);
    if (card <= DEFAULT_MAX_SIZE) {
        array_container_t *new_ac = array_container_from_bitset(new_bc);
        bitset_container_free(new_bc);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    } else {
        *result_type = BITSET_CONTAINER_TYPE;
        return new_bc;
    }
}


/**
 * IN-PLACE XOR COMBINATIONS
 *
 * If a tailored in-place implemention is not available, then the plain xor is
 * used to generate a new container and the old one freed.
 *
 * Java implementation (as of May 2016) for array_run, run_run and bitset_run
 * don't do anything different for inplace.  Could adopt the mixed_union.c
 * approach instead (ie, using smart_append_exclusive)
 */

bool bitset_array_container_ixor(
    container_t **c1, uint8_t *type1,
    const array_container_t *ac2
){
    assert(*type1 == BITSET_CONTAINER_TYPE);
    bitset_container_t *bc1 = *movable_CAST_bitset(c1);

    bc1->cardinality = (uint32_t)bitset_flip_list_withcard(
                                        bc1->array, bc1->cardinality,
                                        ac2->array, ac2->cardinality);

    if (bc1->cardinality <= DEFAULT_MAX_SIZE) {
        array_container_t *ac1 = array_container_from_bitset(bc1);
        if (ac1 == NULL)
            return false;

        *c1 = ac1;
        bitset_container_free(bc1);
        *type1 = ARRAY_CONTAINER_TYPE;
        return true;
    }

    assert(*type1 == BITSET_CONTAINER_TYPE);
    return true;
}

DECLARE_INPLACE_DEFAULT(bitset, bitset, xor)

DECLARE_INPLACE_DEFAULT(array, bitset, xor)

DECLARE_INPLACE_DEFAULT(run, bitset, xor)

DECLARE_INPLACE_DEFAULT(bitset, run, xor)

DECLARE_INPLACE_DEFAULT(array, run, xor)

DECLARE_INPLACE_DEFAULT(run, array, xor)

DECLARE_INPLACE_DEFAULT(array, array, xor)

DECLARE_INPLACE_DEFAULT(run, run, xor)


#ifdef __cplusplus
} } }  // extern "C" { namespace roaring { namespace internal {
#endif
