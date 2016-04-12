#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "vendor/cmocka/cmocka.h"

#define DESCRIBE_TEST fprintf(stderr, "--- %s\n", __func__)
