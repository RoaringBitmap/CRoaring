/*
 * portability.c
 *
 * This file contains definitions for non-inlinable contents of portability.h
 *
 * At time of writing, that's the function pointer values for hooking the
 * debug build's malloc()/realloc()/free() at runtime.
 */

#include "portability.h"

#if !defined NDEBUG

static alloc_hook_t *alloc_hook;
static realloc_hook_t *realloc_hook;
static free_hook_t *free_hook;
static aligned_alloc_hook_t *aligned_alloc_hook;
static free_hook_t *aligned_free_hook;

// Hooked versions (call default versions if hook is NULL)

void* roaring_debug_alloc(size_t size)
{
   if (alloc_hook != NULL)
       return alloc_hook(size);
   return malloc(size);
}

void* roaring_debug_realloc(void *p, size_t size)
{
    if (realloc_hook != NULL)
       return realloc_hook(p, size);
    return realloc(p, size);
}

void roaring_debug_free(void *p)
{
    if (free_hook != NULL)
        free_hook(p);
    else
        free(p);
}

void* roaring_debug_aligned_alloc(size_t alignment, size_t size)
{
    if (aligned_alloc_hook != NULL)
       return aligned_alloc_hook(alignment, size);
    return roaring_bitmap_aligned_malloc(alignment, size);
}

void roaring_debug_aligned_free(void *p)
{
    if (aligned_free_hook != NULL)
        aligned_free_hook(p);
    else
        roaring_bitmap_aligned_free(p);
}


// Functions for setting the hook function pointers

alloc_hook_t *roaring_debug_set_alloc_hook(alloc_hook_t *hook)
{
    alloc_hook_t *old = alloc_hook ? alloc_hook : &malloc;
    alloc_hook = hook;
    return old;
}

realloc_hook_t *roaring_debug_set_realloc_hook(realloc_hook_t *hook)
{
    realloc_hook_t *old = realloc_hook ? realloc_hook : &realloc;
    realloc_hook = hook;
    return old;
}

free_hook_t *roaring_debug_set_free_hook(free_hook_t *hook)
{
    free_hook_t *old = free_hook ? free_hook : &free;
    free_hook = hook;
    return old;
}

aligned_alloc_hook_t *roaring_debug_set_aligned_alloc_hook(aligned_alloc_hook_t *hook)
{
    aligned_alloc_hook_t *old = aligned_alloc_hook
            ? aligned_alloc_hook
            : &roaring_bitmap_aligned_malloc;
    aligned_alloc_hook = hook;
    return old;
}

free_hook_t *roaring_debug_set_aligned_free_hook(free_hook_t *hook)
{
    free_hook_t *old = aligned_free_hook
            ? aligned_free_hook
            : &roaring_bitmap_aligned_free;
    aligned_free_hook = hook;
    return old;
}

#endif
