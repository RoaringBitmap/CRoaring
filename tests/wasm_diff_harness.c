/*
 * Deterministic digest for WASM differential testing (native vs wasm32).
 * Fixed PRNG seed; no POSIX; only <roaring/roaring.h> when built via CMake —
 * amalgamation builds use #include "roaring.h" via -include or same directory.
 *
 * Seed: s0 = 0x853c49e6748fea9bULL (splitmix next)
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef CROARING_AMALGAMATED
#include "roaring.h"
#else
#include <roaring/roaring.h>
#endif

typedef struct {
    uint64_t h;
    uint64_t n;
} fold256_t;

static uint64_t rng_s0 = UINT64_C(0x853c49e6748fea9b);

/* murmur-like finalizer for split output */
static uint64_t rng_u64(void) {
    uint64_t z = (rng_s0 += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

static uint32_t rng_u32(void) { return (uint32_t)rng_u64(); }

static uint64_t rng_range_u32(uint32_t max_exclusive) {
    if (max_exclusive == 0) {
        return 0;
    }
    return (uint64_t)(rng_u32() % max_exclusive);
}

static void digest_u64(const char *key, uint64_t v) { printf("%s %" PRIu64 "\n", key, v); }

static bool iter_fold(uint32_t val, void *p) {
    fold256_t *f = (fold256_t *)p;
    f->h = f->h * UINT64_C(6364136223846793005) + (uint64_t)val;
    f->n++;
    return true;
}

static void fold_bitmap(const char *prefix, const roaring_bitmap_t *r) {
    fold256_t acc = {UINT64_C(0x243f6a8885a308d3), 0};
    (void)roaring_iterate(r, iter_fold, &acc);
    char kbuf[96];
    snprintf(kbuf, sizeof(kbuf), "%s_iter_fold", prefix);
    digest_u64(kbuf, acc.h);
    snprintf(kbuf, sizeof(kbuf), "%s_iter_count", prefix);
    digest_u64(kbuf, acc.n);
}

int main(void) {
    /* Sparse + array-heavy */
    roaring_bitmap_t *a = roaring_bitmap_create();
    for (int i = 0; i < 5000; i++) {
        uint32_t base = (uint32_t)(100000u + (rng_u32() % 2000000u));
        roaring_bitmap_add_range(a, base, base + 1 + (uint32_t)(rng_u32() % 120u));
    }

    /* Dense chunks -> bitset containers */
    roaring_bitmap_t *b = roaring_bitmap_create();
    for (uint32_t k = 0; k < 40; k++) {
        uint32_t base = k * 9973u + (uint32_t)rng_range_u32(1000u);
        roaring_bitmap_add_range(b, base, base + 20000u); /* ~bitset per chunk */
    }
    for (uint32_t hi = 3; hi < 12; hi++) {
        uint32_t seg = hi * 0x100000u;
        roaring_bitmap_add_range(b, seg + 10u, seg + 65000u);
    }

    /* Long run-friendly range */
    roaring_bitmap_t *c = roaring_bitmap_from_range(2400000, 2408000, 1);
    roaring_bitmap_t *d = roaring_bitmap_from_range(2410000, 2410900, 2);

    roaring_bitmap_t *u1 = roaring_bitmap_or(a, b);
    roaring_bitmap_t *x1 = roaring_bitmap_xor(c, d);
    roaring_bitmap_t *w = roaring_bitmap_or(u1, x1);

    roaring_bitmap_t *andab = roaring_bitmap_and(a, b);
    roaring_bitmap_t *orab = roaring_bitmap_or(a, b);
    roaring_bitmap_t *xorab = roaring_bitmap_xor(a, b);
    roaring_bitmap_t *anab = roaring_bitmap_andnot(a, b);
    roaring_bitmap_t *anba = roaring_bitmap_andnot(b, a);

    roaring_bitmap_or_inplace(w, orab);
    roaring_bitmap_and_inplace(w, roaring_bitmap_or(andab, xorab));
    roaring_bitmap_xor_inplace(w, anab);
    roaring_bitmap_andnot_inplace(w, anba);

    digest_u64("card_a", roaring_bitmap_get_cardinality(a));
    digest_u64("card_b", roaring_bitmap_get_cardinality(b));
    digest_u64("card_c", roaring_bitmap_get_cardinality(c));
    digest_u64("card_d", roaring_bitmap_get_cardinality(d));
    digest_u64("card_u1", roaring_bitmap_get_cardinality(u1));
    digest_u64("card_w", roaring_bitmap_get_cardinality(w));

    digest_u64("and_card_aa", roaring_bitmap_and_cardinality(a, a));
    digest_u64("or_card_ab", roaring_bitmap_or_cardinality(a, b));
    digest_u64("xor_card_ab", roaring_bitmap_xor_cardinality(a, b));
    digest_u64("andnot_card_ab", roaring_bitmap_andnot_cardinality(a, b));

    digest_u64("portable_sz_a", (uint64_t)roaring_bitmap_portable_size_in_bytes(a));
    digest_u64("portable_sz_w", (uint64_t)roaring_bitmap_portable_size_in_bytes(w));

    fold_bitmap("w", w);

    /* In-place twins for extra container traffic */
    roaring_bitmap_t *t = roaring_bitmap_copy(a);
    roaring_bitmap_and_inplace(t, b);
    digest_u64("card_inplace_and", roaring_bitmap_get_cardinality(t));

    roaring_bitmap_free(t);
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);
    roaring_bitmap_free(c);
    roaring_bitmap_free(d);
    roaring_bitmap_free(u1);
    roaring_bitmap_free(x1);
    roaring_bitmap_free(w);
    roaring_bitmap_free(andab);
    roaring_bitmap_free(orab);
    roaring_bitmap_free(xorab);
    roaring_bitmap_free(anab);
    roaring_bitmap_free(anba);
    return 0;
}
