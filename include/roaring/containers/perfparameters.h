#ifndef ROARING_PERFPARAMETERS_H_
#define ROARING_PERFPARAMETERS_H_

#include <stdbool.h>

/**
During lazy computations, we can transform array containers into bitset
containers as
long as we can expect them to have  ARRAY_LAZY_LOWERBOUND values.
*/
enum { ROARING_ARRAY_LAZY_LOWERBOUND = 1024 };

/* default initial size of a run container */
enum { ROARING_RUN_DEFAULT_INIT_SIZE = 4 };

/* default initial size of an array container */
enum { ROARING_ARRAY_DEFAULT_INIT_SIZE = 16 };

/* automatic bitset conversion during lazy or */
#ifndef ROARING_LAZY_OR_BITSET_CONVERSION
#define ROARING_LAZY_OR_BITSET_CONVERSION true
#endif

#endif
