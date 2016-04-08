/*
 * run_container_unit.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "containers/run.h"
#include "misc/configreport.h"

#include "test.h"

// returns 0 on error, 1 if ok.
int printf_test() {
    DESCRIBE_TEST;

    run_container_t* B = run_container_create();
    run_container_add(B, (uint16_t)1);
    run_container_add(B, (uint16_t)2);
    run_container_add(B, (uint16_t)3);
    run_container_add(B, (uint16_t)10);
    run_container_add(B, (uint16_t)10000);
    run_container_printf(B);  // does it crash?
    printf("\n");
    run_container_free(B);
    return 1;
}

// returns 0 on error, 1 if ok.
int add_contains_test() {
    DESCRIBE_TEST;

    run_container_t* B = run_container_create();
    int x;
    if (B == NULL) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    int expectedcard = 0;
    for (x = 0; x < 1 << 16; x += 3) {
        bool wasadded = run_container_add(B, (uint16_t)x);
        if (!wasadded) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        if (!run_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        expectedcard++;
        if (run_container_cardinality(B) != expectedcard) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        if (run_container_cardinality(B) > B->capacity) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
    }
    for (x = 0; x < 1 << 16; x++) {
        int isset = run_container_contains(B, (uint16_t)x);
        int shouldbeset = (x / 3 * 3 == x);
        if (isset != shouldbeset) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
    }
    if (run_container_cardinality(B) != (1 << 16) / 3 + 1) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        run_container_free(B);
        return 0;
    }
    for (x = 0; x < 1 << 16; x += 3) {
        if (!run_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        bool wasremoved = run_container_remove(B, (uint16_t)x);
        if (!wasremoved) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        expectedcard--;
        if (run_container_cardinality(B) != expectedcard) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }

        if (run_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
    }
    if (run_container_cardinality(B) != 0) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        run_container_free(B);
        return 0;
    }
    if (run_container_cardinality(B) != 0) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        run_container_free(B);
        return 0;
    }

    for (x = 65535; x >= 0; x -= 3) {
        bool wasadded = run_container_add(B, (uint16_t)x);
        if (!wasadded) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        if (!run_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        expectedcard++;
        if (run_container_cardinality(B) != expectedcard) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        if (run_container_cardinality(B) > B->capacity) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
    }
    if (run_container_cardinality(B) != (1 << 16) / 3 + 1) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        run_container_free(B);
        return 0;
    }
    for (x = 0; x < 1 << 16; x++) {
        int isset = run_container_contains(B, (uint16_t)x);
        int shouldbeset = (x / 3 * 3 == x);
        if (isset != shouldbeset) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
    }
    for (x = 0; x < 1 << 16; x += 3) {
        if (!run_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        bool wasremoved = run_container_remove(B, (uint16_t)x);
        if (!wasremoved) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
        expectedcard--;
        if (run_container_cardinality(B) != expectedcard) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }

        if (run_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            run_container_free(B);
            return 0;
        }
    }

    run_container_free(B);
    return 1;
}

// returns 0 on error, 1 if ok.
int and_or_test() {
    DESCRIBE_TEST;

    run_container_t* B1 = run_container_create();
    run_container_t* B2 = run_container_create();
    run_container_t* BI = run_container_create();
    run_container_t* BO = run_container_create();

    int x, c, ci, co;
    if ((B1 == NULL) || (B2 == NULL) || (BO == NULL) || (BI == NULL)) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    for (x = 0; x < (1 << 16); x += 3) {
        run_container_add(B1, (uint16_t)x);
        run_container_add(BI, (uint16_t)x);
    }
    for (x = 0; x < (1 << 16);
         x += 62) {  // important: 62 is not divisible by 3
        run_container_add(B2, (uint16_t)x);
        run_container_add(BI, (uint16_t)x);
    }
    for (x = 0; x < (1 << 16); x += 62 * 3) {
        run_container_add(BO, (uint16_t)x);
    }
    // we interleave O and I on purpose (to trigger bugs!)
    ci = run_container_cardinality(BO);  // expected intersection
    co = run_container_cardinality(BI);  // expected union
    run_container_intersection(B1, B2, BI);
    c = run_container_cardinality(BI);
    if (c != ci) {
        printf("expected int card %d got %d \n", c, ci);
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    run_container_union(B1, B2, BO);
    c = run_container_cardinality(BO);
    if (c != co) {
        printf("expected U card %d got %d \n", c, co);
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    run_container_free(B1);
    run_container_free(B2);
    run_container_free(BO);
    run_container_free(BI);
    return 1;
}

// returns 0 on error, 1 if ok.
int to_uint32_array_test() {
    DESCRIBE_TEST;

    for (int offset = 1; offset < 128; offset *= 2) {
        run_container_t* B = run_container_create();
        for (int k = 0; k < (1 << 16); k += offset) {
            run_container_add(B, (uint16_t)k);
        }
        int card = run_container_cardinality(B);
        uint32_t* out = malloc(sizeof(uint32_t) * card);
        int nc = run_container_to_uint32_array(out, B, 0);
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
        run_container_free(B);
    }
    return 1;
}

int main() {
    tellmeall();
    if (!printf_test()) return -1;
    if (!add_contains_test()) return -1;
    if (!and_or_test()) return -1;
    if (!to_uint32_array_test()) return -1;

    return EXIT_SUCCESS;
}
