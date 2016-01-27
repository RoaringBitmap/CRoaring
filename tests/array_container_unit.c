/*
 * array_container_unit.c
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "containers/array.h"
#include "misc/configreport.h"

// returns 0 on error, 1 if ok.
int printf_test() {
    printf("[%s] %s\n", __FILE__, __func__);
    array_container_t* B = array_container_create();
	array_container_add(B, (uint16_t)1);
	array_container_add(B, (uint16_t)2);
	array_container_add(B, (uint16_t)3);
	array_container_add(B, (uint16_t)10);
	array_container_add(B, (uint16_t)10000);
	array_container_printf(B); // does it crash?
	printf("\n");
    array_container_free(B);
    return 1;
}

// returns 0 on error, 1 if ok.
int add_contains_test() {
    array_container_t* B = array_container_create();
    int x;
    printf("[%s] %s\n", __FILE__, __func__);
    if (B == NULL) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        return 0;
    }
    int expectedcard = 0;
    for (x = 0; x < 1 << 16; x += 3) {
    	bool wasadded = array_container_add(B, (uint16_t)x);
        if(!wasadded) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
        if(!array_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
        expectedcard++;
        if(B->cardinality != expectedcard) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
        if(B->cardinality > B->capacity) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
    }
    for (x = 0; x < 1 << 16; x++) {
        int isset = array_container_contains(B, (uint16_t)x);
        int shouldbeset = (x / 3 * 3 == x);
        if (isset != shouldbeset) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
    }
    if (array_container_cardinality(B) != (1 << 16) / 3 + 1) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        array_container_free(B);
        return 0;
    }
    for (x = 0; x < 1 << 16; x += 3) {
        if(!array_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
    	bool wasremoved = array_container_remove(B, (uint16_t)x);
        if(!wasremoved) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
        expectedcard--;
        if(B->cardinality != expectedcard) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }

        if(array_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }

    }
    if (array_container_cardinality(B) != 0) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        array_container_free(B);
        return 0;
    }
    if (array_container_cardinality(B) != 0) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        array_container_free(B);
        return 0;
    }

    for (x = 65535; x >=0; x -= 3) {
    	bool wasadded = array_container_add(B, (uint16_t)x);
        if(!wasadded) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
        if(!array_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
        expectedcard++;
        if(B->cardinality != expectedcard) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
        if(B->cardinality > B->capacity) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
   }
    if (array_container_cardinality(B) != (1 << 16) / 3 + 1) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
        array_container_free(B);
        return 0;
    }
     for (x = 0; x < 1 << 16; x++) {
        int isset = array_container_contains(B, (uint16_t)x);
        int shouldbeset = (x / 3 * 3 == x);
        if (isset != shouldbeset) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
    }
    for (x = 0; x < 1 << 16; x += 3) {
        if(!array_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
    	bool wasremoved = array_container_remove(B, (uint16_t)x);
        if(!wasremoved) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
        expectedcard--;
        if(B->cardinality != expectedcard) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }

        if(array_container_contains(B, (uint16_t)x)) {
            printf("Bug %s, line %d \n", __FILE__, __LINE__);
            array_container_free(B);
            return 0;
        }
    }

    array_container_free(B);
    return 1;
}

// returns 0 on error, 1 if ok.
int and_or_test() {
	array_container_t* B1 = array_container_create();
	array_container_t* B2 = array_container_create();
	array_container_t* BI = array_container_create();
	array_container_t* BO = array_container_create();

	int x, c, ci, co;
	printf("[%s] %s\n", __FILE__, __func__);
    if ((B1 == NULL) || (B2 == NULL) || (BO == NULL) || (BI == NULL)) {
	        printf("Bug %s, line %d \n", __FILE__, __LINE__);
	        return 0;
	}
	for (x = 0; x < (1<<16); x += 3) {
	        array_container_add(B1, (uint16_t)x);
	        array_container_add(BI, (uint16_t)x);

	}
	for (x = 0; x < (1<<16); x += 62) {// important: 62 is not divisible by 3
	        array_container_add(B2, (uint16_t)x);
	        array_container_add(BI, (uint16_t)x);
	}
	for (x = 0; x < (1<<16); x += 62*3) {
	        array_container_add(BO, (uint16_t)x);
	}
	// we interleave O and I on purpose (to trigger bugs!)
	ci = array_container_cardinality(BO);// expected intersection
	co = array_container_cardinality(BI);// expected union
	array_container_intersection(B1,B2, BI);
	c = array_container_cardinality(BI);
	if(c != ci) {
        printf("Bug %s, line %d \n", __FILE__, __LINE__);
       return 0;
	}
	array_container_union(B1,B2, BO);
	c = array_container_cardinality(BO);
	if(c != co) {
	    printf("Bug %s, line %d \n", __FILE__, __LINE__);
	    return 0;
	}
	return 1;
}

int main() {
	tellmeall();
    if (!printf_test()) return -1;
	if (!add_contains_test()) return -1;
    if (!and_or_test()) return -1;

    printf("[%s] your code might be ok.\n", __FILE__);
    return 0;
}
