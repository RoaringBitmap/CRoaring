#include <roaring/roaring.h>

#include "test.h"

int main(void) {
    // Volatile function pointers require external C definitions even when
    // optimization could otherwise inline these calls from the headers.
    void (*volatile add_range)(roaring_bitmap_t *, uint64_t, uint64_t) =
        roaring_bitmap_add_range;
    bool (*volatile contains)(const roaring_bitmap_t *, uint32_t) =
        roaring_bitmap_contains;
    void (*volatile set_bit)(bitset_t *, size_t) = bitset_set;
    bool (*volatile get_bit)(const bitset_t *, size_t) = bitset_get;
    bool (*volatile next)(const container_t *, uint8_t,
                          roaring_container_iterator_t *, uint16_t *) =
        container_iterator_next;
    bool (*volatile prev)(const container_t *, uint8_t,
                          roaring_container_iterator_t *, uint16_t *) =
        container_iterator_prev;

    roaring_bitmap_t *bitmap = roaring_bitmap_create();
    assert_non_null(bitmap);
    add_range(bitmap, 10, 20);
    assert_int_equal(roaring_bitmap_get_cardinality(bitmap), 10);
    assert_true(contains(bitmap, 10));
    assert_false(contains(bitmap, 20));
    roaring_bitmap_free(bitmap);

    bitset_t *bits = bitset_create();
    assert_non_null(bits);
    set_bit(bits, 70);
    assert_true(get_bit(bits, 70));
    assert_false(get_bit(bits, 69));
    bitset_free(bits);

    array_container_t *array = array_container_create_range(10, 13);
    assert_non_null(array);
    const container_t *container = (const container_t *)array;
    uint16_t value = 0;
    roaring_container_iterator_t it =
        container_init_iterator(container, ARRAY_CONTAINER_TYPE, &value);
    assert_int_equal(value, 10);
    assert_true(next(container, ARRAY_CONTAINER_TYPE, &it, &value));
    assert_int_equal(value, 11);
    assert_true(prev(container, ARRAY_CONTAINER_TYPE, &it, &value));
    assert_int_equal(value, 10);
    assert_false(prev(container, ARRAY_CONTAINER_TYPE, &it, &value));
    array_container_free(array);
    return 0;
}
