#ifndef PERFPARAMETERS_H_
#define PERFPARAMETERS_H_

/**
During lazy computations, we can transform array containers into bitset
containers as
long as we can expect them to have  ARRAY_LAZY_LOWERBOUND values.

When doing "run optimize", instead of converting to a run container when it
would
be smaller, we require that it be RUN_OPTI_MINIMAL_GAIN times smaller. That's
because
huge run containers are implemented less efficiently.

*/
enum { ARRAY_LAZY_LOWERBOUND = 1024, RUN_OPTI_MINIMAL_GAIN = 8 };
#endif
