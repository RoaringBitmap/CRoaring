#ifndef ROARING_INTERNAL_H
#define ROARING_INTERNAL_H

#ifdef __cplusplus
extern "C" {
namespace roaring {
namespace internal {
#endif

static inline bool container_iterator_next_inline(
    const container_t *c, uint8_t typecode, roaring_container_iterator_t *it,
    uint16_t *value) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE: {
            const bitset_container_t *bc = const_CAST_bitset(c);
            it->index++;

            uint32_t wordindex = it->index / 64;
            if (wordindex >= BITSET_CONTAINER_SIZE_IN_WORDS) {
                return false;
            }

            uint64_t word =
                bc->words[wordindex] & (UINT64_MAX << (it->index % 64));
            // next part could be optimized/simplified
            while (word == 0 &&
                   (wordindex + 1 < BITSET_CONTAINER_SIZE_IN_WORDS)) {
                wordindex++;
                word = bc->words[wordindex];
            }
            if (word != 0) {
                it->index = wordindex * 64 + roaring_trailing_zeroes(word);
                *value = it->index;
                return true;
            }
            return false;
        }
        case ARRAY_CONTAINER_TYPE: {
            const array_container_t *ac = const_CAST_array(c);
            it->index++;
            if (it->index < ac->cardinality) {
                *value = ac->array[it->index];
                return true;
            }
            return false;
        }
        case RUN_CONTAINER_TYPE: {
            if (*value == UINT16_MAX) {  // Avoid overflow to zero
                return false;
            }

            const run_container_t *rc = const_CAST_run(c);
            uint32_t limit =
                rc->runs[it->index].value + rc->runs[it->index].length;
            if (*value < limit) {
                (*value)++;
                return true;
            }

            it->index++;
            if (it->index < rc->n_runs) {
                *value = rc->runs[it->index].value;
                return true;
            }
            return false;
        }
        default:
            assert(false);
            roaring_unreachable;
            return false;
    }
}

static inline bool container_iterator_prev_inline(
    const container_t *c, uint8_t typecode, roaring_container_iterator_t *it,
    uint16_t *value) {
    switch (typecode) {
        case BITSET_CONTAINER_TYPE: {
            if (--it->index < 0) {
                return false;
            }

            const bitset_container_t *bc = const_CAST_bitset(c);
            int32_t wordindex = it->index / 64;
            uint64_t word =
                bc->words[wordindex] & (UINT64_MAX >> (63 - (it->index % 64)));

            while (word == 0 && --wordindex >= 0) {
                word = bc->words[wordindex];
            }
            if (word == 0) {
                return false;
            }

            it->index = (wordindex * 64) + (63 - roaring_leading_zeroes(word));
            *value = it->index;
            return true;
        }
        case ARRAY_CONTAINER_TYPE: {
            if (--it->index < 0) {
                return false;
            }
            const array_container_t *ac = const_CAST_array(c);
            *value = ac->array[it->index];
            return true;
        }
        case RUN_CONTAINER_TYPE: {
            if (*value == 0) {
                return false;
            }

            const run_container_t *rc = const_CAST_run(c);
            (*value)--;
            if (*value >= rc->runs[it->index].value) {
                return true;
            }

            if (--it->index < 0) {
                return false;
            }

            *value = rc->runs[it->index].value + rc->runs[it->index].length;
            return true;
        }
        default:
            assert(false);
            roaring_unreachable;
            return false;
    }
}

#ifdef __cplusplus
}
}
}  // extern "C" { namespace roaring { namespace internal {
#endif

#endif  // ROARING_INTERNAL_H