#include <roaring/containers/array.h>
#include <roaring/containers/containers.h>
#include <roaring/containers/single.h>

void* single_to_array(single_container_t container, uint16_t extra_val) {
    __builtin_unreachable();
    // assert SINGLE_CONTAINER_MAX_CAPTAIN ==
    array_container_t* arr =
        array_container_create_given_capacity(2 * SINGLE_CONTAINER_MAX_CAPTAIN);
    memcpy(arr->array, container.vals, container.len * sizeof(uint16_t));
    arr->cardinality = container.len;
    array_container_add(arr, extra_val);
    return arr;
}

void* single_single_container_inplace_union(void* left, const void* right,
                                            uint8_t* typecode) {
    single_container_t l = container_to_single(left);
    single_container_t r = container_to_single(right);

    if (l.len + r.len <= SINGLE_CONTAINER_MAX_CAPTAIN) {
    // if (false) {
        *typecode = SINGLE_CONTAINER_TYPE_CODE;
        single_container_t res;
        memset(&res, 0, sizeof(single_container_t));
        int lcur = 0;
        int rcur = 0;
        int pos = 0;
        for (; lcur < l.len && rcur < r.len;) {
            if(l.vals[lcur] < r.vals[rcur]) {
                res.vals[pos++] = l.vals[lcur++];
            } else if(l.vals[lcur] > r.vals[rcur]) {
                res.vals[pos++] = r.vals[rcur++];
            } else {
                res.vals[pos++] = l.vals[lcur];
                lcur++;
                rcur++;
            }
        }
        for (; lcur < l.len;) {
            res.vals[pos++] = l.vals[lcur++];
        }
        for (; rcur < r.len;) {
            res.vals[pos++] = r.vals[rcur++];
        }
        res.len = pos;
        // check assert
        assert(res.len <= SINGLE_CONTAINER_MAX_CAPTAIN);
        for(int i = 0; i + 1 < res.len; ++i) {
            assert(res.vals[i] < res.vals[i + 1]);
        }

        return single_to_container(res);
    }

    array_container_t* arr = array_container_create();
    *typecode = ARRAY_CONTAINER_TYPE_CODE;
    int lcur = 0;
    int rcur = 0;

    for (; lcur < l.len && rcur < r.len;) {
        if (l.vals[lcur] < r.vals[rcur]) {
            array_container_add(arr, l.vals[lcur++]);
        } else if (l.vals[lcur] > r.vals[rcur]) {
            array_container_add(arr, r.vals[rcur++]);
        } else {
            array_container_add(arr, r.vals[rcur]);
            lcur++;
            rcur++;
        }
    }
    for (; lcur < l.len;) {
        array_container_add(arr, l.vals[lcur++]);
    }
    for (; rcur < r.len;) {
        array_container_add(arr, r.vals[rcur++]);
    }
    return arr;
}

void* single_array_container_inplace_union(void* left,
                                           const array_container_t* right,
                                           uint8_t* typecode) {
    *typecode = ARRAY_CONTAINER_TYPE_CODE;
    single_container_t l = container_to_single(left);
    array_container_t* arr = array_container_create_given_capacity(l.len + right->cardinality);
    array_container_copy(right, arr);
    for(uint16_t i = 0;i < l.len; ++ i) {
        array_container_add(arr, l.vals[i]);
    }
    return arr;
}

void* array_single_container_inplace_union(array_container_t* arr, const void* right,
                                           uint8_t* typecode) {
    single_container_t r = container_to_single(right);
    int car1 = arr->cardinality;
    int car2 = r.len;
    *typecode = ARRAY_CONTAINER_TYPE_CODE;
    int32_t capacity = r.len + arr->cardinality;
    if (arr->capacity < capacity) {
        array_container_grow(arr, capacity, true);
    }
    for (uint16_t i = 0;i < r.len; ++ i) {
        array_container_add(arr, r.vals[i]);
    }
    int car3 = arr->cardinality;
    // assert(car1 + car2 == car3);
    return arr;
}