/*
 * mixed_container_unit.c
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <roaring/containers/containers.h>
#include <roaring/containers/mixed_andnot.h>
#include <roaring/containers/mixed_intersection.h>
#include <roaring/containers/mixed_negation.h>
#include <roaring/containers/mixed_union.h>
#include <roaring/containers/mixed_xor.h>
#include <roaring/misc/configreport.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
    using namespace roaring::internal;
#endif

#include "test.h"


//#define UNVERBOSE_MIXED_CONTAINER

DEFINE_TEST(array_bitset_and_or_xor_andnot_test) {
    array_container_t* A1 = array_container_create();
    array_container_t* A2 = array_container_create();
    array_container_t* AI = array_container_create();
    array_container_t* AO = array_container_create();
    array_container_t* AX = array_container_create();
    array_container_t* AM = array_container_create();
    array_container_t* AM1 = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BI = bitset_container_create();
    bitset_container_t* BO = bitset_container_create();
    bitset_container_t* BX = bitset_container_create();
    bitset_container_t* BM = bitset_container_create();
    bitset_container_t* BM1 = bitset_container_create();

    // nb, array containers will be illegally big.
    for (int x = 0; x < (1 << 16); x += 3) {
        array_container_add(A1, x);
        array_container_add(AO, x);
        bitset_container_set(B1, x);
        bitset_container_set(BO, x);
    }

    // important: 62 is not divisible by 3
    for (int x = 0; x < (1 << 16); x += 62) {
        array_container_add(A2, x);
        array_container_add(AO, x);
        bitset_container_set(B2, x);
        bitset_container_set(BO, x);
    }

    for (int x = 0; x < (1 << 16); x += 62 * 3) {
        array_container_add(AI, x);
        bitset_container_set(BI, x);
    }

    for (int x = 0; x < (1 << 16); x++) {
        if ((x % 62 == 0) ^ (x % 3 == 0)) {
            array_container_add(AX, x);
            bitset_container_set(BX, x);
        }
        if ((x % 3 == 0) && !(x % 62 == 0)) {
            array_container_add(AM, x);
            bitset_container_set(BM, x);
        }
        if ((x % 62 == 0) && !(x % 3 == 0)) {
            array_container_add(AM1, x);
            bitset_container_set(BM1, x);
        }
    }
    // we interleave O and I on purpose (to trigger bugs!)
    int ci = array_container_cardinality(AI);  // expected intersection
    int co = array_container_cardinality(AO);  // expected union
    int cx = array_container_cardinality(AX);  // expected xor
    int cm = array_container_cardinality(AM);  // expected minus (andNot)
    int cm1 =
        array_container_cardinality(AM1);  // expected minus (andNot) reversed

    assert_int_equal(ci, bitset_container_cardinality(BI));
    assert_int_equal(co, bitset_container_cardinality(BO));

    array_container_intersection(A1, A2, AI);
    array_container_union(A1, A2, AO);
    array_container_xor(A1, A2, AX);
    array_container_andnot(A1, A2, AM);
    array_container_andnot(A2, A1, AM1);
    bitset_container_intersection(B1, B2, BI);
    bitset_container_union(B1, B2, BO);
    bitset_container_xor(B1, B2, BX);
    bitset_container_andnot(B1, B2, BM);
    bitset_container_andnot(B2, B1, BM1);

    assert_int_equal(ci, bitset_container_cardinality(BI));
    assert_int_equal(co, bitset_container_cardinality(BO));
    assert_int_equal(cx, bitset_container_cardinality(BX));
    assert_int_equal(cm, bitset_container_cardinality(BM));
    assert_int_equal(cm1, bitset_container_cardinality(BM1));
    assert_int_equal(ci, array_container_cardinality(AI));
    assert_int_equal(co, array_container_cardinality(AO));
    assert_int_equal(cx, array_container_cardinality(AX));
    assert_int_equal(cm, array_container_cardinality(AM));
    assert_int_equal(cm1, array_container_cardinality(AM1));

    array_bitset_container_intersection(A1, B2, AI);
    assert_int_equal(ci, array_container_cardinality(AI));

    array_bitset_container_intersection(A2, B1, AI);
    assert_int_equal(ci, array_container_cardinality(AI));

    array_bitset_container_union(A1, B2, BO);
    assert_int_equal(co, bitset_container_cardinality(BO));

    array_bitset_container_union(A2, B1, BO);
    assert_int_equal(co, bitset_container_cardinality(BO));

    container_t* C = NULL;

    assert_true(array_bitset_container_xor(A1, B2, &C));
    assert_int_equal(cx, bitset_container_cardinality(CAST_bitset(C)));

    bitset_container_free(CAST_bitset(C));
    C = NULL;
    assert_true(array_bitset_container_xor(A2, B1, &C));
    assert_int_equal(cx, bitset_container_cardinality(CAST_bitset(C)));

    bitset_container_free(CAST_bitset(C));
    C = NULL;
    assert_true(array_array_container_xor(A2, A1, &C));
    assert_int_equal(cx, bitset_container_cardinality(CAST_bitset(C)));

    bitset_container_free(CAST_bitset(C));
    C = NULL;
    assert_true(bitset_bitset_container_xor(B2, B1, &C));
    assert_int_equal(cx, bitset_container_cardinality(CAST_bitset(C)));

    bitset_container_free(CAST_bitset(C));
    C = NULL;
    // xoring something with itself, getting array
    assert_false(array_bitset_container_xor(A2, B2, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));

    array_container_free(CAST_array(C));
    C = NULL;
    // xoring array with itself, getting array
    assert_false(array_array_container_xor(A2, A2, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));

    array_container_free(CAST_array(C));
    C = NULL;
    // xoring bitset with itself, getting array
    assert_false(bitset_bitset_container_xor(B2, B2, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));

    array_container_free(CAST_array(C));
    C = NULL;

    array_bitset_container_andnot(A1, B2, AM);
    assert_int_equal(cm, array_container_cardinality(AM));

    array_bitset_container_andnot(A2, B1, AM1);
    assert_int_equal(cm1, array_container_cardinality(AM1));

    array_array_container_andnot(A2, A1, AM1);
    assert_int_equal(cm1, array_container_cardinality(AM1));

    array_array_container_andnot(A1, A2, AM);
    assert_int_equal(cm, array_container_cardinality(AM));

    // C will be sometimes bitmap, sometimes array

    assert_true(bitset_bitset_container_andnot(B1, B2, &C));
    assert_int_equal(cm, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    assert_true(bitset_array_container_andnot(B1, A2, &C));
    assert_int_equal(cm, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    // Hopefully density means it will be an array
    assert_false(bitset_bitset_container_andnot(B2, B1, &C));
    assert_int_equal(cm1, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    // Hopefully density means it will be an array
    assert_false(bitset_array_container_andnot(B2, A1, &C));
    assert_int_equal(cm1, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    // subtracting something with itself, getting array
    array_bitset_container_andnot(A2, B2, AM1);
    assert_int_equal(0, array_container_cardinality(AM1));

    // subtracting something with itself, getting array
    bitset_array_container_andnot(B2, A2, &C);
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    // subtracting array with itself, getting array
    array_array_container_andnot(A2, A2, AM1);
    assert_int_equal(0, array_container_cardinality(AM1));

    // subtracting bitset with itself, getting array
    assert_false(bitset_bitset_container_andnot(B2, B2, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));

    array_container_free(A1);
    array_container_free(A2);
    array_container_free(AI);
    array_container_free(AO);
    array_container_free(AX);
    array_container_free(AM);
    array_container_free(AM1);

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(BI);
    bitset_container_free(BO);
    bitset_container_free(BX);
    bitset_container_free(BM);
    bitset_container_free(BM1);
    // bitset_container_free(CAST_bitset(C));
}

// all xor routines with lazy option
DEFINE_TEST(array_bitset_run_lazy_xor_test) {
    // not all these containers are currently used in tests
    array_container_t* A1 = array_container_create();
    array_container_t* A2 = array_container_create();
    array_container_t* AX = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* B2copy = bitset_container_create();
    bitset_container_t* BX = bitset_container_create();
    run_container_t* R1 = run_container_create();
    run_container_t* R2 = run_container_create();
    run_container_t* RX = run_container_create();

    // nb, array and run containers will be illegally big.
    for (int x = 0; x < (1 << 16); x += 3) {
        array_container_add(A1, x);
        bitset_container_set(B1, x);
        run_container_add(R1, x);
    }

    // important: 62 is not divisible by 3
    for (int x = 0; x < (1 << 16); x += 62) {
        array_container_add(A2, x);
        bitset_container_set(B2, x);
        bitset_container_set(B2copy, x);
        run_container_add(R2, x);
    }

    for (int x = 0; x < (1 << 16); x++)
        if ((x % 62 == 0) ^ (x % 3 == 0)) {
            array_container_add(AX, x);
            bitset_container_set(BX, x);
            run_container_add(RX, x);
        }

    // we interleave O and I on purpose (to trigger bugs!)
    int cx = array_container_cardinality(AX);  // expected xor

    array_bitset_container_lazy_xor(A1, B2, BX);
    assert_int_equal(BITSET_UNKNOWN_CARDINALITY,
                     bitset_container_cardinality(BX));
    assert_int_equal(cx, bitset_container_compute_cardinality(BX));

    array_bitset_container_lazy_xor(A1, B2, B2);  // result onto B2, allowed
    assert_int_equal(BITSET_UNKNOWN_CARDINALITY,
                     bitset_container_cardinality(B2));
    assert_int_equal(cx, bitset_container_compute_cardinality(B2));
    bitset_container_copy(B2copy, B2);

    run_bitset_container_lazy_xor(R1, B2, BX);
    assert_int_equal(BITSET_UNKNOWN_CARDINALITY,
                     bitset_container_cardinality(BX));
    assert_int_equal(cx, bitset_container_compute_cardinality(BX));

    run_bitset_container_lazy_xor(
        R1, B2, B2);  // result onto B2 : not sure it's allowed
    assert_int_equal(BITSET_UNKNOWN_CARDINALITY,
                     bitset_container_cardinality(B2));
    assert_int_equal(cx, bitset_container_compute_cardinality(B2));
    bitset_container_copy(B2copy, B2);

    container_t *ans = 0;
    assert_true(array_array_container_lazy_xor(A1, A2, &ans));
    assert_int_equal(BITSET_UNKNOWN_CARDINALITY,
                     bitset_container_cardinality(CAST_bitset(ans)));
    assert_int_equal(cx, bitset_container_compute_cardinality(CAST_bitset(ans)));
    bitset_container_free(CAST_bitset(ans));

    array_run_container_lazy_xor(A1, R2, RX);  // destroys content of RX
    assert_int_equal(cx, run_container_cardinality(RX));

    array_container_free(A1);
    array_container_free(A2);
    array_container_free(AX);

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(B2copy);
    bitset_container_free(BX);

    run_container_free(R1);
    run_container_free(R2);
    run_container_free(RX);
}

DEFINE_TEST(array_bitset_ixor_test) {
    array_container_t* A1 = array_container_create();
    array_container_t* A1copy = array_container_create();
    array_container_t* A1mod = array_container_create();
    array_container_t* A2 = array_container_create();
    array_container_t* AX = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B1copy = bitset_container_create();
    bitset_container_t* B1mod = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BX = bitset_container_create();

    // nb, array containers will be illegally big.
    for (int x = 0; x < (1 << 16); x += 3) {
        array_container_add(A1, x);
        bitset_container_set(B1, x);
    }

    // important: 62 is not divisible by 3
    for (int x = 0; x < (1 << 16); x += 62) {
        array_container_add(A2, x);
        bitset_container_set(B2, x);
    }

    for (int x = 0; x < (1 << 16); x++)
        if ((x % 62 == 0) ^ (x % 3 == 0)) {
            array_container_add(AX, x);
            bitset_container_set(BX, x);
        }

    array_container_copy(A1, A1copy);
    bitset_container_copy(B1, B1copy);
    array_container_copy(A1, A1mod);
    array_container_add(A1mod, 2);
    bitset_container_copy(B1, B1mod);
    bitset_container_add(B1mod, 2);

    int cx = array_container_cardinality(AX);  // expected xor

    container_t* C = NULL;

    assert_true(bitset_array_container_ixor(B2, A1, &C));
    assert_int_equal(cx, bitset_container_cardinality(CAST_bitset(C)));
    // this case, result is inplace
    assert_ptr_equal(C, B2);

    C = NULL;
    assert_true(array_bitset_container_ixor(A2, B1, &C));
    assert_int_equal(cx, bitset_container_cardinality(CAST_bitset(C)));
    assert_ptr_not_equal(C, A2);  // nb A2 is destroyed
    // don't test a case where result can fit in the array
    // until this is implemented...at that point, make sure

    bitset_container_free(CAST_bitset(C));
    C = NULL;
    // xoring something with itself, getting array
    assert_false(array_bitset_container_ixor(A1, B1, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));

    array_container_free(CAST_array(C));
    C = NULL;

    // B1mod and B1copy differ in position 2 only
    assert_false(bitset_bitset_container_ixor(B1mod, B1copy, &C));
    assert_int_equal(1, array_container_cardinality(CAST_array(C)));

    array_container_free(CAST_array(C));
    C = NULL;
    assert_false(array_array_container_ixor(A1mod, A1copy, &C));
    assert_int_equal(1, array_container_cardinality(CAST_array(C)));

    // array_container_free(A1); // disposed already
    //    array_container_free(A2); // has been disposed already
    array_container_free(AX);
    array_container_free(A1copy);

    bitset_container_free(B1);
    bitset_container_free(B1copy);
    bitset_container_free(B2);
    bitset_container_free(BX);
    array_container_free(CAST_array(C));
}

DEFINE_TEST(array_bitset_iandnot_test) {
    array_container_t* A1 = array_container_create();
    array_container_t* AM = array_container_create();
    array_container_t* AM1 = array_container_create();
    array_container_t* A1copy = array_container_create();
    array_container_t* A2copy = array_container_create();
    array_container_t* A1mod = array_container_create();
    array_container_t* A2 = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* BM = bitset_container_create();
    bitset_container_t* BM1 = bitset_container_create();
    bitset_container_t* B1copy = bitset_container_create();
    bitset_container_t* B1mod = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* B2copy = bitset_container_create();

    // nb, array containers will be illegally big.
    for (int x = 0; x < (1 << 16); x += 3) {
        array_container_add(A1, x);
        bitset_container_set(B1, x);
    }

    // important: 62 is not divisible by 3
    for (int x = 0; x < (1 << 16); x += 62) {
        array_container_add(A2, x);
        bitset_container_set(B2, x);
    }

    for (int x = 0; x < (1 << 16); x++) {
        if ((x % 3 == 0) && !(x % 62 == 0)) {
            array_container_add(AM, x);
            bitset_container_set(BM, x);
        }
        if ((x % 62 == 0) && !(x % 3 == 0)) {
            array_container_add(AM1, x);
            bitset_container_set(BM1, x);
        }
    }

    array_container_copy(A1, A1copy);
    array_container_copy(A2, A2copy);
    bitset_container_copy(B1, B1copy);
    bitset_container_copy(B2, B2copy);
    array_container_copy(A1, A1mod);
    array_container_add(A1mod, 2);
    bitset_container_copy(B1, B1mod);
    bitset_container_add(B1mod, 2);

    int cm = array_container_cardinality(AM);    // expected difference
    int cm1 = array_container_cardinality(AM1);  // expected reverse difference

    container_t *C = NULL;

    assert_false(bitset_array_container_iandnot(B2, A1, &C));
    assert_int_equal(cm1, array_container_cardinality(CAST_array(C)));
    // this case, result is not inplace
    assert_ptr_not_equal(C, B2);
    B2 = bitset_container_create();  // since B2 had been destroyed.
    array_container_free(CAST_array(C));
    bitset_container_copy(B2copy, B2);

    assert_true(bitset_array_container_iandnot(B1, A2, &C));
    assert_int_equal(cm, bitset_container_cardinality(CAST_bitset(C)));
    // this case, result is inplace
    assert_ptr_equal(C, B1);
    bitset_container_copy(B1copy, B1);

    array_bitset_container_iandnot(A2, B1);
    assert_int_equal(cm1, array_container_cardinality(A2));
    array_container_copy(A2copy, A2);

    // subtracting something from itself, getting array
    array_bitset_container_iandnot(A1, B1);
    assert_int_equal(0, array_container_cardinality(A1));
    array_container_copy(A1copy, A1);

    // B1mod and B1copy differ in position 2 only (B1mod has it)
    assert_false(bitset_bitset_container_iandnot(B1mod, B1copy, &C));
    assert_int_equal(1, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    array_array_container_iandnot(A1mod, A1copy);
    assert_int_equal(1, array_container_cardinality(A1mod));
    // A1 mod now corrupted

    array_container_free(A1);
    array_container_free(A2);
    array_container_free(AM);
    array_container_free(AM1);
    array_container_free(A1copy);
    array_container_free(A2copy);
    array_container_free(A1mod);

    bitset_container_free(B1);
    bitset_container_free(B1copy);
    bitset_container_free(B2);
    bitset_container_free(B2copy);
    bitset_container_free(BM);
    bitset_container_free(BM1);
}

// routines where one of the containers is a run container
DEFINE_TEST(run_xor_test) {
    array_container_t* A1 = array_container_create();
    array_container_t* A2 = array_container_create();
    array_container_t* A3 = array_container_create();
    array_container_t* AX = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* B3 = bitset_container_create();
    bitset_container_t* BX = bitset_container_create();
    run_container_t* R1 = run_container_create();
    run_container_t* R2 = run_container_create();
    run_container_t* R3 = run_container_create();
    run_container_t* R4 = run_container_create();

    // B/A1 xor R1 is empty (array or run, I guess)
    // B/A1 xor R2 is probably best left as runs
    // B/A3 xor R1 is best as an array.
    // B/A3 xor R4 is best as a bitmap

    // nb, array containers will be illegally big.
    for (int x = 0; x < (1 << 16); x++) {
        if (x % 5 < 3) {
            array_container_add(A1, x);
            bitset_container_set(B1, x);
            run_container_add(R1, x);
        }
    }

    for (int x = 0; x < (1 << 16); x++) {
        if (x % 62 < 37) {
            array_container_add(A2, x);
            bitset_container_set(B2, x);
            run_container_add(R2, x);
        }
    }

    for (int x = 0; x < (1 << 16); x++)
        if ((x % 62 < 37) ^ (x % 5 < 3)) {
            array_container_add(AX, x);
            bitset_container_set(BX, x);
        }

    // the elements x%5 == 2 differ for less than 10k, otherwise same)
    for (int x = 0; x < (1 << 16); x++) {
        if ((x % 5 < 2) || ((x % 5 < 3) && (x > 10000))) {
            array_container_add(A3, x);
            bitset_container_set(B3, x);
            run_container_add(R3, x);
        }
    }

    int randstate = 1;  // for Oakenfull RNG, hope LSBits are nice
    for (int x = 0; x < (1 << 16); x++) {
        if (randstate % 4) {
            run_container_add(R4, x);
        }
        randstate = (3432 * randstate + 6789) % 9973;
    }

    int cx12 = array_container_cardinality(AX);  // expected xor for ?1 and ?2

    container_t* C = NULL;

    assert_false(run_bitset_container_xor(R1, B1, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     array_run_container_xor(A1, R1, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    // both run coding and array coding have same serialized size for
    // empty
    assert_int_equal(RUN_CONTAINER_TYPE,
                     run_run_container_xor(R1, R1, &C));
    assert_int_equal(0, run_container_cardinality(CAST_run(C)));
    run_container_free(CAST_run(C));
    C = NULL;

    assert_false(run_bitset_container_xor(R1, B3, &C));
    assert_int_equal(2000, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     array_run_container_xor(A3, R1, &C));
    assert_int_equal(2000, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_run_container_xor(R1, R3, &C));
    assert_int_equal(2000, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    assert_true(run_bitset_container_xor(R1, B2, &C));
    assert_int_equal(cx12, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    assert_int_equal(BITSET_CONTAINER_TYPE,
                     array_run_container_xor(A2, R1, &C));
    assert_int_equal(cx12, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    array_container_t* A_small = array_container_create();
    for (int i = 1000; i < 1010; ++i) array_container_add(A_small, i);

    assert_int_equal(RUN_CONTAINER_TYPE,
                     array_run_container_xor(A_small, R2, &C));
    assert_int_equal(0x98bd,
                     run_container_cardinality(CAST_run(C)));
    run_container_free(CAST_run(C));
    C = NULL;

    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_run_container_xor(R1, R2, &C));
    assert_int_equal(cx12, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    assert_true(run_bitset_container_xor(R4, B3, &C));
    int card_3_4 = bitset_container_cardinality(CAST_bitset(C));
    // assert_int_equal(card_3_4, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    assert_int_equal(BITSET_CONTAINER_TYPE,
                     array_run_container_xor(A3, R4, &C));
    // if this fails, either this bitset is wrong or the previous one...
    assert_int_equal(card_3_4, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_run_container_xor(R4, R3, &C));
    assert_int_equal(card_3_4, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    array_container_free(A1);
    array_container_free(A2);
    array_container_free(A3);
    array_container_free(AX);
    array_container_free(A_small);

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(B3);
    bitset_container_free(BX);

    run_container_free(R1);
    run_container_free(R2);
    run_container_free(R3);
    run_container_free(R4);
}

// routines where one of the containers is a run container, copied from xor code
DEFINE_TEST(run_andnot_test) {
    array_container_t* A1 = array_container_create();
    array_container_t* A2 = array_container_create();
    array_container_t* A3 = array_container_create();
    array_container_t* A4 = array_container_create();
    array_container_t* AM = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* B3 = bitset_container_create();
    bitset_container_t* B4 = bitset_container_create();
    bitset_container_t* BM = bitset_container_create();
    run_container_t* R1 = run_container_create();
    run_container_t* R2 = run_container_create();
    run_container_t* R3 = run_container_create();
    run_container_t* R4 = run_container_create();

    // B/A1 minus  R1 is empty (array or run, I guess)
    // B/A1 minus R2 is probably best left as runs
    // B/A3 minus R1 is best as an array.
    // B/A3 minus R4 is best as a bitmap

    // nb, array containers will be illegally big.
    for (int x = 0; x < (1 << 16); x++) {
        if (x % 5 < 3) {
            array_container_add(A1, x);
            bitset_container_set(B1, x);
            run_container_add(R1, x);
        }
    }

    for (int x = 0; x < (1 << 16); x++) {
        if (x % 62 < 37) {
            array_container_add(A2, x);
            bitset_container_set(B2, x);
            run_container_add(R2, x);
        }
    }

    for (int x = 0; x < (1 << 16); x++)
        if ((x % 5 < 3) && !(x % 62 < 37)) {
            array_container_add(AM, x);
            bitset_container_set(BM, x);
        }

    // the elements x%5 == 2 differ for less than 10k, otherwise same)
    for (int x = 0; x < (1 << 16); x++) {
        if ((x % 5 < 2) || ((x % 5 < 3) && (x > 10000))) {
            array_container_add(A3, x);
            bitset_container_set(B3, x);
            run_container_add(R3, x);
        }
    }

    int randstate = 1;  // for Oakenfull RNG, hope LSBits are nice
    for (int x = 0; x < (1 << 16); x++) {
        if (randstate % 4) {
            run_container_add(R4, x);
            array_container_add(A4, x);
            bitset_container_add(B4, x);
        }
        randstate = (3432 * randstate + 6789) % 9973;
    }

    int cm12 = array_container_cardinality(AM);

    container_t* BM_1 = NULL;

    assert_false(run_bitset_container_andnot(R1, B1, &BM_1));
    assert_int_equal(0, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    array_run_container_andnot(A1, R1, AM);
    assert_int_equal(0, array_container_cardinality(AM));

    // both run coding and array coding have same serialized size for
    // empty
    assert_int_equal(RUN_CONTAINER_TYPE,
                     run_run_container_andnot(R1, R1, &BM_1));
    assert_int_equal(0, run_container_cardinality(CAST_run(BM_1)));
    run_container_free(CAST_run(BM_1));
    BM_1 = NULL;

    assert_false(run_bitset_container_andnot(R1, B3, &BM_1));
    assert_int_equal(2000, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    assert_false(bitset_run_container_andnot(B1, R3, &BM_1));
    assert_int_equal(2000, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    array_run_container_andnot(A1, R3, AM);
    assert_int_equal(2000, array_container_cardinality(AM));

    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_array_container_andnot(R1, A3, &BM_1));
    assert_int_equal(2000, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_run_container_andnot(R1, R3, &BM_1));
    assert_int_equal(2000, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    assert_true(run_bitset_container_andnot(R1, B2, &BM_1));
    assert_int_equal(cm12, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    array_run_container_andnot(A1, R2, AM);
    assert_int_equal(cm12, array_container_cardinality(AM));

    array_container_t* A_small = array_container_create();
    for (int i = 990; i < 1000; ++i) array_container_add(A_small, i);

    run_container_t* R_small = run_container_create();
    for (int i = 990; i < 1000; ++i) run_container_add(R_small, i);

    array_run_container_andnot(A_small, R2, AM);
    assert_int_equal(2,                                 // something like that
                     array_container_cardinality(AM));  // hopefully right...

    assert_false(run_bitset_container_andnot(R_small, B2, &BM_1));
    assert_int_equal(2, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    // note, result is equally small as an array or a run
    assert_int_equal(RUN_CONTAINER_TYPE,
                     run_array_container_andnot(R_small, A2, &BM_1));
    assert_int_equal(2, run_container_cardinality(CAST_run(BM_1)));
    run_container_free(CAST_run(BM_1));
    BM_1 = NULL;

    // test with more complicated small run structure (to do)
    run_container_t* R_small_complex = run_container_create();
    array_container_t* temp_ac = array_container_create();

    for (int i = 0; i < 3; ++i) run_container_add(R_small_complex, i);
    for (int i = 10; i < 12; ++i) run_container_add(R_small_complex, i);
    for (int i = 990; i < 995; ++i) run_container_add(R_small_complex, i);
    for (int i = 10000; i < 10003; ++i) run_container_add(R_small_complex, i);
    for (int i = 20000; i < 20002; ++i) run_container_add(R_small_complex, i);

    array_container_add(temp_ac, 993);
    array_container_add(temp_ac, 994);
    array_container_add(temp_ac, 2000);

    assert_int_equal(
        RUN_CONTAINER_TYPE,
        run_array_container_andnot(R_small_complex, temp_ac, &BM_1));
    assert_int_equal(13, run_container_cardinality(CAST_run(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    array_container_free(temp_ac);
    run_container_free(R_small_complex);

    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_array_container_andnot(R1, A3, &BM_1));
    assert_int_equal(2000, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_run_container_andnot(R1, R2, &BM_1));
    assert_int_equal(cm12, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    // compute the true card for cont4 - cont3 assuming that
    // bitset-bitset implementation is known correct
    assert_true(bitset_bitset_container_andnot(B4, B3, &BM_1));
    int card_4_3 = bitset_container_cardinality(CAST_bitset(BM_1));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    assert_true(run_bitset_container_andnot(R4, B3, &BM_1));
    assert_int_equal(card_4_3, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    array_run_container_andnot(A4, R3, AM);
    // if this fails, either this bitset is wrong or the previous one...
    assert_int_equal(card_4_3, array_container_cardinality(AM));

    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_run_container_andnot(R4, R3, &BM_1));
    assert_int_equal(card_4_3, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    array_container_free(A1);
    array_container_free(A2);
    array_container_free(A3);
    array_container_free(A4);
    array_container_free(AM);
    array_container_free(A_small);

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(B3);
    bitset_container_free(B4);
    bitset_container_free(BM);

    run_container_free(R1);
    run_container_free(R2);
    run_container_free(R3);
    run_container_free(R4);
    run_container_free(R_small);
}

// routines where one of the containers is a run container
DEFINE_TEST(run_ixor_test) {
    array_container_t* A1 = array_container_create();
    array_container_t* A2 = array_container_create();
    array_container_t* A3 = array_container_create();
    array_container_t* A4 = array_container_create();
    array_container_t* AX = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* B3 = bitset_container_create();
    bitset_container_t* BX = bitset_container_create();
    run_container_t* R1 = run_container_create();
    run_container_t* R2 = run_container_create();
    run_container_t* R3 = run_container_create();
    run_container_t* R4 = run_container_create();

    // B/A1 xor R1 is empty (array or run, I guess)
    // B/A1 xor R2 is probably best left as runs
    // B/A3 xor R1 is best as an array.
    // B/A3 xor R4 is best as a bitmap

    // nb, array containers will be illegally big.
    for (int x = 0; x < (1 << 16); x++) {
        if (x % 5 < 3) {
            array_container_add(A1, x);
            bitset_container_set(B1, x);
            run_container_add(R1, x);
        }
    }

    for (int x = 0; x < (1 << 16); x++) {
        if (x % 62 < 37) {
            array_container_add(A2, x);
            bitset_container_set(B2, x);
            run_container_add(R2, x);
        }
    }

    for (int x = 0; x < (1 << 16); x++)
        if ((x % 62 < 37) ^ (x % 5 < 3)) {
            array_container_add(AX, x);
            bitset_container_set(BX, x);
        }

    // the elements x%5 == 2 differ for less than 10k, otherwise same)
    for (int x = 0; x < (1 << 16); x++) {
        if ((x % 5 < 2) || ((x % 5 < 3) && (x > 10000))) {
            array_container_add(A3, x);
            bitset_container_set(B3, x);
            run_container_add(R3, x);
        }
    }

    int randstate = 1;  // for Oakenfull RNG, hope LSBits are nice
    for (int x = 0; x < (1 << 16); x++) {
        if (randstate % 4) {
            run_container_add(R4, x);
            array_container_add(A4, x);
        }
        randstate = (3432 * randstate + 6789) % 9973;
    }

    int cx12 = array_container_cardinality(AX);  // expected xor for ?1 and ?2

    container_t* C = NULL;

    run_container_t* temp_r = run_container_clone(R1);
    assert_false(run_bitset_container_ixor(temp_r, B1, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    bitset_container_t* temp_b = bitset_container_create();
    bitset_container_copy(B1, temp_b);
    assert_false(bitset_run_container_ixor(temp_b, R1, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    array_container_t* temp_a = array_container_clone(A1);
    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     array_run_container_ixor(temp_a, R1, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    temp_r = run_container_clone(R1);
    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_array_container_ixor(temp_r, A1, &C));
    assert_int_equal(0, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    // both run coding and array coding have same serialized size for
    // empty
    temp_r = run_container_clone(R1);
    int ret_type = run_run_container_ixor(temp_r, R1, &C);
    assert_int_not_equal(BITSET_CONTAINER_TYPE, ret_type);
    if (ret_type == RUN_CONTAINER_TYPE) {
        assert_int_equal(0, run_container_cardinality(CAST_run(C)));
        run_container_free(CAST_run(C));
    } else {
        assert_int_equal(0, array_container_cardinality(CAST_array(C)));
        array_container_free(CAST_array(C));
    }
    C = NULL;

    temp_r = run_container_clone(R1);
    assert_false(run_bitset_container_ixor(temp_r, B3, &C));
    assert_int_equal(2000, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    temp_a = array_container_clone(A3);
    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     array_run_container_ixor(temp_a, R1, &C));
    assert_int_equal(2000, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    temp_b = bitset_container_create();
    bitset_container_copy(B1, temp_b);
    assert_false(bitset_run_container_ixor(temp_b, R3, &C));
    assert_int_equal(2000, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    temp_r = run_container_clone(R3);
    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_array_container_ixor(temp_r, A1, &C));
    assert_int_equal(2000, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    temp_r = run_container_clone(R1);
    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_run_container_ixor(temp_r, R3, &C));
    assert_int_equal(2000, array_container_cardinality(CAST_array(C)));
    array_container_free(CAST_array(C));
    C = NULL;

    temp_r = run_container_clone(R1);
    assert_true(run_bitset_container_ixor(temp_r, B2, &C));
    assert_int_equal(cx12, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    temp_a = array_container_clone(A2);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     array_run_container_ixor(temp_a, R1, &C));
    assert_int_equal(cx12, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    temp_b = bitset_container_create();
    bitset_container_copy(B1, temp_b);
    assert_true(bitset_run_container_ixor(temp_b, R2, &C));
    assert_int_equal(cx12, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    temp_r = run_container_clone(R1);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_array_container_ixor(temp_r, A2, &C));
    assert_int_equal(cx12, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    temp_r = run_container_clone(R1);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_run_container_ixor(temp_r, R2, &C));
    assert_int_equal(cx12, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    temp_r = run_container_clone(R4);
    assert_true(run_bitset_container_ixor(temp_r, B3, &C));
    int card_3_4 = bitset_container_cardinality(CAST_bitset(C));
    // assert_int_equal(card_3_4, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    temp_a = array_container_clone(A3);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     array_run_container_ixor(temp_a, R4, &C));
    // if this fails, either this bitset is wrong or the previous one...
    assert_int_equal(card_3_4, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    temp_b = bitset_container_create();
    bitset_container_copy(B3, temp_b);
    assert_true(bitset_run_container_ixor(temp_b, R4, &C));
    assert_int_equal(card_3_4, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    temp_r = run_container_clone(R3);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_array_container_ixor(temp_r, A4, &C));
    assert_int_equal(card_3_4, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    temp_r = run_container_clone(R4);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_run_container_ixor(temp_r, R3, &C));
    assert_int_equal(card_3_4, bitset_container_cardinality(CAST_bitset(C)));
    bitset_container_free(CAST_bitset(C));
    C = NULL;

    array_container_free(A1);
    array_container_free(A2);
    array_container_free(A3);
    array_container_free(AX);
    array_container_free(A4);

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(B3);
    bitset_container_free(BX);

    run_container_free(R1);
    run_container_free(R2);
    run_container_free(R3);
    run_container_free(R4);
}

DEFINE_TEST(run_iandnot_test) {
    array_container_t* A1 = array_container_create();
    array_container_t* A2 = array_container_create();
    array_container_t* A3 = array_container_create();
    array_container_t* A4 = array_container_create();
    array_container_t* AM = array_container_create();
    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* B3 = bitset_container_create();
    bitset_container_t* B4 = bitset_container_create();
    bitset_container_t* BM = bitset_container_create();
    run_container_t* R1 = run_container_create();
    run_container_t* R2 = run_container_create();
    run_container_t* R3 = run_container_create();
    run_container_t* R4 = run_container_create();

    // nb, array containers will be illegally big.
    for (int x = 0; x < (1 << 16); x++) {
        if (x % 5 < 3) {
            array_container_add(A1, x);
            bitset_container_set(B1, x);
            run_container_add(R1, x);
        }
    }

    for (int x = 0; x < (1 << 16); x++) {
        if (x % 62 < 37) {
            array_container_add(A2, x);
            bitset_container_set(B2, x);
            run_container_add(R2, x);
        }
    }

    for (int x = 0; x < (1 << 16); x++)
        if ((x % 5 < 3) && !(x % 62 < 37)) {
            array_container_add(AM, x);
            bitset_container_set(BM, x);
        }

    // the elements x%5 == 2 differ for less than 10k, otherwise same)
    for (int x = 0; x < (1 << 16); x++) {
        if ((x % 5 < 2) || ((x % 5 < 3) && (x > 10000))) {
            array_container_add(A3, x);
            bitset_container_set(B3, x);
            run_container_add(R3, x);
        }
    }

    int randstate = 1;  // for Oakenfull RNG, hope LSBits are nice
    for (int x = 0; x < (1 << 16); x++) {
        if (randstate % 4) {
            run_container_add(R4, x);
            array_container_add(A4, x);
            bitset_container_add(B4, x);
        }
        randstate = (3432 * randstate + 6789) % 9973;
    }

    int cm12 = array_container_cardinality(AM);  // expected xor for ?1 and ?2

    container_t* BM_1 = NULL;

    run_container_t* temp_r = run_container_clone(R1);
    assert_false(run_bitset_container_iandnot(temp_r, B1, &BM_1));
    assert_int_equal(0, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    bitset_container_t* temp_b = bitset_container_create();
    bitset_container_copy(B1, temp_b);
    assert_false(bitset_run_container_iandnot(temp_b, R1, &BM_1));
    assert_int_equal(0, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    array_container_t* temp_a = array_container_clone(A1);
    array_run_container_iandnot(temp_a, R1);
    assert_int_equal(0, array_container_cardinality(temp_a));
    array_container_free(temp_a);

    temp_r = run_container_clone(R1);
    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_array_container_iandnot(temp_r, A1, &BM_1));
    assert_int_equal(0, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    // both run coding and array coding have same serialized size for
    // empty
    temp_r = run_container_clone(R1);
    int ret_type = run_run_container_iandnot(temp_r, R1, &BM_1);
    assert_int_not_equal(BITSET_CONTAINER_TYPE, ret_type);
    if (ret_type == RUN_CONTAINER_TYPE) {
        assert_int_equal(0, run_container_cardinality(CAST_run(BM_1)));
        run_container_free(CAST_run(BM_1));
    } else {
        assert_int_equal(0, array_container_cardinality(CAST_array(BM_1)));
        array_container_free(CAST_array(BM_1));
    }
    BM_1 = NULL;

    temp_r = run_container_clone(R1);
    assert_false(run_bitset_container_iandnot(temp_r, B3, &BM_1));
    assert_int_equal(2000, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    temp_a = array_container_clone(A1);
    array_run_container_iandnot(temp_a, R3);
    assert_int_equal(2000, array_container_cardinality(temp_a));
    array_container_free(temp_a);

    temp_b = bitset_container_create();
    bitset_container_copy(B1, temp_b);
    assert_false(bitset_run_container_iandnot(temp_b, R3, &BM_1));
    assert_int_equal(2000, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    temp_r = run_container_clone(R1);
    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_array_container_iandnot(temp_r, A3, &BM_1));
    assert_int_equal(2000, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    temp_r = run_container_clone(R1);
    assert_int_equal(ARRAY_CONTAINER_TYPE,
                     run_run_container_iandnot(temp_r, R3, &BM_1));
    assert_int_equal(2000, array_container_cardinality(CAST_array(BM_1)));
    array_container_free(CAST_array(BM_1));
    BM_1 = NULL;

    temp_r = run_container_clone(R1);
    assert_true(run_bitset_container_iandnot(temp_r, B2, &BM_1));
    assert_int_equal(cm12, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    temp_a = array_container_clone(A1);
    array_run_container_iandnot(temp_a, R2);
    assert_int_equal(cm12, array_container_cardinality(temp_a));
    array_container_free(temp_a);

    temp_b = bitset_container_create();
    bitset_container_copy(B1, temp_b);
    assert_true(bitset_run_container_iandnot(temp_b, R2, &BM_1));
    assert_int_equal(cm12, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    temp_r = run_container_clone(R1);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_array_container_iandnot(temp_r, A2, &BM_1));
    assert_int_equal(cm12, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    temp_r = run_container_clone(R1);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_run_container_iandnot(temp_r, R2, &BM_1));
    assert_int_equal(cm12, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    assert_true(bitset_bitset_container_andnot(B4, B3, &BM_1));
    int card_4_3 = bitset_container_cardinality(CAST_bitset(BM_1));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    temp_r = run_container_clone(R4);
    assert_true(run_bitset_container_iandnot(temp_r, B3, &BM_1));
    assert_int_equal(card_4_3, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    temp_a = array_container_clone(A4);
    array_run_container_iandnot(temp_a, R3);
    // if this fails, either this bitset is wrong or the previous one...
    assert_int_equal(card_4_3, array_container_cardinality(temp_a));
    array_container_free(temp_a);

    temp_b = bitset_container_create();
    bitset_container_copy(B4, temp_b);
    assert_true(bitset_run_container_iandnot(temp_b, R3, &BM_1));
    assert_int_equal(card_4_3, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    temp_r = run_container_clone(R4);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_array_container_iandnot(temp_r, A3, &BM_1));
    assert_int_equal(card_4_3, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    temp_r = run_container_clone(R4);
    assert_int_equal(BITSET_CONTAINER_TYPE,
                     run_run_container_iandnot(temp_r, R3, &BM_1));
    assert_int_equal(card_4_3, bitset_container_cardinality(CAST_bitset(BM_1)));
    bitset_container_free(CAST_bitset(BM_1));
    BM_1 = NULL;

    array_container_free(A1);
    array_container_free(A2);
    array_container_free(A3);
    array_container_free(AM);
    array_container_free(A4);

    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(B3);
    bitset_container_free(B4);
    bitset_container_free(BM);

    run_container_free(R1);
    run_container_free(R2);
    run_container_free(R3);
    run_container_free(R4);
}

/* test replicating bug seen on real data */
DEFINE_TEST(run_array_andnot_bug_test) {
    int runcontents[] = {
        196608, 196611, 196612, 196613, 196616, 196619, 196621, 196623, 196628,
        196629, 196630, 196631, 196632, 196633, 196634, 196635, 196636, 196638,
        196639, 196640, 196641, 196642, 196644, 196645, 196646, 196647, 196648,
        196649, 196650, 196652, 196653, 196654, 196656, 196658, 196659, 196660,
        196662, 196663, 196664, 196665, 196666, 196667, 196669, 196670, 196671,
        196672, 196673, 196674, 196675, 196677, 196678, 196679, 196680, 196682,
        196684, 196685, 196686, 196688, 196689, 196690, 196691, 196692, 196693,
        196694, 196695, 196697, 196698, 196699, 196700, 196701, 196702, 196703,
        196704, 196705, 196706, 196707, 196708, 196709, 196710, 196711, 196712,
        196713, 196714, 196715, 196717, 196719, 196720, 196722, 196723, 196725,
        196726, 196727, 196728, 196729, -1};
    int arraycontents[] = {196722, 196824, 196989, -1};

    run_container_t* r = run_container_create();
    array_container_t* a = array_container_create();

    for (int* p = runcontents; *p != -1; ++p) run_container_add(r, *p % 65536);
    for (int* p = arraycontents; *p != -1; ++p)
        array_container_add(a, *p % 65536);

    int kindofresult;
    container_t* result = 0;
    kindofresult = run_array_container_andnot(r, a, &result);
    assert_int_equal(ARRAY_CONTAINER_TYPE, kindofresult);
    assert_false(array_container_contains(CAST_array(result), 196722 % 65536));

    run_container_free(r);
    array_container_free(a);
    array_container_free(CAST_array(result));
}

DEFINE_TEST(array_negation_empty_test) {
    array_container_t* AI = array_container_create();
    bitset_container_t* BO = bitset_container_create();

    array_container_negation(AI, BO);

    assert_int_equal(bitset_container_cardinality(BO), (1 << 16));

    array_container_free(AI);
    bitset_container_free(BO);
}

DEFINE_TEST(array_negation_test) {
    int ctr = 0;
    array_container_t* AI = array_container_create();
    bitset_container_t* BO = bitset_container_create();

    for (int x = 0; x < (1 << 16); x += 29) {
        array_container_add(AI, (uint16_t)x);
        ++ctr;
    }

    array_container_negation(AI, BO);
    assert_int_equal(bitset_container_cardinality(BO), (1 << 16) - ctr);

    for (int x = 0; x < (1 << 16); x++) {
        if (x % 29 == 0) {
            assert_false(bitset_container_contains(BO, (uint16_t)x));
        } else {
            assert_true(bitset_container_contains(BO, (uint16_t)x));
        }
        array_container_add(AI, (uint16_t)x);
        ++ctr;
    }

    array_container_free(AI);
    bitset_container_free(BO);
}

static int array_negation_range_test(int r_start, int r_end, bool is_bitset) {
    bool result_is_bitset;
    int result_size_should_be = 0;

    array_container_t* AI = array_container_create();
    container_t* BO;  // bitset or array

    for (int x = 0; x < (1 << 16); x += 29) {
        array_container_add(AI, (uint16_t)x);
    }

    for (int x = 0; x < (1 << 16); x++) {
        if (x >= r_start && x < r_end)
            if (x % 29 != 0)
                result_size_should_be++;
            else {
            }
        else if (x % 29 == 0)
            result_size_should_be++;
    }

    result_is_bitset =
        array_container_negation_range(AI, r_start, r_end, &BO);
    uint8_t result_typecode = (result_is_bitset ? BITSET_CONTAINER_TYPE
                                                : ARRAY_CONTAINER_TYPE);

    int result_card = container_get_cardinality(BO, result_typecode);

    assert_int_equal(is_bitset, result_is_bitset);
    assert_int_equal(result_size_should_be, result_card);

    for (int x = 0; x < (1 << 16); x++) {
        bool should_be_present;
        if (x >= r_start && x < r_end)
            should_be_present = (x % 29 != 0);
        else
            should_be_present = (x % 29 == 0);

#ifndef UNVERBOSE_MIXED_CONTAINER
        if (should_be_present !=
            container_contains(BO, (uint16_t)x, result_typecode))
            printf("oops on %d\n", x);
#endif
        assert_int_equal(container_contains(BO, (uint16_t)x, result_typecode),
                         should_be_present);
    }
    container_free(BO, result_typecode);
    array_container_free(AI);
    return 1;
}

/* result is a bitset.  Range fits neatly in words */
DEFINE_TEST(array_negation_range_test1) {
    array_negation_range_test(0x4000, 0xc000, true);
}

/* result is a bitset.  Range begins and ends mid word */
DEFINE_TEST(array_negation_range_test1a) {
    array_negation_range_test(0x4010, 0xc010, true);
}
/* result is an array */
DEFINE_TEST(array_negation_range_test2) {
    array_negation_range_test(0x7f00, 0x8030, false);
}
/* Empty range.  result is a clone */
DEFINE_TEST(array_negation_range_test3) {
    array_negation_range_test(0x7800, 0x7800, false);
}

/* sparsity parameter 1=empty; k: every kth is NOT set; k=100 will
 * negate to
 * sparse */
static int bitset_negation_range_tests(int sparsity, int r_start, int r_end,
                                       bool is_bitset, bool inplace) {
    int ctr = 0;
    bitset_container_t* BI = bitset_container_create();
    container_t* BO;
    bool result_is_bitset;
    int result_size_should_be = 0;

    for (int x = 0; x < (1 << 16); x++) {
        if (x % sparsity) bitset_container_add(BI, (uint16_t)x);
        ++ctr;
    }

    for (int x = 0; x < (1 << 16); x++) {
        if (x >= r_start && x < r_end)
            if (x % sparsity == 0)
                result_size_should_be++;
            else {
            }
        else if (x % sparsity)
            result_size_should_be++;
    }

    if (inplace)
        result_is_bitset = bitset_container_negation_range_inplace(
            BI, r_start, r_end, &BO);
    else
        result_is_bitset =
            bitset_container_negation_range(BI, r_start, r_end, &BO);

    uint8_t result_typecode = (result_is_bitset ? BITSET_CONTAINER_TYPE
                                                : ARRAY_CONTAINER_TYPE);

    int result_card = container_get_cardinality(BO, result_typecode);

    assert_int_equal(is_bitset, result_is_bitset);

    if (is_bitset && inplace) {
        assert_true(BO == BI);  // it really is inplace
    } else {
        assert_false(BO == BI);  // it better not be inplace
    }

    assert_int_equal(result_size_should_be, result_card);

    for (int x = 0; x < (1 << 16); x++) {
        bool should_be_present;
        if (x >= r_start && x < r_end)
            should_be_present = (x % sparsity == 0);
        else
            should_be_present = (x % sparsity != 0);

#ifndef UNVERBOSE_MIXED_CONTAINER
        if (should_be_present !=
            container_contains(BO, (uint16_t)x, result_typecode))
            printf("oops on %d\n", x);
#endif
        assert_int_equal(container_contains(BO, (uint16_t)x, result_typecode),
                         should_be_present);
    }
    container_free(BO, result_typecode);
    if (!inplace) bitset_container_free(BI);
    // for inplace: input is either output, or it was already freed
    // internally

    return 1;
}

/* result is a bitset */
DEFINE_TEST(bitset_negation_range_test1) {
    // 33% density will be a bitmap and remain so after any range
    // negated
    bitset_negation_range_tests(3, 0x7f00, 0x8030, true, false);
}

/* result is a array */
DEFINE_TEST(bitset_negation_range_test2) {
    // 99% density will be a bitmap and become array when mostly flipped
    bitset_negation_range_tests(100, 0x080, 0xff80, false, false);
}

/* inplace: result is a bitset */
DEFINE_TEST(bitset_negation_range_inplace_test1) {
    // 33% density will be a bitmap and remain so after any range
    // negated
    bitset_negation_range_tests(3, 0x7f00, 0x8030, true, true);
}

/* inplace: result is a array */
DEFINE_TEST(bitset_negation_range_inplace_test2) {
    // 99% density will be a bitmap and become array when mostly flipped
    bitset_negation_range_tests(100, 0x080, 0xff80, false, true);
}

/* specify how often runs start (k).  Runs are length h, h+1, .. k-1, 1,
 * 2...*/
/* start_offset allows for data that begins outside a run */

static int run_negation_range_tests(int k, int h, int start_offset, int r_start,
                                    int r_end, int expected_type, bool inplace,
                                    bool expected_actual_inplace) {
    int card = 0;
    run_container_t* RI =
        run_container_create_given_capacity((1 << 16) / k + 1);
    container_t* BO;
    int returned_type;
    int result_size_should_be;
    bool result_should_be[1 << 16];

    assert(h < k);  // bad test call otherwise..not failure of code under test

    int runlen = h;
    for (int x = 0; x < (1 << 16) - start_offset; x++) {
        int offsetx = x + start_offset;
        if (x % k == 0) {
            int actual_runlen = runlen;
            if (offsetx + runlen > (1 << 16))
                actual_runlen = (1 << 16) - offsetx;

            // run_container_append does not dynamically increase its
            // array
            run_container_append_first(
                RI, MAKE_RLE16(offsetx, actual_runlen - 1));
            card += actual_runlen;
            if (++runlen == k) runlen = h;  // wrap after k-1 back to h.
        }
    }

    result_size_should_be = 0;

    for (int i = 0; i < (1 << 16); ++i) {
        bool in_zone = (i >= r_start && i < r_end);
        if (run_container_contains(RI, (uint16_t)i) ^ in_zone) {
            result_should_be[i] = true;
            ++result_size_should_be;
        } else
            result_should_be[i] = false;
    }
    if (inplace)
        returned_type = run_container_negation_range_inplace(RI, r_start, r_end,
                                                             &BO);
    else
        returned_type =
            run_container_negation_range(RI, r_start, r_end, &BO);

    uint8_t result_typecode = (uint8_t)returned_type;

    int result_card = container_get_cardinality(BO, result_typecode);

    assert_int_equal(expected_type, returned_type);

    if (expected_actual_inplace) {
        assert_true(BO == RI);  // it really is inplace
    } else {
        assert_false(BO == RI);  // it better not be inplace
    }

    assert_int_equal(result_size_should_be, result_card);

    for (int x = 0; x < (1 << 16); x++) {
#ifndef UNVERBOSE_MIXED_CONTAINER
        if (container_contains(BO, (uint16_t)x, result_typecode) !=
            result_should_be[x])
            printf("problem at index %d should be (but isnt) %d\n", x,
                   (int)result_should_be[x]);
#endif
        assert_int_equal(container_contains(BO, (uint16_t)x, result_typecode),
                         result_should_be[x]);
    }
    // assert_int_equal(result_size_should_be, result_card);
    container_free(BO, result_typecode);
    if (!inplace) run_container_free(RI);
    // for inplace: input is either output, or it was already freed
    // internally

    return 1;
}

/* Version that does not check whether return types and inplaceness are
 * right */

static int run_negation_range_tests_simpler(int k, int h, int start_offset,
                                            int r_start, int r_end,
                                            bool inplace) {
    int card = 0;
    run_container_t* RI =
        run_container_create_given_capacity((1 << 16) / k + 1);
    container_t* BO;
    int returned_type;
    int result_size_should_be;
    bool result_should_be[1 << 16];

    assert(h < k);

    int runlen = h;
    for (int x = 0; x < (1 << 16) - start_offset; x++) {
        int offsetx = x + start_offset;
        if (x % k == 0) {
            int actual_runlen = runlen;
            if (offsetx + runlen > (1 << 16))
                actual_runlen = (1 << 16) - offsetx;

            run_container_append_first(
                RI, MAKE_RLE16(offsetx, actual_runlen));
            card += actual_runlen;
            if (++runlen == k) runlen = h;
        }
    }

    result_size_should_be = 0;

    for (int i = 0; i < (1 << 16); ++i) {
        bool in_zone = (i >= r_start && i < r_end);
        if (run_container_contains(RI, (uint16_t)i) ^ in_zone) {
            result_should_be[i] = true;
            ++result_size_should_be;
        } else
            result_should_be[i] = false;
    }
    if (inplace)
        returned_type = run_container_negation_range_inplace(RI, r_start, r_end,
                                                             &BO);
    else
        returned_type =
            run_container_negation_range(RI, r_start, r_end, &BO);

    uint8_t result_typecode = (uint8_t)returned_type;

    int result_card = container_get_cardinality(BO, result_typecode);

    assert_int_equal(result_size_should_be, result_card);

    for (int x = 0; x < (1 << 16); x++) {
#ifndef UNVERBOSE_MIXED_CONTAINER
        if (container_contains(BO, (uint16_t)x, result_typecode) !=
            result_should_be[x])
            printf("problem at index %d should be (but isnt) %d\n", x,
                   (int)result_should_be[x]);
#endif
        assert_int_equal(container_contains(BO, (uint16_t)x, result_typecode),
                         result_should_be[x]);
    }
    container_free(BO, result_typecode);
    if (!inplace) run_container_free(RI);
    return 1;
}

static int run_many_negation_range_tests_simpler(bool inplace) {
    for (int h = 1; h < 100; h *= 3) {
        printf("h=%d\n", h);
        for (int k = h + 1; k < 100; k = k * 1.5 + 1) {
            printf("  k=%d\n", k);
            for (int start_offset = 0; start_offset < 1000;
                 start_offset = start_offset * 2.7 + 1) {
                for (int r_start = 0; r_start < 65535; r_start += 10013)
                    for (int span = 0; r_start + span < 65536;
                         span = span * 3 + 1) {
                        run_negation_range_tests_simpler(
                            k, h, start_offset, r_start, r_start + span,
                            inplace);
                    }
            }
        }
    }
    return 1;
}

DEFINE_TEST(run_many_negation_range_tests_simpler_notinplace) {
    run_many_negation_range_tests_simpler(false);
}

DEFINE_TEST(run_many_negation_range_tests_simpler_inplace) {
    run_many_negation_range_tests_simpler(true);
}

/* result is a bitset */
DEFINE_TEST(run_negation_range_inplace_test1) {
    // runs of length 7, 8, 9 begin every 10
    // starting at 0.
    // (should not have been run encoded, but...)
    // last run starts at 65530 hence we end in a
    // run
    // negation over whole range.  Result should be
    // bitset

    run_negation_range_tests(10, 7, 0, 0x0000, 0x10000,
                             BITSET_CONTAINER_TYPE, true,
                             false);  // request but don't get inplace
}

DEFINE_TEST(run_negation_range_inplace_test2) {
    // runs of length 7, 8, 9 begin every 10
    // starting at 1.
    // last run starts at 65531 hence we end in a
    // run
    // negation over whole range.  Result should be
    // bitset

    run_negation_range_tests(10, 7, 1, 0x0000, 0x10000,
                             BITSET_CONTAINER_TYPE, true,
                             false);  // request but don't get inplace
}

DEFINE_TEST(run_negation_range_inplace_test3) {
    // runs of length 2,3,..9 begin every 10
    // starting at 1.
    // last run starts at 65531. Run length is (6553
    // % 8)+2 = 3.
    // So 65535 stores 0.
    // negation over whole range.  Result should be
    // bitset

    run_negation_range_tests(10, 2, 1, 0x0000, 0x10000,
                             BITSET_CONTAINER_TYPE, true,
                             false);  // request but don't get inplace
}

/* Results are going to be arrays*/
DEFINE_TEST(run_negation_range_inplace_test4) {
    // runs of length 999 begin every 1000 starting
    // at 0.
    // last run starts at 65000 hence we end in a
    // run
    // negation over whole range.
    // Result should be array

    run_negation_range_tests(1000, 999, 0, 0x0000, 0x10000,
                             ARRAY_CONTAINER_TYPE, true,
                             false);  // request but don't get inplace
}

DEFINE_TEST(run_negation_range_inplace_test5) {
    // runs of length 999 begin every 10000 starting
    // at 1.
    // last run starts at 65001 hence we end in a
    // run.
    // negation over whole range.  Result should be
    // bitset

    run_negation_range_tests(1000, 999, 1, 0x0000, 0x10000,
                             ARRAY_CONTAINER_TYPE, true,
                             false);  // request but don't get inplace
}

DEFINE_TEST(run_negation_range_inplace_test6) {
    // runs of length 999 begin every 10000 starting
    // at 536
    // last run starts at 64536.
    // So 65535 stores 0.
    // negation over whole range except some
    // initial.  Result should be array

    run_negation_range_tests(1000, 999, 536, 530, 0x10000,
                             ARRAY_CONTAINER_TYPE, true,
                             false);  // request but don't get inplace
}

/* Results are going to be runs*/
DEFINE_TEST(run_negation_range_inplace_test7) {
    // short runs of length 2, 3, .. 67 begin every
    // 1000 starting at 550.
    // last run starts at 65550 hence we end in a
    // run.
    // negation over whole range.  Result should be
    // run.
    // should always fit in the previous space

    run_negation_range_tests(1000, 2, 550, 0x0000, 0x10000,
                             RUN_CONTAINER_TYPE, true,
                             true);  // request and  get inplace
}

DEFINE_TEST(run_negation_range_inplace_test8) {
    // runs of length 2..67 begin every 10000
    // starting at 0.
    // last run starts at 65000 hence we end outside
    // a run
    // negation over whole range.  Result should be
    // run and will fit.

    run_negation_range_tests(1000, 2, 0, 0x0000, 0x10000,
                             RUN_CONTAINER_TYPE, true,
                             true);  // request, get inplace
}

DEFINE_TEST(run_negation_range_inplace_test9) {
    // runs of length 2..67 begin every 10000
    // starting at 1
    // last run starts at 64001.
    // So 65535 stores 0.
    // negation over whole range.  Result should
    // have one run
    // more than original, and buffer happens to not
    // have any extra space.

    run_negation_range_tests(1000, 2, 1, 0x0000, 0x10000,
                             RUN_CONTAINER_TYPE, true,
                             false);  // request, but not get, inplace
}

// now, 9 more tests that do not request inplace.

/* result is a bitset */
DEFINE_TEST(run_negation_range_test1) {
    // runs of length 7, 8, 9 begin every 10
    // starting at 0.
    // (should not have been run encoded, but...)
    // last run starts at 65530 hence we end in a
    // run
    // negation over whole range.  Result should be
    // bitset

    run_negation_range_tests(10, 7, 0, 0x0000, 0x10000,
                             BITSET_CONTAINER_TYPE, false, false);
}

DEFINE_TEST(run_negation_range_test2) {
    // runs of length 7, 8, 9 begin every 10
    // starting at 1.
    // last run starts at 65531 hence we end in a
    // run
    // negation over whole range.  Result should be
    // bitset

    run_negation_range_tests(10, 7, 1, 0x0000, 0x10000,
                             BITSET_CONTAINER_TYPE, false, false);
}

DEFINE_TEST(run_negation_range_test3) {
    // runs of length 2,3,..9 begin every 10
    // starting at 1.
    // last run starts at 65531. Run length is (6553
    // % 8)+2 = 3.
    // So 65535 stores 0.
    // negation over whole range.  Result should be
    // bitset

    run_negation_range_tests(10, 2, 1, 0x0000, 0x10000,
                             BITSET_CONTAINER_TYPE, false,
                             false);  // request but don't get inplace
}

/* Results are going to be arrays*/
DEFINE_TEST(run_negation_range_test4) {
    // runs of length 999 begin every 1000 starting
    // at 0.
    // last run starts at 65000 hence we end in a
    // run
    // negation over whole range.  Result should be
    // array

    run_negation_range_tests(1000, 999, 0, 0x0000, 0x10000,
                             ARRAY_CONTAINER_TYPE, false, false);
}

DEFINE_TEST(run_negation_range_test5) {
    // runs of length 999 begin every 10000 starting
    // at 1.
    // last run starts at 65001 hence we end in a
    // run
    // negation over whole range.  Result should be
    // bitset

    run_negation_range_tests(1000, 999, 1, 0x0000, 0x10000,
                             ARRAY_CONTAINER_TYPE, false, false);
}

DEFINE_TEST(run_negation_range_test6) {
    // runs of length 999 begin every 10000 starting
    // at 536
    // last run starts at 64536.
    // So 65535 stores 0.
    // negation over whole range except initial
    // fragment. Result should be array

    run_negation_range_tests(1000, 999, 536, 530, 0x10000,
                             ARRAY_CONTAINER_TYPE, false, false);
}

/* Results are going to be runs*/
DEFINE_TEST(run_negation_range_test7) {
    // short runs of length 2, 3, .. 67 begin every
    // 1000 starting at 550.
    // last run starts at 65550 hence we end in a
    // run.
    // negation over whole range.  Result should be
    // run.
    // should always fit in the previous space

    run_negation_range_tests(1000, 2, 550, 0x0000, 0x10000,
                             RUN_CONTAINER_TYPE, false, false);
}

DEFINE_TEST(run_negation_range_test8) {
    // runs of length 2..67 begin every 10000
    // starting at 0.
    // last run starts at 65000 hence we end outside
    // a run
    // negation over whole range.  Result should be
    // run and will fit.

    run_negation_range_tests(1000, 2, 0, 0x0000, 0x10000,
                             RUN_CONTAINER_TYPE, false, false);
}

DEFINE_TEST(run_negation_range_test9) {
    // runs of length 2..67 begin every 10000
    // starting at 1
    // last run starts at 64001.
    // So 65535 stores 0.
    // negation over whole range.  Result should be
    // have one run
    // more than original, but we think buffer will
    // usually have space  :)

    run_negation_range_tests(1000, 2, 1, 0x0000, 0x10000,
                             RUN_CONTAINER_TYPE, false, false);
}

int main() {
    tellmeall();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(array_bitset_and_or_xor_andnot_test),
        cmocka_unit_test(array_bitset_run_lazy_xor_test),
        cmocka_unit_test(run_xor_test),
        cmocka_unit_test(run_ixor_test),
        cmocka_unit_test(run_andnot_test),
        cmocka_unit_test(run_iandnot_test),
        cmocka_unit_test(run_array_andnot_bug_test),
        cmocka_unit_test(array_bitset_ixor_test),
        cmocka_unit_test(array_bitset_iandnot_test),
        cmocka_unit_test(array_negation_empty_test),
        cmocka_unit_test(array_negation_test),
        cmocka_unit_test(array_negation_range_test1),
        cmocka_unit_test(array_negation_range_test1a),
        cmocka_unit_test(array_negation_range_test2),
        cmocka_unit_test(array_negation_range_test3),
        cmocka_unit_test(bitset_negation_range_test1),
        cmocka_unit_test(bitset_negation_range_test2),
        cmocka_unit_test(bitset_negation_range_inplace_test1),
        cmocka_unit_test(bitset_negation_range_inplace_test2),
        cmocka_unit_test(run_negation_range_inplace_test1),
        cmocka_unit_test(run_negation_range_inplace_test2),
        cmocka_unit_test(run_negation_range_inplace_test3),
        cmocka_unit_test(run_negation_range_inplace_test4),
        cmocka_unit_test(run_negation_range_inplace_test5),
        cmocka_unit_test(run_negation_range_inplace_test6),
        cmocka_unit_test(run_negation_range_inplace_test7),
        cmocka_unit_test(run_negation_range_inplace_test8),
        cmocka_unit_test(run_negation_range_inplace_test9),
        cmocka_unit_test(run_negation_range_test1),
        cmocka_unit_test(run_negation_range_test2),
        cmocka_unit_test(run_negation_range_test3),
        cmocka_unit_test(run_negation_range_test4),
        cmocka_unit_test(run_negation_range_test5),
        cmocka_unit_test(run_negation_range_test6),
        cmocka_unit_test(run_negation_range_test7),
        cmocka_unit_test(run_negation_range_test8),
        cmocka_unit_test(run_negation_range_test9),
        /* two very expensive tests that probably should usually be
           omitted */

        /*cmocka_unit_test(
            run_many_negation_range_tests_simpler_notinplace),  // lots
        of
                                                                //
        partial
                                                                //
        ranges,
        cmocka_unit_test(run_many_negation_range_tests_simpler_inplace),*/
        /* */
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
