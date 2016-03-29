#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitset.h"
#include "containers.h"
#include "roaring_array.h"

// ported from RoaringArray.java
// Todo: optimization (eg branchless binary search)
// Go version has copy-on-write, has combo binary/sequential search
// AND: fast SIMD and on key sets; containerwise AND; SIMD partial sum
//    with +1 for nonempty containers, 0 for empty containers
//    then use this to pack the arrays for the result.

// Convention: [0,ra->size) all elements are initialized
//  [ra->size, ra->allocation_size) is junk and contains nothing needing freeing

extern int32_t ra_get_size(roaring_array_t *ra);

#define INITIAL_CAPACITY 4

roaring_array_t *ra_create_with_capacity(int32_t cap) {
    roaring_array_t *new_ra = malloc(sizeof(roaring_array_t));
    if (!new_ra) return NULL;
    new_ra->keys = NULL;
    new_ra->containers = NULL;
    new_ra->typecodes = NULL;

    new_ra->allocation_size = cap;
    new_ra->keys = malloc(cap * sizeof(uint16_t));
    new_ra->containers = malloc(cap * sizeof(void *));
    new_ra->typecodes = malloc(cap * sizeof(uint8_t));
    if (!new_ra->keys || !new_ra->containers || !new_ra->typecodes) {
        free(new_ra);
        free(new_ra->keys);
        free(new_ra->containers);
        free(new_ra->typecodes);
        return NULL;
    }
    new_ra->size = 0;

    return new_ra;
}

roaring_array_t *ra_create() {
    return ra_create_with_capacity(INITIAL_CAPACITY);
}

roaring_array_t *ra_copy(roaring_array_t *r) {
    roaring_array_t *new_ra = malloc(sizeof(roaring_array_t));
    if (!new_ra) return NULL;
    new_ra->keys = NULL;
    new_ra->containers = NULL;
    new_ra->typecodes = NULL;

    const int32_t allocsize = r->allocation_size;
    new_ra->allocation_size = allocsize;
    new_ra->keys = malloc(allocsize * sizeof(uint16_t));
    new_ra->containers =
        calloc(allocsize, sizeof(void *));  // setting pointers to zero
    new_ra->typecodes = malloc(allocsize * sizeof(uint8_t));
    if (!new_ra->keys || !new_ra->containers || !new_ra->typecodes) {
        free(new_ra);
        free(new_ra->keys);
        free(new_ra->containers);
        free(new_ra->typecodes);
        return NULL;
    }
    int32_t s = r->size;
    new_ra->size = s;
    memcpy(new_ra->keys, r->keys, s * sizeof(uint16_t));
    // next line would be a shallow copy, but we need better...
    // memcpy(new_ra->containers,r->containers,s * sizeof(void *));
    memcpy(new_ra->typecodes, r->typecodes, s * sizeof(uint8_t));
    for (int32_t i = 0; i < s; i++) {
        new_ra->containers[i] =
            container_clone(r->containers[i], r->typecodes[i]);
        if (new_ra->containers[i] == NULL) {
            for (int32_t j = 0; j < i; j++) {
                container_free(r->containers[j], r->typecodes[j]);
            }
            free(new_ra);
            free(new_ra->keys);
            free(new_ra->containers);
            free(new_ra->typecodes);
            return NULL;
        }
    }
    return new_ra;
}

static void ra_clear(roaring_array_t *ra) {
    free(ra->keys);
    ra->keys = NULL;  // paranoid
    for (int i = 0; i < ra->size; ++i)
        container_free(ra->containers[i], ra->typecodes[i]);

    free(ra->containers);
    ra->containers = NULL;  // paranoid
    free(ra->typecodes);
    ra->typecodes = NULL;  // paranoid
}

void ra_free(roaring_array_t *ra) {
    ra_clear(ra);
    free(ra);
}

static void extend_array(roaring_array_t *ra, uint16_t k) {
    // corresponding Java code uses >= ??
    int desired_size = ra->size + (int)k;
    if (desired_size > ra->allocation_size) {
        int new_capacity =
            (ra->size < 1024) ? 2 * desired_size : 5 * desired_size / 4;

        ra->keys = realloc(ra->keys, sizeof(uint16_t) * new_capacity);
        ra->containers = realloc(ra->containers, sizeof(void *) * new_capacity);
        ra->typecodes = realloc(ra->typecodes, sizeof(uint8_t) * new_capacity);

        if (!ra->keys || !ra->containers || !ra->typecodes) {
            fprintf(stderr, "[%s] %s\n", __FILE__, __func__);
            perror(0);
            exit(1);
        }

#if 0
    // should not be needed
    // mark the garbage entries
    for (int i = ra->allocation_size; i < new_capacity; ++i) {
      ra->typecodes[i] = UNINITIALIZED_TYPE_CODE;
      // should not be necessary
      ra->containers[i] = ra->keys[i] = 0;
    }
#endif
        ra->allocation_size = new_capacity;
    }
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

void ra_append_copy(roaring_array_t *ra, roaring_array_t *sa, uint16_t index) {
    extend_array(ra, 1);
    const int32_t pos = ra->size;

    // old contents is junk not needing freeing
    ra->keys[pos] = sa->keys[index];
    ra->containers[pos] =
        container_clone(sa->containers[index], sa->typecodes[index]);
    ra->typecodes[pos] = sa->typecodes[index];
    ra->size++;
}

void ra_append_copy_range(roaring_array_t *ra, roaring_array_t *sa,
                          uint16_t start_index, uint16_t end_index) {
    extend_array(ra, end_index - start_index);

    for (uint16_t i = start_index; i < end_index; ++i) {
        const int32_t pos = ra->size;

        ra->keys[pos] = sa->keys[i];
        ra->containers[pos] =
            container_clone(sa->containers[i], sa->typecodes[i]);
        ra->typecodes[pos] = sa->typecodes[i];
        ra->size++;
    }
}

#if 0
// if actually used, should be documented and part of header file
void ra_append_copies_after(roaring_array_t *ra, roaring_array_t *sa,
                       uint16_t before_start) {
  int start_location = ra_get_index(sa, before_start);
  if (start_location >= 0)
    ++start_location;
  else
    start_location = -start_location -1;

  extend_array(ra, sa->size - start_location);

  for (uint16_t i = start_location; i < sa->size; ++i) {
    const int32_t pos = ra->size;

    ra->keys[pos] = sa->keys[i];
    ra->containers[pos] = container_clone(sa->containers[i], sa->typecodes[i]);
    ra->typecodes[pos] = sa->typecodes[i];
    ra->size++;
  }
}
#endif

#if 0
// a form of deep equality. Keys must match and containers must test as equal in
// contents (check Java impl semantics regarding different representations of the same
// container contents, eg, run container vs one of the others)
bool equals( roaring_array_t *ra1, roaring_array_t *ra2) {
  if (ra1->size != ra2->size)
    return false;
  for (int i=0; i < ra1->size; ++i)
    if (ra1->keys[i] != ra2->keys[i] ||
        ! container_equals(ra1->containers[i], ra1->typecodes[i],
                           ra2->containers[i], ra2->typecodes[i]))
      return false;
  return true;
}
#endif

void *ra_get_container(roaring_array_t *ra, uint16_t x, uint8_t *typecode) {
    int i = binarySearch(ra->keys, (int32_t)ra->size, x);
    if (i < 0) return NULL;
    *typecode = ra->typecodes[i];
    return ra->containers[i];
}

void *ra_get_container_at_index(roaring_array_t *ra, uint16_t i,
                                uint8_t *typecode) {
    assert(i < ra->size);
    *typecode = ra->typecodes[i];
    return ra->containers[i];
}

uint16_t ra_get_key_at_index(roaring_array_t *ra, uint16_t i) {
    return ra->keys[i];
}

int32_t ra_get_index(roaring_array_t *ra, uint16_t x) {
    // TODO: next line is possibly unsafe
    if ((ra->size == 0) || ra->keys[ra->size - 1] == x) return ra->size - 1;

    return binarySearch(ra->keys, (int32_t)ra->size, x);
}

extern int32_t ra_advance_until(roaring_array_t *ra, uint16_t x, int32_t pos);

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

// printf("ra downsize from size %d to %d\n", (int)ra->size, (int)new_length);

// all these excess containers  are either in use elsewhere or
// have already been freed by inplace and.

/* bad idea...
for (int i = new_length; i < ra->size; ++i) {
        container_free(ra->containers[i], ra->typecodes[i]);
*/

// by convention, these things will be above ra->size
// and hence garbage not requiring freeing.
#if 0
    ra->containers[i] = NULL; // unnecessary, avoids dangling pointer
    ra->typecodes[i] = UNINITIALIZED_TYPE_CODE;
#endif
    //}
    ra->size = new_length;
}

void ra_remove_at_index(roaring_array_t *ra, int32_t i) {
    container_free(ra->containers[i], ra->typecodes[i]);
    memmove(&(ra->containers[i]), &(ra->containers[i + 1]),
            sizeof(void *) * (ra->size - i - 1));
    memmove(&(ra->keys[i]), &(ra->keys[i + 1]),
            sizeof(uint16_t) * (ra->size - i - 1));
    memmove(&(ra->typecodes[i]), &(ra->typecodes[i + 1]),
            sizeof(uint8_t) * (ra->size - i - 1));
#if 0
  // ought to be unnecessary
  ra->keys[ra->size-1] = ra->containers[ra->size-1] = 0;
  ra->typecodes[ra->size-1] = UNINITIALIZED_TYPE_CODE;
#endif
    ra->size--;
}

void ra_remove_index_range(roaring_array_t *ra, int32_t begin, int32_t end) {
    if (end <= begin) return;

    const int range = end - begin;
    for (int i = begin; i < end; ++i) {
        container_free(ra->containers[i], ra->typecodes[i]);
        memmove(&(ra->containers[begin]), &(ra->containers[end]),
                sizeof(void *) * (ra->size - end));
        memmove(&(ra->keys[begin]), &(ra->keys[end]),
                sizeof(uint16_t) * (ra->size - end));
        memmove(&(ra->typecodes[begin]), &(ra->typecodes[end]),
                sizeof(uint8_t) * (ra->size - end));
    }
#if 0
  // should be unnecessary
  for (int i = 1; i <= range; ++i)
  ra->keys[ra->size-i] = ra->containers[ra->size-i]
    = ra->typecodes[ra->size-i] = 0;
#endif
    ra->size -= range;
}

// used in inplace andNot only, to slide left the containers from
// the mutated RoaringBitmap that are after the largest container of
// the argument RoaringBitmap.  It is followed by a call to resize.
//
void ra_copy_range(roaring_array_t *ra, uint32_t begin, uint32_t end,
                   uint32_t new_begin) {
    static bool warned_em = false;
    if (!warned_em) {
        fprintf(stderr, "[Warning] potential memory leak in ra_copy_range");
        warned_em = true;
    }
    assert(begin <= end);
    assert(new_begin < begin);

    const int range = end - begin;

    // TODO: there is a memory leak here, for any overwritten containers
    // that are not copied elsewhere

    memmove(&(ra->containers[new_begin]), &(ra->containers[begin]),
            sizeof(void *) * range);
    memmove(&(ra->keys[new_begin]), &(ra->keys[begin]),
            sizeof(uint16_t) * range);
    memmove(&(ra->typecodes[new_begin]), &(ra->typecodes[begin]),
            sizeof(uint8_t) * range);
}

void ra_set_container_at_index(roaring_array_t *ra, int32_t i, void *c,
                               uint8_t typecode) {
    assert(i < ra->size);
    // valid container there already
    // container_free(ra->containers[i], ra->typecodes[i]);// too eager!
    // is there a possible memory leak here?

    ra->containers[i] = c;
    ra->typecodes[i] = typecode;
}

void ra_replace_key_and_container_at_index(roaring_array_t *ra, int32_t i,
                                           uint16_t key, void *c,
                                           uint8_t typecode) {
    assert(i < ra->size);
    // container_free(ra->containers[i], ra->typecodes[i]);//too eager!
    // is there a possible memory leak here, then?

    // anyone calling this is responsible for making sure we have freed the
    // container currently at the index,
    // (unless it is the same one we are writing in)

    // possibly we just need to avoid free if c == ra->containers[i] but
    // otherwise do it.

    ra->keys[i] = key;
    ra->containers[i] = c;
    ra->typecodes[i] = typecode;
}

// just for debugging use
void show_structure(roaring_array_t *ra) {
    for (int i = 0; i < ra->size; ++i) {
        printf(" i=%d\n", i);
        fflush(stdout);

        printf("Container %d has key %d and its type is %s  of card %d\n", i,
               (int)ra->keys[i], get_container_name(ra->typecodes[i]),
               container_get_cardinality(ra->containers[i], ra->typecodes[i]));
    }
}

char *ra_serialize(roaring_array_t *ra, uint32_t *serialize_len,
                   uint8_t *retry_with_array) {
    uint32_t off, l,
        cardinality = 0,
        tot_len =
            4 /* tot_len */ + sizeof(roaring_array_t) +
            ra->size * (sizeof(uint16_t) + sizeof(void *) + sizeof(uint8_t));
    char *out;
    uint16_t *lens;

    (*retry_with_array) = 0;
    /* [ 32 bit length ] [ serialization bytes ] */
    if ((lens = (uint16_t *)malloc(sizeof(int16_t) * ra->size)) == NULL) {
        *serialize_len = 0;
        return (NULL);
    }

    for (int32_t i = 0; i < ra->size; i++) {
        lens[i] =
            container_serialization_len(ra->containers[i], ra->typecodes[i]);

        assert(lens[i] != 0);
        tot_len += (lens[i] + sizeof(lens[i]));
        cardinality +=
            container_get_cardinality(ra->containers[i], ra->typecodes[i]);
    }

    if ((cardinality * sizeof(uint32_t)) < tot_len) {
        *retry_with_array = 1;
        free(lens);
        return (NULL);
    }

    out = (char *)malloc(tot_len);

    if (out == NULL) {
        free(lens);
        *serialize_len = 0;
        return (NULL);
    } else
        *serialize_len = tot_len;

    /* Total lenght (first 4 bytes of the serialization) */
    memcpy(out, &tot_len, 4), off = 4;

    l = sizeof(roaring_array_t);
    uint32_t saved_allocation_size = ra->allocation_size;

    ra->allocation_size = ra->size;
    memcpy(&out[off], ra, l);
    ra->allocation_size = saved_allocation_size;
    off += l;

    l = ra->size * sizeof(uint16_t);
    memcpy(&out[off], ra->keys, l);
    off += l;

    l = ra->size * sizeof(void *);
    memcpy(&out[off], ra->containers, l);
    off += l;

    l = ra->size * sizeof(uint8_t);
    memcpy(&out[off], ra->typecodes, l);
    off += l;

    for (int32_t i = 0; i < ra->size; i++) {
        int32_t serialized_bytes;

        memcpy(&out[off], &lens[i], sizeof(lens[i]));
        off += sizeof(lens[i]);
        serialized_bytes =
            container_serialize(ra->containers[i], ra->typecodes[i], &out[off]);

        if (serialized_bytes != lens[i]) {
            for (int32_t j = 0; j <= i; j++)
                container_free(ra->containers[j], ra->typecodes[j]);

            free(lens);
            free(out);
            /* printf("ERROR: serialized_bytes=%d, each=%d\n", serialized_bytes,
             * lens[i]); */
            assert(serialized_bytes != lens[i]);
            return (NULL);
        }

        off += serialized_bytes;
    }

    if (tot_len != off) {
        /* printf("ERROR: tot_len=%d, off=%d\n", tot_len, off); */
        assert(tot_len != off);
    }

    free(lens);

    return (out);
}

roaring_array_t *ra_deserialize(const void *buf, uint32_t buf_len) {
    int32_t size;
    const char *bufaschar = (const char *)buf;
    memcpy(&size, bufaschar, sizeof(int32_t));
    roaring_array_t *ra_copy;
    uint32_t off, l;
    uint32_t expected_len =
        sizeof(roaring_array_t) +
        size * (sizeof(uint16_t) + sizeof(void *) + sizeof(uint8_t));

    if (buf_len < expected_len) return (NULL);

    if ((ra_copy = (roaring_array_t *)malloc(sizeof(roaring_array_t))) == NULL)
        return (NULL);

    memcpy(ra_copy, bufaschar, off = sizeof(roaring_array_t));

    if ((ra_copy->keys = malloc(size * sizeof(uint16_t))) == NULL) {
        free(ra_copy);
        return (NULL);
    }

    if ((ra_copy->containers = malloc(size * sizeof(void *))) == NULL) {
        free(ra_copy->keys);
        free(ra_copy);
        return (NULL);
    }

    if ((ra_copy->typecodes = malloc(size * sizeof(uint8_t))) == NULL) {
        free(ra_copy->containers);
        free(ra_copy->keys);
        free(ra_copy);
        return (NULL);
    }

    l = size * sizeof(uint16_t);
    memcpy(ra_copy->keys, &bufaschar[off], l);
    off += l;

    l = size * sizeof(void *);
    memcpy(ra_copy->containers, &bufaschar[off], l);
    off += l;

    l = size * sizeof(uint8_t);
    memcpy(ra_copy->typecodes, &bufaschar[off], l);
    off += l;

    for (int32_t i = 0; i < size; i++) {
        uint16_t len;

        memcpy(&len, &bufaschar[off], sizeof(len));
        off += sizeof(len);

        ra_copy->containers[i] =
            container_deserialize(ra_copy->typecodes[i], &bufaschar[off], len);

        if (ra_copy->containers[i] == NULL) {
            for (int32_t j = 0; j < i; j++)
                container_free(ra_copy->containers[j], ra_copy->typecodes[j]);

            free(ra_copy->containers);
            free(ra_copy->keys);
            free(ra_copy);
            return (NULL);
        }

        off += len;
    }

    return (ra_copy);
}

bool ra_has_run_container(roaring_array_t *ra) {
    for (int32_t k = 0; k < ra->size; ++k) {
        if (ra->typecodes[k] == RUN_CONTAINER_TYPE_CODE) return true;
    }
    return false;
}

uint32_t ra_portable_header_size(roaring_array_t *ra) {
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

size_t ra_portable_size_in_bytes(roaring_array_t *ra) {
    size_t count = ra_portable_header_size(ra);

    for (int32_t k = 0; k < ra->size; ++k) {
        count += container_size_in_bytes(ra->containers[k], ra->typecodes[k]);
    }
    return count;
}

size_t ra_portable_serialize(roaring_array_t *ra, char *buf) {
    assert(!IS_BIG_ENDIAN);  // not implemented
    char *initbuf = buf;
    uint32_t startOffset = 0;
    bool hasrun = ra_has_run_container(ra);
    if (hasrun) {
        uint32_t cookie = SERIAL_COOKIE | ((ra->size - 1) << 16);
        memcpy(buf, &cookie, sizeof(cookie));
        buf += sizeof(cookie);
        uint32_t s = (ra->size + 7) / 8;
        uint8_t *bitmapOfRunContainers = calloc(s, 1);
        assert(bitmapOfRunContainers != NULL);  // todo: handle
        for (int32_t i = 0; i < ra->size; ++i) {
            if (ra->typecodes[i] == RUN_CONTAINER_TYPE_CODE) {
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

        uint16_t card =
            container_get_cardinality(ra->containers[k], ra->typecodes[k]) - 1;
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

roaring_array_t *ra_portable_deserialize(const char *buf) {
    assert(!IS_BIG_ENDIAN);  // not implemented
    uint32_t cookie;
    memcpy(&cookie, buf, sizeof(int32_t));
    buf += sizeof(uint32_t);
    if ((cookie & 0xFFFF) != SERIAL_COOKIE &&
        cookie != SERIAL_COOKIE_NO_RUNCONTAINER) {
        fprintf(stderr, "I failed to find one of the right cookies. Found %d\n",
                cookie);
        return NULL;
    }
    int32_t size;

    if ((cookie & 0xFFFF) == SERIAL_COOKIE)
        size = (cookie >> 16) + 1;
    else {
        memcpy(&size, buf, sizeof(int32_t));
        buf += sizeof(uint32_t);
    }
    roaring_array_t *answer = ra_create_with_capacity(size);
    if (answer == NULL) {
        fprintf(stderr, "Failed to allocate memory early on. Bailing out.\n");
        return answer;
    }
    answer->size = size;
    char *bitmapOfRunContainers = NULL;
    bool hasrun = (cookie & 0xFFFF) == SERIAL_COOKIE;
    if (hasrun) {
        int32_t s = (size + 7) / 8;
        bitmapOfRunContainers = malloc((size + 7) / 8);
        assert(bitmapOfRunContainers != NULL);  // todo: handle
        memcpy(bitmapOfRunContainers, buf, s);
        buf += s;
    }
    uint16_t *keys = answer->keys;
    int32_t *cardinalities = malloc(size * sizeof(int32_t));
    assert(cardinalities != NULL);  // todo: handle
    bool *isBitmap = malloc(size * sizeof(bool));
    assert(isBitmap != NULL);  // todo: handle
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
    free(cardinalities);
    free(isBitmap);
    return answer;
}
