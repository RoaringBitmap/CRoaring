#include <assert.h>
#include <roaring/roaring.h>
#include <roaring/ordered_writer.h>

roaring_bitmap_writer_t *roaring_bitmap_writer_create(roaring_bitmap_t *target) {
    roaring_bitmap_writer_t *result = (roaring_bitmap_writer_t *) malloc(sizeof(roaring_bitmap_writer_t));
    result->bitmap = (uint64_t *) malloc(sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
    memset(result->bitmap, 0, sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
    result->target = target;
    result->current_key = 0;
    result->dirty = false;
    return result;
}

void roaring_bitmap_writer_free(roaring_bitmap_writer_t *writer) {
    free(writer->bitmap);
    free(writer);
}

void roaring_bitmap_writer_flush(roaring_bitmap_writer_t *writer) {
    if (!(writer->dirty)) {
        return;
    }

    uint64_t *bitmap = writer->bitmap;
    roaring_bitmap_t *target = writer->target;

    bitset_container_t *newac = bitset_container_create();

    newac->cardinality = -1;
    memcpy(newac->array, bitmap, sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);

    uint8_t new_typecode = BITSET_CONTAINER_TYPE_CODE;
    void *newcontainer = container_repair_after_lazy(newac, &new_typecode);

    ra_append(&target->high_low_container, writer->current_key, newcontainer, new_typecode);

    memset(bitmap, 0, sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
}

void roaring_bitmap_writer_add(roaring_bitmap_writer_t *writer, const uint32_t val) {
    const uint16_t hb = val >> 16;
    const uint16_t lb = val & 0xFFFF;

    if (hb != writer->current_key) {
        assert(hb > writer->current_key);
        roaring_bitmap_writer_flush(writer);
    }

    uint64_t *bitmap = writer->bitmap;

    const int index = lb & 63;
    bitmap[lb >> 6] |= (UINT64_C(1) << (lb & 63));

    writer->current_key = hb;
    writer->dirty = true;
}
