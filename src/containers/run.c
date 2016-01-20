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

extern void run_container_clear(run_container_t *run);
extern bool run_container_is_full(run_container_t *run);

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

// TODO: could be more efficient
static void smartAppend(run_container_t *run, uint16_t start, uint16_t length) {
        int32_t oldend;
        if((run->nbrruns==0) ||
                (start >
                (oldend = getValue(run->valueslength, run->nbrruns - 1)
                		+ getLength(run->valueslength, run->nbrruns - 1)) + 1)) { // we add a new one
        	run->valueslength[2 * run->nbrruns] =  start;
        	run->valueslength[2 * run->nbrruns + 1] = length;
        	run->nbrruns++;
            return;
        }
        int32_t newend = start + length + 1;
        if(newend > oldend)  { // we merge
            setLength(run->valueslength, run->nbrruns - 1,  (newend - 1 - getValue(run->valueslength, run->nbrruns - 1)));
        }
}


static void increaseCapacity(run_container_t *run, int32_t min, bool copy) {
    int32_t newCapacity = (run->capacity == 0) ? DEFAULT_INIT_SIZE :
    		run->capacity < 64 ? run->capacity * 2
            : run->capacity < 1024 ? run->capacity * 3 / 2
            : run->capacity * 5 / 4;
    if(newCapacity < min) newCapacity = min;
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
        if (run->nbrruns+1 > run->capacity) increaseCapacity(run, run->nbrruns+1, true);
        memmove(run->valueslength+(1+index)*2,run->valueslength+2*index,(run->nbrruns - index) * 2 * sizeof(uint16_t));
        run->nbrruns++;
}

static inline void recoverRoomAtIndex(run_container_t *run,uint16_t index) {
    memmove(run->valueslength+2*index,run->valueslength+(1+index)*2,(run->nbrruns - index - 1) * 2 * sizeof(uint16_t));
	run->nbrruns--;
}

/* copy one container into another */
void run_container_copy(run_container_t *source, run_container_t *dest) {
	if(source->nbrruns < dest->capacity) {
		increaseCapacity(dest,source->nbrruns,false);
	}
	dest->nbrruns = source->nbrruns;
	memcpy(dest->valueslength,source->valueslength,2*sizeof(uint16_t)*source->nbrruns);
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
    if(target > source[ 2 * n - 2 ]) return -n - 1; // without this, buffer overrun
    while(n>1) {
    	int32_t half = n >> 1;
        base = (base[2*half] < target) ? base + 2*half : base;
        n -= half;
    }
    base += (*base < target)*2;
    return  ( *base == target) ? (base - source)/2 : (source - base)/2 - 1;
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
void run_container_union(run_container_t *src_1,
                           run_container_t *src_2,
                           run_container_t *dst) {
	// TODO: this could be a lot more efficient

	// we start out with inexpensive checks
	const bool if1 = run_container_is_full(src_1);
	const bool if2 = run_container_is_full(src_2);
	if (if1 || if2) {
		if (if1) {
			run_container_copy(src_2, dst);
			return;
		}
		if (if2) {
			run_container_copy(src_1, dst);
			return;
		}
	}
	const int32_t neededcapacity = src_1->nbrruns + src_2->nbrruns;
	if(dst->capacity < neededcapacity)
		increaseCapacity(dst, neededcapacity,false);
	dst->nbrruns = 0;
	int32_t rlepos = 0;
    int32_t xrlepos = 0;

    while ((xrlepos < src_2->nbrruns) && (rlepos < src_1->nbrruns)) {
        if(getValue(src_1->valueslength,rlepos) <= getValue(src_2->valueslength,xrlepos) ) {
            smartAppend(dst,getValue(src_1->valueslength,rlepos), getLength(src_1->valueslength,rlepos));
            rlepos++;
        } else {
            smartAppend(dst,getValue(src_2->valueslength,xrlepos), getLength(src_2->valueslength,xrlepos));
            xrlepos++;
        }
    }
    while (xrlepos < src_2->nbrruns) {
        smartAppend(dst,getValue(src_2->valueslength,xrlepos), getLength(src_2->valueslength,xrlepos));
        xrlepos++;
    }
    while (rlepos < src_1->nbrruns) {
        smartAppend(dst,getValue(src_1->valueslength,rlepos), getLength(src_1->valueslength,rlepos));
        rlepos++;
    }

}

/* Compute the intersection of src_1 and src_2 and write the result to
 * dst. It is assumed that dst is distinct from both src_1 and src_2. */
void run_container_intersection(run_container_t *src_1,
                                  run_container_t *src_2,
                                  run_container_t *dst) {
	// TODO: this could be a lot more efficient, could use SIMD optimizations
	const int32_t neededcapacity = src_1->nbrruns + src_2->nbrruns;
	if(dst->capacity < neededcapacity)
		increaseCapacity(dst, neededcapacity,false);
	dst->nbrruns = 0;
    int32_t rlepos = 0;
    int32_t xrlepos = 0;
    int32_t start = getValue(src_1->valueslength,rlepos);
    int32_t end = start + getLength(src_1->valueslength,rlepos) + 1;
    int32_t xstart = getValue(src_2->valueslength,xrlepos);
    int32_t xend = xstart + getLength(src_2->valueslength,xrlepos) + 1;
    while ((rlepos < src_1->nbrruns ) && (xrlepos < src_2->nbrruns )) {
        if (end  <= xstart) {
            ++rlepos;
            if(rlepos < src_1->nbrruns ) {
                start = getValue(src_1->valueslength,rlepos);
                end = start + getLength(src_1->valueslength,rlepos) + 1;
            }
        } else if (xend <= start) {
            ++xrlepos;
            if(xrlepos < src_2->nbrruns ) {
                xstart = getValue(src_2->valueslength,xrlepos);
                xend = xstart + getLength(src_2->valueslength,xrlepos) + 1;
            }
        } else {// they overlap
            const int32_t lateststart = start > xstart ? start : xstart;
            int32_t earliestend;
            if(end == xend) {// improbable
                earliestend = end;
                rlepos++;
                xrlepos++;
                if(rlepos < src_1->nbrruns ) {
                    start = getValue(src_1->valueslength,rlepos);
                    end = start + getLength(src_1->valueslength,rlepos) + 1;
                }
                if(xrlepos < src_2->nbrruns) {
                    xstart = getValue(src_2->valueslength,xrlepos);
                    xend = xstart + getLength(src_2->valueslength,xrlepos) + 1;
                }
            } else if(end < xend) {
                earliestend = end;
                rlepos++;
                if(rlepos < src_1->nbrruns ) {
                    start = getValue(src_1->valueslength,rlepos);
                    end = start + getLength(src_1->valueslength,rlepos) + 1;
                }

            } else {// end > xend
                earliestend = xend;
                xrlepos++;
                if(xrlepos < src_2->nbrruns) {
                    xstart = getValue(src_2->valueslength,xrlepos);
                    xend = xstart + getLength(src_2->valueslength,xrlepos) + 1;
                }
            }
            dst->valueslength[2 * dst->nbrruns] = lateststart;
            dst->valueslength[2 * dst->nbrruns + 1] = (earliestend - lateststart - 1);
            dst->nbrruns++;
        }
    }

}

void run_container_to_uint32_array( uint32_t *out, const run_container_t *cont, uint32_t base) {
  int outpos = 0;
  for (int i = 0; i < cont->nbrruns; ++i) {
    uint32_t run_start = base + cont->valueslength[2 * i];
    for (int j = 0; j <= cont->valueslength[2 * i + 1]; ++j)
      out[outpos++] = run_start + j;
  }
}
