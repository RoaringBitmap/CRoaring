#ifndef OPTIONS_H
#define OPTIONS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <roaring/memory.h>
#include <string.h>

struct roaring_options_s {
    roaring_memory_t* memory;
};

typedef struct roaring_options_s roaring_options_t;

#ifdef __cplusplus
}
#endif

#endif