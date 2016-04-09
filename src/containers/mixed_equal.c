#include "containers/mixed_equal.h"

bool array_container_equal_bitset(array_container_t* container1,
                                  bitset_container_t* container2) {
    if (container2->cardinality != BITSET_UNKNOWN_CARDINALITY) {
        if (container2->cardinality != container1->cardinality) {
            return false;
        }
    }
    int32_t pos = 0;
    for (int32_t i = 0; i < BITSET_CONTAINER_SIZE_IN_WORDS; ++i) {
        uint64_t w = container2->array[i];
        while (w != 0) {
            uint64_t t = w & -w;
            uint16_t r = i * 64 + __builtin_ctzl(w);
            if (pos >= container1->cardinality) {
                return false;
            }
            if (container1->array[pos] != r) {
                return false;
            }
            ++pos;
            w ^= t;
        }
    }
    return (pos == container1->cardinality);
}

bool run_container_equals_array(run_container_t* container1,
                                array_container_t* container2) {
    if (run_container_cardinality(container1) != container2->cardinality)
        return false;
    int32_t pos = 0;
    for (int i = 0; i < container1->n_runs; ++i) {
        uint32_t run_start = container1->runs[i].value;
        uint32_t le = container1->runs[i].length;

        for (uint32_t j = run_start; j <= run_start + le; ++j) {
            if (pos >= container2->cardinality) {
                return false;
            }
            if (container2->array[pos] != j) {
                return false;
            }
            ++pos;
        }
    }
    return (pos == container2->cardinality);
}

bool run_container_equals_bitset(run_container_t* container1,
                                 bitset_container_t* container2) {
    if (container2->cardinality != BITSET_UNKNOWN_CARDINALITY) {
        if (container2->cardinality != run_container_cardinality(container1)) {
            return false;
        }
    } else {
        int32_t card = bitset_container_compute_cardinality(
            container2);  // modify container2?
        if (card != run_container_cardinality(container1)) {
            return false;
        }
    }
    for (int i = 0; i < container1->n_runs; ++i) {
        uint32_t run_start = container1->runs[i].value;
        uint32_t le = container1->runs[i].length;
        for (uint32_t j = run_start; j <= run_start + le; ++j) {
            // todo: this code could be much faster
            if (!bitset_container_contains(container2, j)) {
                return false;
            }
        }
    }
    return true;
}
