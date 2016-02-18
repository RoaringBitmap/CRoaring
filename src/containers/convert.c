#include "convert.h"
#include "util.h"

bitset_container_t *bitset_container_from_array(array_container_t *a) {
    bitset_container_t *ans = bitset_container_create();
    int limit = array_container_cardinality(a);
    for (int i = 0; i < limit; ++i) bitset_container_set(ans, a->array[i]);
    return ans;
}

bitset_container_t *bitset_container_from_run(run_container_t *arr) {
    int card = run_container_cardinality(arr);
    bitset_container_t *answer = bitset_container_create();
    for (int rlepos = 0; rlepos < arr->n_runs; ++rlepos) {
        rle16_t vl = arr->valueslength[rlepos];
        bitset_set_range(answer->array, vl.value, vl.value + vl.length + 1);
    }
    answer->cardinality = card;
    return answer;
}

array_container_t *array_container_from_bitset(bitset_container_t *bits) {
    array_container_t *result =
        array_container_create_given_capacity(bits->cardinality);
    result->cardinality = bits->cardinality;
    int outpos = 0;
    uint16_t *out = result->array;
    for (int i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; ++i) {
        uint64_t w = bits->array[i];
        while (w != 0) {
            uint64_t t = w & -w;
            int r = __builtin_ctzl(w);
            out[outpos++] = i * 64 + r;
            w ^= t;
        }
    }
    return result;
}
