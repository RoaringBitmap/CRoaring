//
// This test.inc file is included by all the unit tests.
//
// It contains helpers for working with the cmocka unit tests.  Since that is
// a third party file, putting common macros and functions here is better
// than changing it.
//
// It is an `.inc` file instead of a `.h` file because it can contain static
// data declarations (e.g. for the random number generator state)
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
    //
    // It's generally not good to span a header file with `extern "C"`, but
    // this is how the cpp_unit.cpp test was doing it.  cmocka.h apparently
    // only has #ifdefs for extern "C" under MSC (?)
    //
    extern "C" {
        #include "vendor/cmocka/cmocka.h"
    }
#else
    #include "vendor/cmocka/cmocka.h"
#endif


#define DESCRIBE_TEST fprintf(stderr, "--- %s\n", __func__)


// The "cmocka" test functions are supposed to look like:
//
//      void test_function(void **state)
//
// Originally the C tests would declare it like:
//
//      void test_function()
//
// But in C++ that would not match the type, so the C++ tests declared as:
//
//      void test_function(void **)
//
// There's a problem if you're trying to write code that will compile in
// either C or C++, because it's not legal in C99 to not name a parameter...
// and if you give it a name, then there will be complaints that the paramter
// is not used.
//
// Disabling bad cast warnings in C++ defeats the point of compiling in C++,
// and knowing when parameters aren't referenced is useful even in tests.  So
// rather than disabling warnings, this defines a macro to declare the tests.
//
#ifdef __cplusplus
    #define DEFINE_TEST(name)   void name(void**)
#else
    #define DEFINE_TEST(name)   void name()
#endif


// Depending on a system-specific random number generator would lead to
// different outcomes for tests involving randomness, which makes for difficult
// debugging when a remote build encounters a failure that you don't get
// to happen locally.
//
static unsigned int seed = 123456789;
static const int OUR_RAND_MAX = (1 << 30) - 1;
inline static unsigned int our_rand() {
    seed = (1103515245 * seed + 12345);
    return seed & OUR_RAND_MAX;
}