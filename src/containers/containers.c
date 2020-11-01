
#include <roaring/containers/containers.h>

#ifdef __cplusplus
extern "C" { namespace roaring { namespace internal {
#endif

extern inline const container_t *container_unwrap_shared(
        const container_t *candidate_shared_container, uint8_t *type);

extern inline container_t *container_mutable_unwrap_shared(
        container_t *candidate_shared_container, uint8_t *type);

extern inline int container_get_cardinality(
        const container_t *c, uint8_t typecode);

extern inline container_t *container_iand(
        container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

extern inline container_t *container_ior(
        container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

extern inline container_t *container_ixor(
        container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

extern inline container_t *container_iandnot(
        container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

void container_free(container_t *container, uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_free((bitset_container_t *)container);
            break;
        case ARRAY_CONTAINER_TYPE_CODE:
            array_container_free((array_container_t *)container);
            break;
        case RUN_CONTAINER_TYPE_CODE:
            run_container_free((run_container_t *)container);
            break;
        case SHARED_CONTAINER_TYPE_CODE:
            shared_container_free((shared_container_t *)container);
            break;
        default:
            assert(false);
            __builtin_unreachable();
    }
}

void container_printf(const container_t *container, uint8_t typecode) {
    container = container_unwrap_shared(container, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_printf((const bitset_container_t *)container);
            return;
        case ARRAY_CONTAINER_TYPE_CODE:
            array_container_printf((const array_container_t *)container);
            return;
        case RUN_CONTAINER_TYPE_CODE:
            run_container_printf((const run_container_t *)container);
            return;
        default:
            __builtin_unreachable();
    }
}

void container_printf_as_uint32_array(
    const container_t *c, uint8_t typecode,
    uint32_t base
){
    c = container_unwrap_shared(c, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_printf_as_uint32_array(
                (const bitset_container_t *)c, base);
            return;
        case ARRAY_CONTAINER_TYPE_CODE:
            array_container_printf_as_uint32_array(
                (const array_container_t *)c, base);
            return;
        case RUN_CONTAINER_TYPE_CODE:
            run_container_printf_as_uint32_array(
                (const run_container_t *)c, base);
            return;
        default:
            __builtin_unreachable();
    }
}

extern inline bool container_nonzero_cardinality(
        const container_t *c, uint8_t typecode);

extern inline int container_to_uint32_array(
        uint32_t *output,
        const container_t *c, uint8_t typecode,
        uint32_t base);

extern inline container_t *container_add(
        container_t *c,
        uint16_t val,
        uint8_t typecode,  // !!! 2nd arg?
        uint8_t *new_typecode);

extern inline bool container_contains(
        const container_t *c,
        uint16_t val,
        uint8_t typecode);  // !!! 2nd arg?

extern inline container_t *container_and(
        const container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

extern inline container_t *container_or(
        const container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

extern inline container_t *container_xor(
        const container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

container_t *get_copy_of_container(
    container_t *c, uint8_t *typecode,
    bool copy_on_write
){
    if (copy_on_write) {
        shared_container_t *shared_container;
        if (*typecode == SHARED_CONTAINER_TYPE_CODE) {
            shared_container = (shared_container_t *)c;
            shared_container->counter += 1;
            return shared_container;
        }
        assert(*typecode != SHARED_CONTAINER_TYPE_CODE);

        if ((shared_container = (shared_container_t *)malloc(
                 sizeof(shared_container_t))) == NULL) {
            return NULL;
        }

        shared_container->container = c;
        shared_container->typecode = *typecode;

        shared_container->counter = 2;
        *typecode = SHARED_CONTAINER_TYPE_CODE;

        return shared_container;
    }  // copy_on_write
    // otherwise, no copy on write...
    const container_t *actual_container = container_unwrap_shared(c, typecode);
    assert(*typecode != SHARED_CONTAINER_TYPE_CODE);
    return container_clone(actual_container, *typecode);
}

/**
 * Copies a container, requires a typecode. This allocates new memory, caller
 * is responsible for deallocation.
 */
container_t *container_clone(const container_t *c, uint8_t typecode) {
    c = container_unwrap_shared(c, &typecode);
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_clone((const bitset_container_t *)c);
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_clone((const array_container_t *)c);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_clone((const run_container_t *)c);
        case SHARED_CONTAINER_TYPE_CODE:
            printf("shared containers are not cloneable\n");
            assert(false);
            return NULL;
        default:
            assert(false);
            __builtin_unreachable();
            return NULL;
    }
}

container_t *shared_container_extract_copy(
    shared_container_t *sc, uint8_t *typecode
){
    assert(sc->counter > 0);
    assert(sc->typecode != SHARED_CONTAINER_TYPE_CODE);
    sc->counter--;
    *typecode = sc->typecode;
    container_t *answer;
    if (sc->counter == 0) {
        answer = sc->container;
        sc->container = NULL;  // paranoid
        free(sc);
    } else {
        answer = container_clone(sc->container, *typecode);
    }
    assert(*typecode != SHARED_CONTAINER_TYPE_CODE);
    return answer;
}

void shared_container_free(shared_container_t *container) {
    assert(container->counter > 0);
    container->counter--;
    if (container->counter == 0) {
        assert(container->typecode != SHARED_CONTAINER_TYPE_CODE);
        container_free(container->container, container->typecode);
        container->container = NULL;  // paranoid
        free(container);
    }
}

extern inline container_t *container_not(
        const container_t *c1, uint8_t type1,
        uint8_t *result_type);

extern inline container_t *container_not_range(
        const container_t *c1, uint8_t type1,
        uint32_t range_start, uint32_t range_end,
        uint8_t *result_type);

extern inline container_t *container_inot(
        container_t *c1, uint8_t type1,
        uint8_t *result_type);

extern inline container_t *container_inot_range(
        container_t *c1, uint8_t type1,
        uint32_t range_start, uint32_t range_end,
        uint8_t *result_type);

extern inline container_t *container_range_of_ones(
        uint32_t range_start, uint32_t range_end,
        uint8_t *result_type);

// where are the correponding things for union and intersection??
extern inline container_t *container_lazy_xor(
        const container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

extern inline container_t *container_lazy_ixor(
        container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

extern inline container_t *container_andnot(
        const container_t *c1, uint8_t type1,
        const container_t *c2, uint8_t type2,
        uint8_t *result_type);

#ifdef __cplusplus
} } }  // extern "C" { namespace roaring { namespace internal {
#endif
