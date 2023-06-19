//
// This test.h file is included by all the unit tests.
//
// It contains helpers for working with the cmocka unit tests.  Since that is
// a third party file, putting common macros and functions here is better
// than changing it.
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
        #include <cmocka.h>
    }
#else
    #include <cmocka.h>
#endif


#define DESCRIBE_TEST fprintf(stderr, "--- %s\n", __func__)

#define assert_bitmap_validate(b) do {                                       \
        const char *internal_reason_buf = NULL;                              \
        if (!roaring_bitmap_internal_validate((b), &internal_reason_buf)) {  \
            fail_msg("internal validation failed: %s", internal_reason_buf); \
        }                                                                    \
    } while (0)

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
    #define DEFINE_TEST(name)   static void name(void**)
#else
    #define DEFINE_TEST(name)   static void name()
#endif
