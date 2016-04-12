
#include "containers/containers.h"

extern const char *get_container_name(uint8_t typecode);

extern int container_get_cardinality(const void *container, uint8_t typecode);

extern void *container_iand(void *c1, uint8_t type1, const void *c2,
                            uint8_t type2, uint8_t *result_type);

extern void *container_ior(void *c1, uint8_t type1, const void *c2,
                           uint8_t type2, uint8_t *result_type);

void container_printf(const void *container, uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_printf(container);
            return;
        case ARRAY_CONTAINER_TYPE_CODE:
            array_container_printf(container);
            return;
        case RUN_CONTAINER_TYPE_CODE:
            run_container_printf(container);
            return;
    }
}

void container_printf_as_uint32_array(const void *container, uint8_t typecode,
                                      uint32_t base) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            bitset_container_printf_as_uint32_array(container, base);
            return;
        case ARRAY_CONTAINER_TYPE_CODE:
            array_container_printf_as_uint32_array(container, base);
            return;
        case RUN_CONTAINER_TYPE_CODE:
            run_container_printf_as_uint32_array(container, base);
            return;
    }
}

int32_t container_serialize(void *container, uint8_t typecode, char *buf) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return (bitset_container_serialize((bitset_container_t *)container,
                                               buf));
        case ARRAY_CONTAINER_TYPE_CODE:
            return (
                array_container_serialize((array_container_t *)container, buf));
        case RUN_CONTAINER_TYPE_CODE:
            return (run_container_serialize((run_container_t *)container, buf));
        default:
            assert(0);
            return (-1);
    }
}

uint32_t container_serialization_len(void *container, uint8_t typecode) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return bitset_container_serialization_len();
        case ARRAY_CONTAINER_TYPE_CODE:
            return array_container_serialization_len(
                (array_container_t *)container);
        case RUN_CONTAINER_TYPE_CODE:
            return run_container_serialization_len(
                (run_container_t *)container);
        default:
            assert(0);
            return (0);
    }
}

void *container_deserialize(uint8_t typecode, const char *buf, size_t buf_len) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE_CODE:
            return (bitset_container_deserialize(buf, buf_len));
        case ARRAY_CONTAINER_TYPE_CODE:
            return (array_container_deserialize(buf, buf_len));
        case RUN_CONTAINER_TYPE_CODE:
            return (run_container_deserialize(buf, buf_len));
        default:
            assert(0);
            return (NULL);
    }
}

extern bool container_nonzero_cardinality(const void *container,
                                          uint8_t typecode);

extern void container_free(void *container, uint8_t typecode);

extern int container_to_uint32_array(uint32_t *output, const void *container,
                                     uint8_t typecode, uint32_t base);

extern void *container_add(void *container, uint16_t val, uint8_t typecode,
                           uint8_t *new_typecode);

extern bool container_contains(const void *container, uint16_t val,
                               uint8_t typecode);

extern void *container_clone(const void *container, uint8_t typecode);

extern void *container_and(const void *c1, uint8_t type1, const void *c2,
                           uint8_t type2, uint8_t *result_type);

extern void *container_or(const void *c1, uint8_t type1, const void *c2,
                          uint8_t type2, uint8_t *result_type);
