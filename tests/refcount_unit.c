#include <stdint.h>

#include <roaring/portability.h>

#include "test.h"

DEFINE_TEST(test_non_atomic_refcount) {
    croaring_refcount_t refcount = 2;

    assert_false(croaring_refcount_dec(&refcount));
    assert_int_equal(croaring_refcount_get(&refcount), 1);
    assert_true(croaring_refcount_dec(&refcount));
    assert_int_equal(croaring_refcount_get(&refcount), 0);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_non_atomic_refcount),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
