#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/containers/bitset.h>
#include <roaring/containers/containers.h>
#include <roaring/roaring_array.h>

// Convention: [0,ra->size) all elements are initialized
//  [ra->size, ra->allocation_size) is junk and contains nothing needing freeing

extern inline int32_t ra_get_size(const roaring_array_t *ra);
extern inline int32_t ra_get_index(const roaring_array_t *ra, uint16_t x);
extern inline void *ra_get_container_at_index(const roaring_array_t *ra,
                                              uint16_t i, uint8_t *typecode);
extern inline void ra_unshare_container_at_index(roaring_array_t *ra,
                                                 uint16_t i);
extern inline void ra_replace_key_and_container_at_index(roaring_array_t *ra,
                                                         int32_t i,
                                                         uint16_t key, void *c,
                                                         uint8_t typecode);
extern inline void ra_set_container_at_index(const roaring_array_t *ra,
                                             int32_t i, void *c,
                                             uint8_t typecode);

#define INITIAL_CAPACITY 4

static bool realloc_array(roaring_array_t *ra, size_t new_capacity) {
    // because we combine the allocations, it is not possible to use realloc
    /*ra->keys =
    (uint16_t *)realloc(ra->keys, sizeof(uint16_t) * new_capacity);
ra->containers =
    (void **)realloc(ra->containers, sizeof(void *) * new_capacity);
ra->typecodes =
    (uint8_t *)realloc(ra->typecodes, sizeof(uint8_t) * new_capacity);
if (!ra->keys || !ra->containers || !ra->typecodes) {
    free(ra->keys);
    free(ra->containers);
    free(ra->typecodes);
    return false;
}*/

    if ( new_capacity == 0 ) {
      free(ra->containers);
      ra->containers = NULL;
      ra->keys = NULL;
      ra->typecodes = NULL;
      ra->allocation_size = 0;
      return true;
    }
    const size_t memoryneeded =
        new_capacity * (sizeof(uint16_t) + sizeof(void *) + sizeof(uint8_t));
    void *bigalloc = malloc(memoryneeded);
    if (!bigalloc) return false;
    void *oldbigalloc = ra->containers;
    void **newcontainers = (void **)bigalloc;
    uint16_t *newkeys = (uint16_t *)(newcontainers + new_capacity);
    uint8_t *newtypecodes = (uint8_t *)(newkeys + new_capacity);
    assert((char *)(newtypecodes + new_capacity) ==
           (char *)bigalloc + memoryneeded);
    memcpy(newcontainers, ra->containers, sizeof(void *) * ra->size);
    memcpy(newkeys, ra->keys, sizeof(uint16_t) * ra->size);
    memcpy(newtypecodes, ra->typecodes, sizeof(uint8_t) * ra->size);
    ra->containers = newcontainers;
    ra->keys = newkeys;
    ra->typecodes = newtypecodes;
    free(oldbigalloc);
    return true;
}

bool ra_init_with_capacity(roaring_array_t *new_ra, uint32_t cap) {
    if (!new_ra) return false;
    new_ra->keys = NULL;
    new_ra->containers = NULL;
    new_ra->typecodes = NULL;

    new_ra->allocation_size = cap;
    new_ra->size = 0;
    if(cap > 0) {
      void *bigalloc =
        malloc(cap * (sizeof(uint16_t) + sizeof(void *) + sizeof(uint8_t)));
      if( bigalloc == NULL ) return false;
      new_ra->containers = (void **)bigalloc;
      new_ra->keys = (uint16_t *)(new_ra->containers + cap);
      new_ra->typecodes = (uint8_t *)(new_ra->keys + cap);
    }
    return true;
}

int ra_shrink_to_fit(roaring_array_t *ra) {
    int savings = (ra->allocation_size - ra->size) *
                  (sizeof(uint16_t) + sizeof(void *) + sizeof(uint8_t));
    if (!realloc_array(ra, ra->size)) {
      return 0;
    }
    ra->allocation_size = ra->size;
    return savings;
}

bool ra_init(roaring_array_t *t) {
    return ra_init_with_capacity(t, INITIAL_CAPACITY);
}

bool ra_copy(const roaring_array_t *source, roaring_array_t *dest,
             bool copy_on_write) {
    if (!ra_init_with_capacity(dest, source->size)) return false;
    dest->size = source->size;
    dest->allocation_size = source->size;
    memcpy(dest->keys, source->keys, dest->size * sizeof(uint16_t));
    // we go through the containers, turning them into shared containers...
    if (copy_on_write) {
        for (int32_t i = 0; i < dest->size; ++i) {
            source->containers[i] = get_copy_of_container(
                source->containers[i], &source->typecodes[i], copy_on_write);
        }
        // we do a shallow copy to the other bitmap
        memcpy(dest->containers, source->containers,
               dest->size * sizeof(void *));
        memcpy(dest->typecodes, source->typecodes,
               dest->size * sizeof(uint8_t));
    } else {
        memcpy(dest->typecodes, source->typecodes,
               dest->size * sizeof(uint8_t));
        for (int32_t i = 0; i < dest->size; i++) {
            dest->containers[i] =
                container_clone(source->containers[i], source->typecodes[i]);
            if (dest->containers[i] == NULL) {
                for (int32_t j = 0; j < i; j++) {
                    container_free(dest->containers[j], dest->typecodes[j]);
                }
                ra_clear_without_containers(dest);
                return false;
            }
        }
    }
    return true;
}

bool ra_overwrite(const roaring_array_t *source, roaring_array_t *dest,
                  bool copy_on_write) {
    ra_clear_containers(dest);  // we are going to overwrite them
    if (dest->allocation_size < source->size) {
        if (!realloc_array(dest, source->size)) {
            return false;
        }
    }
    dest->size = source->size;
    memcpy(dest->keys, source->keys, dest->size * sizeof(uint16_t));
    // we go through the containers, turning them into shared containers...
    if (copy_on_write) {
        for (int32_t i = 0; i < dest->size; ++i) {
            source->containers[i] = get_copy_of_container(
                source->containers[i], &source->typecodes[i], copy_on_write);
        }
        // we do a shallow copy to the other bitmap
        memcpy(dest->containers, source->containers,
               dest->size * sizeof(void *));
        memcpy(dest->typecodes, source->typecodes,
               dest->size * sizeof(uint8_t));
    } else {
        memcpy(dest->typecodes, source->typecodes,
               dest->size * sizeof(uint8_t));
        for (int32_t i = 0; i < dest->size; i++) {
            dest->containers[i] =
                container_clone(source->containers[i], source->typecodes[i]);
            if (dest->containers[i] == NULL) {
                for (int32_t j = 0; j < i; j++) {
                    container_free(dest->containers[j], dest->typecodes[j]);
                }
                ra_clear_without_containers(dest);
                return false;
            }
        }
    }
    return true;
}

void ra_clear_containers(roaring_array_t *ra) {
    for (int32_t i = 0; i < ra->size; ++i) {
        container_free(ra->containers[i], ra->typecodes[i]);
    }
}

void ra_reset(roaring_array_t *ra) {
  ra_clear_containers(ra);
  ra->size = 0;
  ra_shrink_to_fit(ra);
}

void ra_clear_without_containers(roaring_array_t *ra) {
    free(ra->containers);   // keys and typecodes are allocated with containers
    ra->keys = NULL;        // paranoid
    ra->containers = NULL;  // paranoid
    ra->typecodes = NULL;   // paranoid
}

void ra_clear(roaring_array_t *ra) {
    ra_clear_containers(ra);
    ra_clear_without_containers(ra);
}

bool extend_array(roaring_array_t *ra, int32_t k) {
    int32_t desired_size = ra->size + k;
    if (desired_size > ra->allocation_size) {
        size_t new_capacity =
            (ra->size < 1024) ? 2 * desired_size : 5 * desired_size / 4;

        return realloc_array(ra, new_capacity);
    }
    return true;
}

void ra_append(roaring_array_t *ra, uint16_t key, void *container,
               uint8_t typecode) {
    extend_array(ra, 1);
    const int32_t pos = ra->size;

    ra->keys[pos] = key;
    ra->containers[pos] = container;
    ra->typecodes[pos] = typecode;
    ra->size++;
}

void ra_append_copy(roaring_array_t *ra, const roaring_array_t *sa,
                    uint16_t index, bool copy_on_write) {
    extend_array(ra, 1);
    const int32_t pos = ra->size;

    // old contents is junk not needing freeing
    ra->keys[pos] = sa->keys[index];
    // the shared container will be in two bitmaps
    if (copy_on_write) {
        sa->containers[index] = get_copy_of_container(
            sa->containers[index], &sa->typecodes[index], copy_on_write);
        ra->containers[pos] = sa->containers[index];
        ra->typecodes[pos] = sa->typecodes[index];
    } else {
        ra->containers[pos] =
            container_clone(sa->containers[index], sa->typecodes[index]);
        ra->typecodes[pos] = sa->typecodes[index];
    }
    ra->size++;
}

void ra_append_copies_until(roaring_array_t *ra, const roaring_array_t *sa,
                            uint16_t stopping_key, bool copy_on_write) {
    for (uint16_t i = 0; i < sa->size; ++i) {
        if (sa->keys[i] >= stopping_key) break;
        ra_append_copy(ra, sa, i, copy_on_write);
    }
}

void ra_append_copy_range(roaring_array_t *ra, const roaring_array_t *sa,
                          uint16_t start_index, uint16_t end_index,
                          bool copy_on_write) {
    extend_array(ra, end_index - start_index);

    for (uint16_t i = start_index; i < end_index; ++i) {
        const int32_t pos = ra->size;
        ra->keys[pos] = sa->keys[i];
        if (copy_on_write) {
            sa->containers[i] = get_copy_of_container(
                sa->containers[i], &sa->typecodes[i], copy_on_write);
            ra->containers[pos] = sa->containers[i];
            ra->typecodes[pos] = sa->typecodes[i];
        } else {
            ra->containers[pos] =
                container_clone(sa->containers[i], sa->typecodes[i]);
            ra->typecodes[pos] = sa->typecodes[i];
        }
        ra->size++;
    }
}

void ra_append_copies_after(roaring_array_t *ra, const roaring_array_t *sa,
                            uint16_t before_start, bool copy_on_write) {
    int start_location = ra_get_index(sa, before_start);
    if (start_location >= 0)
        ++start_location;
    else
        start_location = -start_location - 1;
    ra_append_copy_range(ra, sa, start_location, sa->size, copy_on_write);
}

void ra_append_move_range(roaring_array_t *ra, roaring_array_t *sa,
                          uint16_t start_index, uint16_t end_index) {
    extend_array(ra, end_index - start_index);

    for (uint16_t i = start_index; i < end_index; ++i) {
        const int32_t pos = ra->size;

        ra->keys[pos] = sa->keys[i];
        ra->containers[pos] = sa->containers[i];
        ra->typecodes[pos] = sa->typecodes[i];
        ra->size++;
    }
}

void ra_append_range(roaring_array_t *ra, roaring_array_t *sa,
                     uint16_t start_index, uint16_t end_index,
                     bool copy_on_write) {
    extend_array(ra, end_index - start_index);

    for (uint16_t i = start_index; i < end_index; ++i) {
        const int32_t pos = ra->size;
        ra->keys[pos] = sa->keys[i];
        if (copy_on_write) {
            sa->containers[i] = get_copy_of_container(
                sa->containers[i], &sa->typecodes[i], copy_on_write);
            ra->containers[pos] = sa->containers[i];
            ra->typecodes[pos] = sa->typecodes[i];
        } else {
            ra->containers[pos] =
                container_clone(sa->containers[i], sa->typecodes[i]);
            ra->typecodes[pos] = sa->typecodes[i];
        }
        ra->size++;
    }
}

void *ra_get_container(roaring_array_t *ra, uint16_t x, uint8_t *typecode) {
    int i = binarySearch(ra->keys, (int32_t)ra->size, x);
    if (i < 0) return NULL;
    *typecode = ra->typecodes[i];
    return ra->containers[i];
}

extern void *ra_get_container_at_index(const roaring_array_t *ra, uint16_t i,
                                       uint8_t *typecode);

void *ra_get_writable_container(roaring_array_t *ra, uint16_t x,
                                uint8_t *typecode) {
    int i = binarySearch(ra->keys, (int32_t)ra->size, x);
    if (i < 0) return NULL;
    *typecode = ra->typecodes[i];
    return get_writable_copy_if_shared(ra->containers[i], typecode);
}

void *ra_get_writable_container_at_index(roaring_array_t *ra, uint16_t i,
                                         uint8_t *typecode) {
    assert(i < ra->size);
    *typecode = ra->typecodes[i];
    return get_writable_copy_if_shared(ra->containers[i], typecode);
}

uint16_t ra_get_key_at_index(const roaring_array_t *ra, uint16_t i) {
    return ra->keys[i];
}

extern int32_t ra_get_index(const roaring_array_t *ra, uint16_t x);

extern int32_t ra_advance_until(const roaring_array_t *ra, uint16_t x,
                                int32_t pos);

// everything skipped over is freed
int32_t ra_advance_until_freeing(roaring_array_t *ra, uint16_t x, int32_t pos) {
    while (pos < ra->size && ra->keys[pos] < x) {
        container_free(ra->containers[pos], ra->typecodes[pos]);
        ++pos;
    }
    return pos;
}

void ra_insert_new_key_value_at(roaring_array_t *ra, int32_t i, uint16_t key,
                                void *container, uint8_t typecode) {
    extend_array(ra, 1);
    // May be an optimization opportunity with DIY memmove
    memmove(&(ra->keys[i + 1]), &(ra->keys[i]),
            sizeof(uint16_t) * (ra->size - i));
    memmove(&(ra->containers[i + 1]), &(ra->containers[i]),
            sizeof(void *) * (ra->size - i));
    memmove(&(ra->typecodes[i + 1]), &(ra->typecodes[i]),
            sizeof(uint8_t) * (ra->size - i));
    ra->keys[i] = key;
    ra->containers[i] = container;
    ra->typecodes[i] = typecode;
    ra->size++;
}

// note: Java routine set things to 0, enabling GC.
// Java called it "resize" but it was always used to downsize.
// Allowing upsize would break the conventions about
// valid containers below ra->size.

void ra_downsize(roaring_array_t *ra, int32_t new_length) {
    assert(new_length <= ra->size);
    ra->size = new_length;
}

void ra_remove_at_index(roaring_array_t *ra, int32_t i) {
    memmove(&(ra->containers[i]), &(ra->containers[i + 1]),
            sizeof(void *) * (ra->size - i - 1));
    memmove(&(ra->keys[i]), &(ra->keys[i + 1]),
            sizeof(uint16_t) * (ra->size - i - 1));
    memmove(&(ra->typecodes[i]), &(ra->typecodes[i + 1]),
            sizeof(uint8_t) * (ra->size - i - 1));
    ra->size--;
}

void ra_remove_at_index_and_free(roaring_array_t *ra, int32_t i) {
    container_free(ra->containers[i], ra->typecodes[i]);
    ra_remove_at_index(ra, i);
}

// used in inplace andNot only, to slide left the containers from
// the mutated RoaringBitmap that are after the largest container of
// the argument RoaringBitmap.  In use it should be followed by a call to
// downsize.
//
void ra_copy_range(roaring_array_t *ra, uint32_t begin, uint32_t end,
                   uint32_t new_begin) {
    assert(begin <= end);
    assert(new_begin < begin);

    const int range = end - begin;

    // We ensure to previously have freed overwritten containers
    // that are not copied elsewhere

    memmove(&(ra->containers[new_begin]), &(ra->containers[begin]),
            sizeof(void *) * range);
    memmove(&(ra->keys[new_begin]), &(ra->keys[begin]),
            sizeof(uint16_t) * range);
    memmove(&(ra->typecodes[new_begin]), &(ra->typecodes[begin]),
            sizeof(uint8_t) * range);
}

size_t ra_size_in_bytes(roaring_array_t *ra) {
    size_t cardinality = 0;
    size_t tot_len =
        1 /* initial byte type */ + 4 /* tot_len */ + sizeof(roaring_array_t) +
        ra->size * (sizeof(uint16_t) + sizeof(void *) + sizeof(uint8_t));
    for (int32_t i = 0; i < ra->size; i++) {
        tot_len +=
            (container_serialization_len(ra->containers[i], ra->typecodes[i]) +
             sizeof(uint16_t));
        cardinality +=
            container_get_cardinality(ra->containers[i], ra->typecodes[i]);
    }

    if ((cardinality * sizeof(uint32_t) + sizeof(uint32_t)) < tot_len) {
        return cardinality * sizeof(uint32_t) + 1 + sizeof(uint32_t);
    }
    return tot_len;
}

void ra_to_uint32_array(const roaring_array_t *ra, uint32_t *ans) {
    size_t ctr = 0;
    for (int i = 0; i < ra->size; ++i) {
        int num_added = container_to_uint32_array(
            ans + ctr, ra->containers[i], ra->typecodes[i],
            ((uint32_t)ra->keys[i]) << 16);
        ctr += num_added;
    }
}

bool ra_has_run_container(const roaring_array_t *ra) {
    for (int32_t k = 0; k < ra->size; ++k) {
        if (get_container_type(ra->containers[k], ra->typecodes[k]) ==
            RUN_CONTAINER_TYPE_CODE)
            return true;
    }
    return false;
}

uint32_t ra_portable_header_size(const roaring_array_t *ra) {
    if (ra_has_run_container(ra)) {
        if (ra->size <
            NO_OFFSET_THRESHOLD) {  // for small bitmaps, we omit the offsets
            return 4 + (ra->size + 7) / 8 + 4 * ra->size;
        }
        return 4 + (ra->size + 7) / 8 +
               8 * ra->size;  // - 4 because we pack the size with the cookie
    } else {
        return 4 + 4 + 8 * ra->size;
    }
}

size_t ra_portable_size_in_bytes(const roaring_array_t *ra) {
    size_t count = ra_portable_header_size(ra);

    for (int32_t k = 0; k < ra->size; ++k) {
        count += container_size_in_bytes(ra->containers[k], ra->typecodes[k]);
    }
    return count;
}

size_t ra_portable_serialize(const roaring_array_t *ra, char *buf) {
    char *initbuf = buf;
    uint32_t startOffset = 0;
    bool hasrun = ra_has_run_container(ra);
    if (hasrun) {
        uint32_t cookie = SERIAL_COOKIE | ((ra->size - 1) << 16);
        memcpy(buf, &cookie, sizeof(cookie));
        buf += sizeof(cookie);
        uint32_t s = (ra->size + 7) / 8;
        uint8_t *bitmapOfRunContainers = (uint8_t *)calloc(s, 1);
        assert(bitmapOfRunContainers != NULL);  // todo: handle
        for (int32_t i = 0; i < ra->size; ++i) {
            if (get_container_type(ra->containers[i], ra->typecodes[i]) ==
                RUN_CONTAINER_TYPE_CODE) {
                bitmapOfRunContainers[i / 8] |= (1 << (i % 8));
            }
        }
        memcpy(buf, bitmapOfRunContainers, s);
        buf += s;
        free(bitmapOfRunContainers);
        if (ra->size < NO_OFFSET_THRESHOLD) {
            startOffset = 4 + 4 * ra->size + s;
        } else {
            startOffset = 4 + 8 * ra->size + s;
        }
    } else {  // backwards compatibility
        uint32_t cookie = SERIAL_COOKIE_NO_RUNCONTAINER;

        memcpy(buf, &cookie, sizeof(cookie));
        buf += sizeof(cookie);
        memcpy(buf, &ra->size, sizeof(ra->size));
        buf += sizeof(ra->size);

        startOffset = 4 + 4 + 4 * ra->size + 4 * ra->size;
    }
    for (int32_t k = 0; k < ra->size; ++k) {
        memcpy(buf, &ra->keys[k], sizeof(ra->keys[k]));
        buf += sizeof(ra->keys[k]);
        // get_cardinality returns a value in [1,1<<16], subtracting one
        // we get [0,1<<16 - 1] which fits in 16 bits
        uint16_t card = (uint16_t)(
            container_get_cardinality(ra->containers[k], ra->typecodes[k]) - 1);
        memcpy(buf, &card, sizeof(card));
        buf += sizeof(card);
    }
    if ((!hasrun) || (ra->size >= NO_OFFSET_THRESHOLD)) {
        // writing the containers offsets
        for (int32_t k = 0; k < ra->size; k++) {
            memcpy(buf, &startOffset, sizeof(startOffset));
            buf += sizeof(startOffset);
            startOffset =
                startOffset +
                container_size_in_bytes(ra->containers[k], ra->typecodes[k]);
        }
    }
    for (int32_t k = 0; k < ra->size; ++k) {
        buf += container_write(ra->containers[k], ra->typecodes[k], buf);
    }
    return buf - initbuf;
}

bool ra_portable_deserialize(roaring_array_t *answer, const char *buf) {
    uint32_t cookie;
    memcpy(&cookie, buf, sizeof(int32_t));
    buf += sizeof(uint32_t);
    if ((cookie & 0xFFFF) != SERIAL_COOKIE &&
        cookie != SERIAL_COOKIE_NO_RUNCONTAINER) {
        fprintf(stderr, "I failed to find one of the right cookies. Found %u\n",
                cookie);
        return false;
    }
    int32_t size;

    if ((cookie & 0xFFFF) == SERIAL_COOKIE)
        size = (cookie >> 16) + 1;
    else {
        memcpy(&size, buf, sizeof(int32_t));
        buf += sizeof(uint32_t);
    }
    bool is_ok = ra_init_with_capacity(answer, size);
    if (!is_ok) {
        fprintf(stderr, "Failed to allocate memory early on. Bailing out.\n");
        return false;
    }
    answer->size = size;
    char *bitmapOfRunContainers = NULL;
    bool hasrun = (cookie & 0xFFFF) == SERIAL_COOKIE;
    if (hasrun) {
        int32_t s = (size + 7) / 8;
        bitmapOfRunContainers = (char *)malloc((size + 7) / 8);
        assert(bitmapOfRunContainers != NULL);  // todo: handle
        memcpy(bitmapOfRunContainers, buf, s);
        buf += s;
    }
    uint16_t *keys = answer->keys;
    int32_t *cardinalities = (int32_t *)malloc(
        size * (sizeof(int32_t) + sizeof(bool)));  // one malloc
    assert(cardinalities != NULL);                 // todo: handle
    bool *isBitmap = (bool *)(cardinalities + size);
    uint16_t tmp;
    for (int32_t k = 0; k < size; ++k) {
        memcpy(&keys[k], buf, sizeof(keys[k]));
        buf += sizeof(keys[k]);
        memcpy(&tmp, buf, sizeof(tmp));
        buf += sizeof(tmp);
        cardinalities[k] = 1 + tmp;
        isBitmap[k] = cardinalities[k] > DEFAULT_MAX_SIZE;
        if (bitmapOfRunContainers != NULL &&
            (bitmapOfRunContainers[k / 8] & (1 << (k % 8))) != 0) {
            isBitmap[k] = false;
        }
    }
    if ((!hasrun) || (size >= NO_OFFSET_THRESHOLD)) {
        // skipping the offsets
        buf += size * 4;
    }
    // Reading the containers
    for (int32_t k = 0; k < size; ++k) {
        if (isBitmap[k]) {
            bitset_container_t *c = bitset_container_create();
            assert(c != NULL);  // todo: handle
            buf += bitset_container_read(cardinalities[k], c, buf);
            answer->containers[k] = c;
            answer->typecodes[k] = BITSET_CONTAINER_TYPE_CODE;
        } else if (bitmapOfRunContainers != NULL &&
                   ((bitmapOfRunContainers[k / 8] & (1 << (k % 8))) != 0)) {
            run_container_t *c = run_container_create();
            assert(c != NULL);  // todo: handle
            buf += run_container_read(cardinalities[k], c, buf);
            answer->containers[k] = c;
            answer->typecodes[k] = RUN_CONTAINER_TYPE_CODE;
        } else {
            array_container_t *c =
                array_container_create_given_capacity(cardinalities[k]);
            assert(c != NULL);  // todo: handle
            buf += array_container_read(cardinalities[k], c, buf);
            answer->containers[k] = c;
            answer->typecodes[k] = ARRAY_CONTAINER_TYPE_CODE;
        }
    }
    free(bitmapOfRunContainers);
    free(cardinalities);  // isBitmap fits in there
    return true;
}
