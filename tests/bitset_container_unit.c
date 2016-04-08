/*
 * bitset_container_unit.c
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "containers/bitset.h"
#include "misc/configreport.h"

#include "test.h"

// returns 0 on error, 1 if ok.
int printf_test() {
    DESCRIBE_TEST;

    bitset_container_t* B = bitset_container_create();
    bitset_container_set(B, (uint16_t)1);
    bitset_container_set(B, (uint16_t)2);
    bitset_container_set(B, (uint16_t)3);
    bitset_container_set(B, (uint16_t)10);
    bitset_container_set(B, (uint16_t)10000);
    bitset_container_printf(B);  // does it crash?
    printf("\n");
    bitset_container_free(B);
    return 1;
}

// returns 0 on error, 1 if ok.
int set_get_test() {
    DESCRIBE_TEST;

    bitset_container_t* B = bitset_container_create();
    int x;
    if (B == NULL) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    for (x = 0; x < 1 << 16; x++) {
        assert(!bitset_container_get(B, x));
    }
    for (x = 0; x < 1 << 16; x += 3) {
        assert(bitset_container_cardinality(B) == x / 3);
        assert(!bitset_container_get(B, x));
        bitset_container_set(B, (uint16_t)x);
        assert(bitset_container_get(B, x));
        assert(bitset_container_cardinality(B) == x / 3 + 1);
    }
    for (x = 0; x < 1 << 16; x++) {
        int isset = bitset_container_get(B, (uint16_t)x);
        int shouldbeset = (x / 3 * 3 == x);
        if (isset != shouldbeset) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            bitset_container_free(B);
            return 0;
        }
    }
    if (bitset_container_cardinality(B) != (1 << 16) / 3 + 1) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        bitset_container_free(B);
        return 0;
    }
    if (bitset_container_compute_cardinality(B) != (1 << 16) / 3 + 1) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        bitset_container_free(B);
        return 0;
    }
    for (x = 0; x < 1 << 16; x += 3) {
        bitset_container_unset(B, (uint16_t)x);
    }
    if (bitset_container_cardinality(B) != 0) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        bitset_container_free(B);
        return 0;
    }
    if (bitset_container_compute_cardinality(B) != 0) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        bitset_container_free(B);
        return 0;
    }
    bitset_container_free(B);
    return 1;
}

// returns 0 on error, 1 if ok.
int and_or_test() {
    DESCRIBE_TEST;

    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BI = bitset_container_create();
    bitset_container_t* BO = bitset_container_create();

    int x, c, ci, co;
    if ((B1 == NULL) || (B2 == NULL) || (BO == NULL) || (BI == NULL)) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    for (x = 0; x < (1 << 16); x += 3) {
        bitset_container_set(B1, (uint16_t)x);
        bitset_container_set(BI, (uint16_t)x);
    }
    for (x = 0; x < (1 << 16);
         x += 62) {  // important: 62 is not divisible by 3
        bitset_container_set(B2, (uint16_t)x);
        bitset_container_set(BI, (uint16_t)x);
    }
    for (x = 0; x < (1 << 16); x += 62 * 3) {
        bitset_container_set(BO, (uint16_t)x);
    }
    // we interleave O and I on purpose (to trigger bugs!)
    ci = bitset_container_compute_cardinality(BO);  // expected intersection
    co = bitset_container_compute_cardinality(BI);  // expected union
    bitset_container_and_nocard(B1, B2, BI);
    c = bitset_container_compute_cardinality(BI);
    if (c != ci) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    c = bitset_container_and(B1, B2, BI);
    ;
    if (c != ci) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    bitset_container_or_nocard(B1, B2, BO);
    c = bitset_container_compute_cardinality(BO);
    if (c != co) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    c = bitset_container_or(B1, B2, BO);
    if (c != co) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(BI);
    bitset_container_free(BO);
    return 1;
}

// returns 0 on error, 1 if ok.
int xor_test() {
    DESCRIBE_TEST;

    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BI = bitset_container_create();

    int x, c, cx;

    for (x = 0; x < (1 << 16); x += 3) {
        bitset_container_set(B1, (uint16_t)x);
        bitset_container_set(BI, (uint16_t)x);
    }
    for (x = 0; x < (1 << 16);
         x += 62) {  // important: 62 is not divisible by 3
        bitset_container_set(B2, (uint16_t)x);
        bitset_container_set(BI, (uint16_t)x);
    }
    for (x = 0; x < (1 << 16); x += 62 * 3) {
        bitset_container_unset(BI, (uint16_t)x);
    }
    cx = bitset_container_compute_cardinality(BI);  // expected xor
    bitset_container_xor_nocard(B1, B2, BI);
    c = bitset_container_compute_cardinality(BI);
    if (c != cx) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    c = bitset_container_xor(B1, B2, BI);
    if (c != cx) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(BI);
    return 1;
}

// returns 0 on error, 1 if ok.
int andnot_test() {
    DESCRIBE_TEST;

    bitset_container_t* B1 = bitset_container_create();
    bitset_container_t* B2 = bitset_container_create();
    bitset_container_t* BI = bitset_container_create();

    int x, c, cn;

    for (x = 0; x < (1 << 16); x += 3) {
        bitset_container_set(B1, (uint16_t)x);
        bitset_container_set(BI, (uint16_t)x);
    }
    for (x = 0; x < (1 << 16);
         x += 62) {  // important: 62 is not divisible by 3
        bitset_container_set(B2, (uint16_t)x);
        bitset_container_unset(BI, (uint16_t)x);
    }

    cn = bitset_container_compute_cardinality(BI);  // expected andnot
    bitset_container_andnot_nocard(B1, B2, BI);
    c = bitset_container_compute_cardinality(BI);
    if (c != cn) {
        printf("c= %d but cn = %d Bug %s, line %d \n", c, cn, __FILE__,
               __LINE__);
        return 0;
    }
    c = bitset_container_andnot(B1, B2, BI);
    if (c != cn) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    bitset_container_free(B1);
    bitset_container_free(B2);
    bitset_container_free(BI);
    return 1;
}

// returns 0 on error, 1 if ok.
int to_uint32_array_test() {
    DESCRIBE_TEST;

    for (int offset = 1; offset < 128; offset *= 2) {
        bitset_container_t* B = bitset_container_create();
        for (int k = 0; k < (1 << 16); k += offset) {
            bitset_container_set(B, (uint16_t)k);
        }
        int card = bitset_container_cardinality(B);
        uint32_t* out = malloc(sizeof(uint32_t) * (card + sizeof(__m256i)));
        int nc = bitset_container_to_uint32_array(out, B, 0);
        if (card != nc) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            return 0;
        }
        for (int k = 1; k < nc; ++k) {
            if (out[k] != offset + out[k - 1]) {
                printf("Bug %s, line %d \n", __FILE__, __LINE__);
                return 0;
            }
        }
        free(out);
        bitset_container_free(B);
    }
    return 1;
}

int main() {
    tellmeall();

    if (!printf_test()) return -1;
    if (!set_get_test()) return -1;
    if (!and_or_test()) return -1;
    if (!xor_test()) return -1;
    if (!andnot_test()) return -1;
    if (!to_uint32_array_test()) return -1;

    return EXIT_SUCCESS;
}
