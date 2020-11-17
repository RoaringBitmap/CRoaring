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
// http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.43.3639
// http://www.iro.umontreal.ca/~simardr/rng/lfsr113.c
//
#define OUR_RAND_MAX UINT32_MAX
uint32_t our_rand(void)  // lfsr113_Bits()
{
   static uint32_t z1 = 12345, z2 = 12345, z3 = 12345, z4 = 12345;
   uint32_t b;
   b  = ((z1 << 6) ^ z1) >> 13;
   z1 = ((z1 & 4294967294U) << 18) ^ b;
   b  = ((z2 << 2) ^ z2) >> 27; 
   z2 = ((z2 & 4294967288U) << 2) ^ b;
   b  = ((z3 << 13) ^ z3) >> 21;
   z3 = ((z3 & 4294967280U) << 7) ^ b;
   b  = ((z4 << 3) ^ z4) >> 12;
   z4 = ((z4 & 4294967168U) << 13) ^ b;
   return (z1 ^ z2 ^ z3 ^ z4);
}
