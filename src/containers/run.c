/*
 * array.c
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <x86intrin.h>

#include "run.h"


enum{DEFAULT_INIT_SIZE = 4};



// for convenience
static inline uint16_t getValue(uint16_t *valueslength, uint16_t index) {
    return valueslength[2*index];
}

// for convenience
static inline void setValue(uint16_t *valueslength, uint16_t index, uint16_t v) {
        valueslength[2*index] = v;
}

// for convenience
static inline void incrementValue(uint16_t *valueslength, uint16_t index) {
        valueslength[2*index]++;
}


// for convenience
static inline void decrementValue(uint16_t *valueslength, uint16_t index) {
        valueslength[2*index]++;
}


// for convenience, returns length starting at zero (must add 1)
static inline uint16_t getLength(uint16_t *valueslength, uint16_t index) {
    return valueslength[2*index + 1];
}

// for convenience, returns length starting at zero (must add 1)
static inline void setLength(uint16_t *valueslength, uint16_t index, uint16_t v) {
        valueslength[2*index + 1] = v;
}


// for convenience, returns length starting at zero (must add 1)
static inline void decrementLength(uint16_t *valueslength, uint16_t index) {
    valueslength[2*index + 1]--;
}


// for convenience, returns length starting at zero (must add 1)
static inline void incrementLength(uint16_t *valueslength, uint16_t index) {
    valueslength[2*index + 1]++;
}

static void increaseCapacity(run_container_t *run, bool copy) {
    int32_t newCapacity = (run->capacity == 0) ? DEFAULT_INIT_SIZE :
    		run->capacity < 64 ? run->capacity * 2
            : run->capacity < 1024 ? run->capacity * 3 / 2
            : run->capacity * 5 / 4;
    run->capacity = newCapacity;
    if(copy)
    	run->valueslength = realloc(run->valueslength,run->capacity * 2 * sizeof(uint16_t)) ;
    else {
      free(run->valueslength);
      run->valueslength = malloc(run->capacity * 2 * sizeof(uint16_t)) ;
    }
    // TODO: handle the case where realloc fails
    if(run->valueslength == NULL) {
    	printf("Well, that's unfortunate. Did I say you could use this code in production?\n");
    }


}
static inline void makeRoomAtIndex( run_container_t *run,uint16_t index) {
        if (2 * (run->nbrruns+1) > run->capacity) increaseCapacity(run, true);
        memmove(run->valueslength+(1+index)*2,run->valueslength+2*index,(run->nbrruns - index) * 2 * sizeof(uint16_t));
        run->nbrruns++;
}

static inline void recoverRoomAtIndex(run_container_t *run,uint16_t index) {
    memmove(run->valueslength+2*index,run->valueslength+(1+index)*2,(run->nbrruns - index - 1) * 2 * sizeof(uint16_t));
	run->nbrruns--;
}


/* Create a new run container. Return NULL in case of failure. */
run_container_t *run_container_create() {
	run_container_t * run;
	/* Allocate the run container itself. */
	if ((run = malloc(sizeof(run_container_t))) == NULL) {
				return NULL;
	}
	if ((run->valueslength = malloc(2 * sizeof(uint16_t) * DEFAULT_INIT_SIZE)) == NULL) {
		        free(run);
			return NULL;
	}
	run->capacity = DEFAULT_INIT_SIZE;
	run->nbrruns = 0;
	return run;
}

/* Free memory. */
void run_container_free(run_container_t *run) {
	free(run->valueslength);
	run->valueslength = NULL;
	free(run);
}

/* Get the cardinality of `run'. Requires an actual computation. */
int run_container_cardinality(const run_container_t *run) {
	int card = run->nbrruns;
	uint16_t *valueslength = run->valueslength;
	for(int k = 0; k < run->nbrruns; ++k) {
		card += getLength(valueslength,k);// TODO: this is begging for vectorization
	}
	return card;
}

/**
* the branchless approach is inspired by
*  Array layouts for comparison-based searching
*  http://arxiv.org/pdf/1509.05053.pdf
*/
// could potentially use SIMD-based bin. search
// values are interleaved with lengths
static int32_t interleavedBinarySearch(uint16_t* source, int32_t n, uint16_t target) {
	uint16_t * base = source;
    if(n == 0) return -1;
    while(n>1) {
    	int32_t half = n >> 1;
        base = (base[2*half] < target) ? &base[2*half] : base;
        n -= half;
    }
    // todo: over last cache line, you can just scan or use SIMD instructions
    base += (*base < target)*2;
    return *base == target ? (base - source)/2 : (source - base)/2 - 1;
}

/* Add `pos' to `run'. Returns true if `pos' was not present. */
bool run_container_add(run_container_t *run, uint16_t pos) {
	int32_t index = interleavedBinarySearch(run->valueslength, run->nbrruns, pos);
    if(index >= 0) return false;// already there
    index = - index - 2;// points to preceding value, possibly -1
    if(index >= 0) {// possible match
    	int32_t offset = pos - getValue(run->valueslength,index);
    	int32_t le =     getLength(run->valueslength,index);
        if(offset <= le) return false;// already there
        if(offset == le + 1) {
            // we may need to fuse
            if(index + 1 < run->nbrruns) {
                if(getValue(run->valueslength,index + 1)  == pos + 1) {
                    // indeed fusion is needed
                    setLength(run->valueslength,index, getValue(run->valueslength,index + 1) + getLength(run->valueslength,index + 1) - getValue(run->valueslength,index));
                    recoverRoomAtIndex(run,index + 1);
                    return true;
                }
            }
            incrementLength(run->valueslength,index);
            return true;
        }
        if(index + 1 < run->nbrruns) {
            // we may need to fuse
            if(getValue(run->valueslength,index + 1)  == pos + 1) {
                // indeed fusion is needed
                setValue(run->valueslength,index+1, pos);
                setLength(run->valueslength,index+1, getLength(run->valueslength,index + 1) + 1);
                return true;
            }
        }
    }
    if(index == -1) {
        // we may need to extend the first run
        if(0 < run->nbrruns) {
            if(getValue(run->valueslength,0)  == pos + 1) {
                incrementLength(run->valueslength,0);
                decrementValue(run->valueslength,0);
                return true;
            }
        }
    }
    makeRoomAtIndex(run,index + 1);
    setValue(run->valueslength,index + 1, pos);
    setLength(run->valueslength,index + 1, 0);
    return true;
}

/* Remove `pos' from `run'. Returns true if `pos' was present. */
bool run_container_remove(run_container_t *run, uint16_t pos) {
	int32_t index = interleavedBinarySearch(run->valueslength, run->nbrruns, pos);
        if(index >= 0) {
        	int32_t le =  getLength(run->valueslength,index);
            if(le == 0) {
                recoverRoomAtIndex(run,index);
            } else {
                incrementValue(run->valueslength,index);
                decrementLength(run->valueslength,index);
            }
            return true;
        }
        index = - index - 2;// points to preceding value, possibly -1
        if(index >= 0) {// possible match
        	int32_t offset = pos - getValue(run->valueslength,index);
        	int32_t le =     getLength(run->valueslength,index);
            if(offset < le) {
                // need to break in two
                setLength(run->valueslength,index, (offset - 1));
                // need to insert
                uint16_t newvalue = pos + 1;
                int32_t newlength = le - offset - 1;
                makeRoomAtIndex(run,index+1);
                setValue(run->valueslength,index+1,  newvalue);
                setLength(run->valueslength,index+1,  newlength);
                return true;

            } else if(offset == le) {
                decrementLength(run->valueslength,index);
                return true;
            }
        }
        // no match
        return false;
    }



/* Check whether `pos' is present in `run'.  */
bool run_container_contains(const run_container_t *run, uint16_t pos) {
	int32_t index = interleavedBinarySearch(run->valueslength, run->nbrruns, pos);
    if(index >= 0) return true;
    index = - index - 2; // points to preceding value, possibly -1
    if (index != -1)  {// possible match
    	int32_t offset = pos - getValue(run->valueslength,index);
    	int32_t le =     getLength(run->valueslength,index);
        if(offset <= le) return true;
    }
    return false;

}



/* Compute the union of `src_1' and `src_2' and write the result to `dst'
 * It is assumed that `dst' is distinct from both `src_1' and `src_2'. */
/*void run_container_union(const run_container_t *src_1,
                           const run_container_t *src_2,
                           run_container_t *dst);
*/
/* Compute the intersection of src_1 and src_2 and write the result to
 * dst. It is assumed that dst is distinct from both src_1 and src_2. */
/*void run_container_intersection(const run_container_t *src_1,
                                  const run_container_t *src_2,
                                  run_container_t *dst);

*/
