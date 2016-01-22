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

extern bool run_container_is_full(run_container_t *run);
extern bool  run_container_nonzero_cardinality(run_container_t *r);
extern void run_container_clear(run_container_t *run) ;

enum{DEFAULT_INIT_SIZE = 4};


// TODO: could be more efficient
static void smartAppend(run_container_t *run, uint16_t start, uint16_t length) {
        int32_t oldend;
        // todo: next line is probably unsafe when nbrruns == 0 in the sense where we might access memory out of bounds (crash prone?)
        if((run->nbrruns==0) ||
                (start >
                (oldend = run->valueslength[run->nbrruns - 1].value
                		+ run->valueslength[run->nbrruns - 1].length) + 1)) { // we add a new one
        	run->valueslength[run->nbrruns].value =  start;
        	run->valueslength[run->nbrruns].length = length;
        	run->nbrruns++;
            return;
        }
        int32_t newend = start + length + 1;
        if(newend > oldend)  { // we merge
        	run->valueslength[run->nbrruns - 1].length =   newend - 1 - run->valueslength[run->nbrruns - 1].value;
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
    	run->valueslength = realloc(run->valueslength,run->capacity * sizeof(valuelength_t)) ;
    else {
      free(run->valueslength);
      run->valueslength = malloc(run->capacity * sizeof(valuelength_t)) ;
    }
    // TODO: handle the case where realloc fails
    if(run->valueslength == NULL) {
    	printf("Well, that's unfortunate. Did I say you could use this code in production?\n");
    }


}
static inline void makeRoomAtIndex( run_container_t *run,uint16_t index) {
        if (run->nbrruns+1 > run->capacity) increaseCapacity(run, run->nbrruns+1, true);
        memmove(run->valueslength+1+index,run->valueslength+index,(run->nbrruns - index) * sizeof(valuelength_t));
        run->nbrruns++;
}

static inline void recoverRoomAtIndex(run_container_t *run,uint16_t index) {
    memmove(run->valueslength+index,run->valueslength+(1+index),(run->nbrruns - index - 1) *  sizeof(valuelength_t));
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
	if ((run->valueslength = malloc(sizeof(valuelength_t) * DEFAULT_INIT_SIZE)) == NULL) {
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
	run->valueslength = NULL;// pedantic
	free(run);
}

/* Get the cardinality of `run'. Requires an actual computation. */
int run_container_cardinality(const run_container_t *run) {
	int card = run->nbrruns;
	valuelength_t *valueslength = run->valueslength;
	for(int k = 0; k < run->nbrruns; ++k) {
		card += valueslength[k].length;// TODO: this is begging for vectorization
	}
	return card;
}


// TODO: replace by a branchy bin search
/**
* the branchless approach is inspired by
*  Array layouts for comparison-based searching
*  http://arxiv.org/pdf/1509.05053.pdf
*/
// could potentially use SIMD-based bin. search
// values are interleaved with lengths
static int32_t interleavedBinarySearch(const valuelength_t* source, int32_t n, uint16_t target) {
    const valuelength_t * base = source;
    if(n == 0) return -1;
    if(target > source[n - 1].value) return -n - 1; // without this, buffer overrun
    while(n>1) {
    	int32_t half = n >> 1;
        base = (base[half].value < target) ? base + half : base;
        n -= half;
    }
    base += (base->value < target);
    return  ( base->value == target) ? (base - source) : (source - base) - 1;
}

/* Add `pos' to `run'. Returns true if `pos' was not present. */
bool run_container_add(run_container_t *run, uint16_t pos) {
	int32_t index = interleavedBinarySearch(run->valueslength, run->nbrruns, pos);
    if(index >= 0) return false;// already there
    index = - index - 2;// points to preceding value, possibly -1
    if(index >= 0) {// possible match
    	int32_t offset = pos - run->valueslength[index].value;
    	int32_t le =     run->valueslength[index].length;
        if(offset <= le) return false;// already there
        if(offset == le + 1) {
            // we may need to fuse
            if(index + 1 < run->nbrruns) {
                if(run->valueslength[index + 1].value  == pos + 1) {
                    // indeed fusion is needed
                    run->valueslength[index].length = run->valueslength[index + 1].value + run->valueslength[index + 1].length - run->valueslength[index].value;
                    recoverRoomAtIndex(run,index + 1);
                    return true;
                }
            }
            run->valueslength[index].length++;
            return true;
        }
        if(index + 1 < run->nbrruns) {
            // we may need to fuse
            if(run->valueslength[index + 1].value == pos + 1) {
                // indeed fusion is needed
                run->valueslength[index+1].value =  pos;
                run->valueslength[index+1].length = run->valueslength[index + 1].length + 1;
                return true;
            }
        }
    }
    if(index == -1) {
        // we may need to extend the first run
        if(0 < run->nbrruns) {
            if(run->valueslength[0].value  == pos + 1) {
                run->valueslength[0].length++;
                run->valueslength[0].value--;
                return true;
            }
        }
    }
    makeRoomAtIndex(run,index + 1);
    run->valueslength[index + 1].value = pos;
    run->valueslength[index + 1].length = 0;
    return true;
}

/* Remove `pos' from `run'. Returns true if `pos' was present. */
bool run_container_remove(run_container_t *run, uint16_t pos) {
	int32_t index = interleavedBinarySearch(run->valueslength, run->nbrruns, pos);
        if(index >= 0) {
        	int32_t le =  run->valueslength[index].length;
            if(le == 0) {
                recoverRoomAtIndex(run,index);
            } else {
                run->valueslength[index].value++;
                run->valueslength[index].length--;
            }
            return true;
        }
        index = - index - 2;// points to preceding value, possibly -1
        if(index >= 0) {// possible match
        	int32_t offset = pos - run->valueslength[index].value;
        	int32_t le =     run->valueslength[index].length;
            if(offset < le) {
                // need to break in two
                run->valueslength[index].length =  offset - 1;
                // need to insert
                uint16_t newvalue = pos + 1;
                int32_t newlength = le - offset - 1;
                makeRoomAtIndex(run,index+1);
                run->valueslength[index+1].value = newvalue;
                run->valueslength[index+1].length = newlength;
                return true;

            } else if(offset == le) {
                run->valueslength[index].length--;
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
    	int32_t offset = pos - run->valueslength[index].value;
    	int32_t le =     run->valueslength[index].length;
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
        if(src_1->valueslength[rlepos].value <= src_2->valueslength[xrlepos].value ) {
            smartAppend(dst,src_1->valueslength[rlepos].value, src_1->valueslength[rlepos].length);
            rlepos++;
        } else {
            smartAppend(dst,src_2->valueslength[xrlepos].value, src_2->valueslength[xrlepos].length);
            xrlepos++;
        }
    }
    while (xrlepos < src_2->nbrruns) {
        smartAppend(dst,src_2->valueslength[xrlepos].value, src_2->valueslength[xrlepos].length);
        xrlepos++;
    }
    while (rlepos < src_1->nbrruns) {
        smartAppend(dst,src_1->valueslength[rlepos].value, src_1->valueslength[rlepos].length);
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
    int32_t start = src_1->valueslength[rlepos].value;
    int32_t end = start + src_1->valueslength[rlepos].length + 1;
    int32_t xstart = src_2->valueslength[xrlepos].value;
    int32_t xend = xstart + src_2->valueslength[xrlepos].length + 1;
    while ((rlepos < src_1->nbrruns ) && (xrlepos < src_2->nbrruns )) {
        if (end  <= xstart) {
            ++rlepos;
            if(rlepos < src_1->nbrruns ) {
                start = src_1->valueslength[rlepos].value;
                end = start + src_1->valueslength[rlepos].length + 1;
            }
        } else if (xend <= start) {
            ++xrlepos;
            if(xrlepos < src_2->nbrruns ) {
                xstart = src_2->valueslength[xrlepos].value;
                xend = xstart + src_2->valueslength[xrlepos].length + 1;
            }
        } else {// they overlap
            const int32_t lateststart = start > xstart ? start : xstart;
            int32_t earliestend;
            if(end == xend) {// improbable
                earliestend = end;
                rlepos++;
                xrlepos++;
                if(rlepos < src_1->nbrruns ) {
                    start = src_1->valueslength[rlepos].value;
                    end = start + src_1->valueslength[rlepos].length + 1;
                }
                if(xrlepos < src_2->nbrruns) {
                    xstart = src_2->valueslength[xrlepos].value;
                    xend = xstart + src_2->valueslength[xrlepos].length + 1;
                }
            } else if(end < xend) {
                earliestend = end;
                rlepos++;
                if(rlepos < src_1->nbrruns ) {
                    start = src_1->valueslength[rlepos].value;
                    end = start + src_1->valueslength[rlepos].length + 1;
                }

            } else {// end > xend
                earliestend = xend;
                xrlepos++;
                if(xrlepos < src_2->nbrruns) {
                    xstart = src_2->valueslength[xrlepos].value;
                    xend = xstart + src_2->valueslength[xrlepos].length + 1;
                }
            }
            dst->valueslength[dst->nbrruns].value = lateststart;
            dst->valueslength[dst->nbrruns].length = (earliestend - lateststart - 1);
            dst->nbrruns++;
        }
    }

}

void run_container_to_uint32_array( uint32_t *out, const run_container_t *cont, uint32_t base) {
  int outpos = 0;
  for (int i = 0; i < cont->nbrruns; ++i) {
    uint32_t run_start = base + cont->valueslength[i].value;
    uint16_t le = cont->valueslength[i].length;
    for (int j = 0; j <= le ; ++j)
      out[outpos++] = run_start + j;
  }
}
