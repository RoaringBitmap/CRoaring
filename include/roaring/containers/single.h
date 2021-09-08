#ifndef INCLUDE_CONTAINERS_SINGLE_H_
#define INCLUDE_CONTAINERS_SINGLE_H_
#include <roaring/containers/array.h>
#include <stdint.h>
#include <string.h>

#define SINGLE_CONTAINER_MAX_CAPTAIN (sizeof(void*) / sizeof(uint16_t) - 1)

typedef struct single_container_s {
    uint16_t len;
    uint16_t vals[SINGLE_CONTAINER_MAX_CAPTAIN];
} single_container_t;

typedef union single_container_converter_u {
    single_container_t single_container;
    void* container;
} single_container_converter_t;

static inline single_container_t container_to_single(const void* container) {
    single_container_converter_t convert;
    convert.container = (void*)container;
    return convert.single_container;
}

static inline void* single_to_container(single_container_t single) {
    single_container_converter_t convert;
    convert.single_container = single;
    return convert.container;
}

static inline int32_t single_container_size_in_bytes(single_container_t container) {
    return container.len * sizeof(uint16_t);
}

static inline bool single_container_nozero_cardinality(single_container_t container) {
    return container.len > 0;
}

static inline int32_t single_container_cardinality(single_container_t container) {
    return container.len;
}

static inline int32_t single_container_write(single_container_t container, char* buf) {
    const int32_t copy_length = single_container_size_in_bytes(container);
    memcpy(buf, container.vals, copy_length);
    return copy_length;
}

static inline int32_t single_container_serialization_len(
    single_container_t container) {
    return sizeof(uint16_t) + (sizeof(uint16_t) * container.len);
}

static inline int single_container_to_uint32_array(void* vout, single_container_t cont,
                                            uint32_t base) {
    int outpos = 0;
    uint32_t* out = (uint32_t*)vout;
    const uint16_t* array = cont.vals;
    int len = cont.len;
    for (int i = 0; i < len; ++i) {
        const uint32_t val = base + array[i];
        memcpy(out + outpos, &val, sizeof(uint32_t));
        outpos++;
    }
    return outpos;
}

// return -1 exceed
// return 1 add succ
// return 0 ignore
// TODO do better
static inline int single_container_try_add(single_container_t* container,
                                    uint16_t val) {
    assert(false);
    __builtin_unreachable();
    uint16_t* array = container->vals;
    uint8_t len = container->len;
    if ((len == 0 || array[len - 1] < val) &&
        len < SINGLE_CONTAINER_MAX_CAPTAIN) {
        array[len] = val;
        container->len++;
        return 1;
    }
    for (uint8_t i = 0; i < len; ++i) {
        if (array[i] == val) return 0;
        if (array[i] > val) {
            if (len < SINGLE_CONTAINER_MAX_CAPTAIN) {
                for (int j = len - 1; j >= i; --j) {
                    array[len] = array[len - 1];
                }
                array[i] = val;
                container->len++;
                return 1;
            } else {
                return -1;
            }
        }
    }
    // assert(false);
    __builtin_unreachable();
}

void* single_to_array(single_container_t container, uint16_t extra_val);

inline void single_container_clear(single_container_t* container) {
    memset(container, 0, sizeof(single_container_t));
}

static inline int32_t single_container_read(uint32_t card,
                                     single_container_t* container,
                                     const char* buf) {
    int32_t byte_sz = card * sizeof(uint16_t);
    container->len = card;
    memcpy(container->vals, buf, byte_sz);
    return byte_sz;
}

static inline int32_t single_container_serialize(single_container_t container,
                                          char* buf) {
    __builtin_unreachable();
    int32_t l;
    uint32_t cardinality = (uint16_t)container.len;
    int32_t offset = sizeof(cardinality);
    memcpy(buf, &cardinality, offset);
    l = sizeof(uint16_t) * container.len;
    if (l) memcpy(&buf[offset], container.vals, l);
    return offset + l;
}

static inline void* single_container_deserialize(const char* buf, size_t buf_len) {
    __builtin_unreachable();
    single_container_t single;
    assert(buf_len >= 2);
    buf_len -= 2;
    size_t len;
    int32_t off;
    uint16_t cardinality;

    memcpy(&cardinality, buf, off = sizeof(cardinality));
    single.len = cardinality;
    len = sizeof(uint16_t) * cardinality;
    if (len) memcpy(single.vals, &buf[off], len);

    return single_to_container(single);
}

static inline uint16_t single_container_maximum(const single_container_t single) {
    if (single.len == 0) return 0;
    return single.vals[single.len - 1];
}

void* single_single_container_inplace_union(void* left, const void* right,
                                            uint8_t* typecode);
void* single_array_container_inplace_union(void* left,
                                           const array_container_t* right,
                                           uint8_t* typecode);
void* array_single_container_inplace_union(array_container_t* arr,
                                           const void* left, uint8_t* typecode);

#endif