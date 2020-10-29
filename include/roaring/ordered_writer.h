#ifndef ROARING_ORDERED_WRITER
#define ROARING_ORDERED_WRITER

#include <assert.h>
#include <roaring/roaring.h>

typedef struct roaring_bitmap_writer_s {
    uint64_t *bitmap;
    roaring_bitmap_t *target;
    uint32_t current_key;
    bool dirty;
} roaring_bitmap_writer_t;

roaring_bitmap_writer_t *roaring_bitmap_writer_create(roaring_bitmap_t *target);

void roaring_bitmap_writer_set(roaring_bitmap_writer_t *result, roaring_bitmap_t *target);

void roaring_bitmap_writer_free(roaring_bitmap_writer_t *writer);

void roaring_bitmap_writer_flush(roaring_bitmap_writer_t *writer);

bool roaring_bitmap_writer_add(roaring_bitmap_writer_t *writer, const uint32_t val);

#endif