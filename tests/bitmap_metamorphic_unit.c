/*
 * Metamorphic "phantom member" defenses for roaring_bitmap_t bitwise ops:
 * exports and iterators respect set-theoretic oracles derived only from
 * operands (covers wrong-but-non-crashing SIMD/scalar divergence, not only deep
 * OR folds).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <roaring/roaring.h>

#include "test.h"

typedef struct splitmix_rng {
    uint64_t state;
} splitmix_rng_t;

static void splitmix_seed(splitmix_rng_t *rng, uint64_t seed) {
    rng->state = seed;
}

static uint64_t splitmix_next(splitmix_rng_t *rng) {
    uint64_t z = (rng->state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

static uint32_t rng_u32(splitmix_rng_t *rng) {
    return (uint32_t)splitmix_next(rng);
}

static uint32_t rng_below(splitmix_rng_t *rng, uint32_t exclusive_max) {
    if (exclusive_max == 0) {
        return 0;
    }
    return (uint32_t)((uint64_t)splitmix_next(rng) % (uint64_t)exclusive_max);
}

/** Fills bitmap; bumps *glob_operand_max to highest value inserted anywhere. */
static void bounded_rand_fill(roaring_bitmap_t *bmp, uint32_t doc_cap,
                              uint32_t pat, splitmix_rng_t *rng,
                              uint32_t *glob_operand_max) {
    uint32_t local_max = 0;

    if ((pat % 5U) == 0U) {
        const int reps = (int)(12U + rng_below(rng, 220));
        for (int jr = 0; jr < reps; jr++) {
            uint32_t v = rng_below(rng, doc_cap);
            roaring_bitmap_add(bmp, v);
            if (v > local_max) {
                local_max = v;
            }
        }
    } else if ((pat % 5U) == 1U) {
        uint32_t run_len = 3U + rng_below(rng, 650);
        if (run_len >= doc_cap) {
            run_len = 10U;
        }
        uint32_t base =
            doc_cap > run_len ? rng_below(rng, doc_cap - run_len + 1U) : 0U;
        roaring_bitmap_add_range(bmp, base, base + run_len);
        local_max = base + run_len - 1U;
    } else if ((pat % 5U) == 2U) {
        uint32_t span = 900U + rng_below(rng, 3500);
        if (span >= doc_cap) {
            span = 100U;
        }
        uint32_t base =
            doc_cap > span ? rng_below(rng, doc_cap - span + 1U) : 0U;
        roaring_bitmap_add_range(bmp, base, base + span);
        local_max = base + span - 1U;
    } else if ((pat % 5U) == 3U) {
        roaring_bitmap_add_range(bmp, 0U, 1U);
        local_max = 0U;
        const int blobs = (int)(2U + rng_below(rng, 6));
        for (int jb = 0; jb < blobs; jb++) {
            uint32_t blen = 40U + rng_below(rng, 800);
            if (blen >= doc_cap) {
                blen = 41U;
            }
            uint32_t bbase =
                doc_cap > blen ? rng_below(rng, doc_cap - blen + 1U) : 0U;
            roaring_bitmap_add_range(bmp, bbase, bbase + blen);
            uint32_t hi = bbase + blen - 1U;
            if (hi > local_max) {
                local_max = hi;
            }
        }
    } else {
        uint32_t span = 3200U + rng_below(rng, 39800);
        if (span >= doc_cap) {
            span = doc_cap - 1U;
        }
        uint32_t base_cap = doc_cap - span + 1U;
        if (base_cap == 0U) {
            base_cap = 1U;
        }
        uint32_t base = rng_below(rng, base_cap);
        roaring_bitmap_add_range(bmp, base, base + span);
        local_max = base + span - 1U;
    }

    if (local_max > *glob_operand_max) {
        *glob_operand_max = local_max;
    }
}

static void roaring_fill_bounded(roaring_bitmap_t *bmp, splitmix_rng_t *rng,
                                 uint32_t doc_cap, int n_patterns,
                                 uint32_t *glob_operand_max) {
    for (int p = 0; p < n_patterns; p++) {
        bounded_rand_fill(bmp, doc_cap, rng_u32(rng), rng, glob_operand_max);
    }
    if (roaring_bitmap_is_empty(bmp)) {
        uint32_t v = rng_below(rng, doc_cap);
        roaring_bitmap_add(bmp, v);
        if (v > *glob_operand_max) {
            *glob_operand_max = v;
        }
    }
}

typedef struct pair_ctx {
    roaring_bitmap_t *a;
    roaring_bitmap_t *b;
} pair_ctx;

static bool pred_or(uint32_t x, void *v) {
    pair_ctx *p = (pair_ctx *)v;
    return roaring_bitmap_contains(p->a, x) || roaring_bitmap_contains(p->b, x);
}

static bool pred_and(uint32_t x, void *v) {
    pair_ctx *p = (pair_ctx *)v;
    return roaring_bitmap_contains(p->a, x) && roaring_bitmap_contains(p->b, x);
}

static bool pred_xor(uint32_t x, void *v) {
    pair_ctx *p = (pair_ctx *)v;
    return roaring_bitmap_contains(p->a, x) != roaring_bitmap_contains(p->b, x);
}

static bool pred_andnot(uint32_t x, void *v) {
    pair_ctx *p = (pair_ctx *)v;
    return roaring_bitmap_contains(p->a, x) &&
           !roaring_bitmap_contains(p->b, x);
}

typedef struct operand_ceiling_ctx {
    uint32_t cap;
    uint32_t opmax;
} operand_ceiling_ctx;

static bool pred_operand_ceiling(uint32_t x, void *v) {
    operand_ceiling_ctx *p = (operand_ceiling_ctx *)v;
    return (x < p->cap) && (x <= p->opmax);
}

typedef struct export_iter_aux {
    const uint32_t *buf;
    uint64_t n;
    uint64_t idx;
} export_iter_aux;

static bool iter_equals_export_cb(uint32_t x, void *raw) {
    export_iter_aux *aux = (export_iter_aux *)raw;
    assert_true(aux->idx < aux->n);
    assert_true((uint64_t)x == (uint64_t)aux->buf[aux->idx]);
    aux->idx++;
    return true;
}

/** Every exported value obeys x < doc_cap and ok(x); iterator order matches
 * export. */
static void assert_membership_via_export_iterate(
    roaring_bitmap_t *rb, uint32_t doc_cap, bool (*ok)(uint32_t x, void *ctx),
    void *ctx) {
    const uint64_t n = roaring_bitmap_get_cardinality(rb);
    assert_true(n <= (uint64_t)SIZE_MAX / sizeof(uint32_t));
    if (n == 0U) {
        return;
    }

    uint32_t *tmp = malloc((size_t)n * sizeof(uint32_t));
    assert_non_null(tmp);
    roaring_bitmap_to_uint32_array(rb, tmp);
    export_iter_aux aux = {tmp, n, UINT64_C(0)};
    (void)roaring_iterate(rb, iter_equals_export_cb, &aux);

    uint64_t vmax = UINT64_C(0);
    for (uint64_t i = 0; i < n; i++) {
        const uint64_t vx = tmp[i];
        assert_true(vx < (uint64_t)doc_cap);
        assert_true(ok(tmp[i], ctx));
        if (vx > vmax) {
            vmax = vx;
        }
    }
    assert_true(roaring_bitmap_maximum(rb) == (uint32_t)vmax);

    assert_true(aux.idx == n);
    free(tmp);
}

#define OR_WS_MAX_LEAVES 384

typedef struct bounded_leaf_workspace {
    roaring_bitmap_t *buf[OR_WS_MAX_LEAVES];
    int n;
} bounded_leaf_workspace_t;

static void workspace_clear(bounded_leaf_workspace_t *ws) {
    for (int i = 0; i < ws->n; i++) {
        if (ws->buf[i] != NULL) {
            roaring_bitmap_free(ws->buf[i]);
            ws->buf[i] = NULL;
        }
    }
    ws->n = 0;
}

static bool workspace_build_leaves_bounded(bounded_leaf_workspace_t *ws,
                                           uint64_t rng_seed, uint32_t doc_cap,
                                           int n_leaves,
                                           uint32_t *operand_max_seen_out) {
    if (n_leaves < 2 || n_leaves > OR_WS_MAX_LEAVES) {
        return false;
    }

    workspace_clear(ws);

    splitmix_rng_t rng_val;
    splitmix_seed(&rng_val, rng_seed);

    uint32_t operand_max = 0;

    for (int li = 0; li < n_leaves; li++) {
        roaring_bitmap_t *leaf = roaring_bitmap_create();
        if (leaf == NULL) {
            workspace_clear(ws);
            return false;
        }
        ws->buf[ws->n++] = leaf;

        bounded_rand_fill(leaf, doc_cap, rng_u32(&rng_val), &rng_val,
                          &operand_max);

        if (roaring_bitmap_is_empty(leaf)) {
            uint32_t v = rng_below(&rng_val, doc_cap);
            roaring_bitmap_add(leaf, v);
            if (v > operand_max) {
                operand_max = v;
            }
        }
        assert_bitmap_validate(leaf);
    }

    *operand_max_seen_out = operand_max;
    return true;
}

static roaring_bitmap_t *or_fold_balanced(const roaring_bitmap_t **leaf_ptrs,
                                          int lo, int hi) {
    if (lo > hi) {
        return NULL;
    }
    if (lo == hi) {
        return roaring_bitmap_copy(leaf_ptrs[lo]);
    }
    const int mid = lo + ((hi - lo) / 2);
    roaring_bitmap_t *left = or_fold_balanced(leaf_ptrs, lo, mid);
    roaring_bitmap_t *right = or_fold_balanced(leaf_ptrs, mid + 1, hi);
    if (left == NULL || right == NULL) {
        roaring_bitmap_free(left);
        roaring_bitmap_free(right);
        return NULL;
    }
    roaring_bitmap_or_inplace(left, right);
    roaring_bitmap_free(right);
    return left;
}

static roaring_bitmap_t *or_fold_left_linear(const roaring_bitmap_t **leaf_ptrs,
                                             int n) {
    roaring_bitmap_t *acc = roaring_bitmap_copy(leaf_ptrs[0]);
    if (acc == NULL) {
        return NULL;
    }
    for (int i = 1; i < n; i++) {
        roaring_bitmap_or_inplace(acc, leaf_ptrs[i]);
    }
    return acc;
}

static roaring_bitmap_t *or_fold_right_linear(
    const roaring_bitmap_t **leaf_ptrs, int n) {
    roaring_bitmap_t *acc = roaring_bitmap_copy(leaf_ptrs[n - 1]);
    if (acc == NULL) {
        return NULL;
    }
    for (int i = n - 2; i >= 0; i--) {
        roaring_bitmap_or_inplace(acc, leaf_ptrs[i]);
    }
    return acc;
}

static void assert_operand_ceiling(roaring_bitmap_t *rb, uint32_t doc_cap,
                                   uint32_t operand_max_seen) {
    operand_ceiling_ctx ctx = {doc_cap, operand_max_seen};
    assert_membership_via_export_iterate(rb, doc_cap, pred_operand_ceiling,
                                         &ctx);
}

static void run_deep_or_fold_case(uint64_t rng_seed, uint32_t doc_cap,
                                  int n_leaves) {
    bounded_leaf_workspace_t ws;
    ws.n = 0;
    uint32_t operand_max_seen = 0;

    assert_true(workspace_build_leaves_bounded(&ws, rng_seed, doc_cap, n_leaves,
                                               &operand_max_seen));

    const roaring_bitmap_t *ptrs[OR_WS_MAX_LEAVES];
    for (int i = 0; i < ws.n; i++) {
        ptrs[i] = ws.buf[i];
    }

    roaring_bitmap_t *left = or_fold_left_linear(ptrs, ws.n);
    roaring_bitmap_t *balanced = or_fold_balanced(ptrs, 0, ws.n - 1);
    roaring_bitmap_t *right = or_fold_right_linear(ptrs, ws.n);

    assert_non_null(left);
    assert_non_null(balanced);
    assert_non_null(right);

    assert_true(roaring_bitmap_equals(left, balanced));
    assert_true(roaring_bitmap_equals(left, right));

    roaring_bitmap_t *heap_union =
        roaring_bitmap_or_many_heap((uint32_t)ws.n, ptrs);
    roaring_bitmap_t *naive_union = roaring_bitmap_or_many((size_t)ws.n, ptrs);
    assert_non_null(heap_union);
    assert_non_null(naive_union);
    assert_true(roaring_bitmap_equals(left, heap_union));
    assert_true(roaring_bitmap_equals(left, naive_union));

    assert_bitmap_validate(left);
    assert_operand_ceiling(left, doc_cap, operand_max_seen);

    roaring_bitmap_free(heap_union);
    roaring_bitmap_free(naive_union);
    roaring_bitmap_free(left);
    roaring_bitmap_free(balanced);
    roaring_bitmap_free(right);
    workspace_clear(&ws);
}

DEFINE_TEST(shallow_deep_or_fold) {
    DESCRIBE_TEST;
    run_deep_or_fold_case(UINT64_C(0x40d9c2f5a1b34e22), 12000U, 48);
    run_deep_or_fold_case(UINT64_C(0x94f3a2c1b8d5e711), 50000U, 285);
}

DEFINE_TEST(bitwise_membership_and_cardinality_metamorphism) {
    DESCRIBE_TEST;
    splitmix_rng_t rng_a, rng_b;
    pair_ctx pc;

    for (int round = 0; round < 24; round++) {
        const uint64_t rs = UINT64_C(0x7c11f00ddead0000) + (uint64_t)round;

        roaring_bitmap_t *a = roaring_bitmap_create();
        roaring_bitmap_t *b = roaring_bitmap_create();
        assert_non_null(a);
        assert_non_null(b);

        uint32_t gmax_a = 0, gmax_b = 0;
        splitmix_seed(&rng_a, rs ^ UINT64_C(0xaaaaaaaaaaaaaaaa));
        splitmix_seed(&rng_b, rs ^ UINT64_C(0xbbbbbbbbbbbbbbbb));
        roaring_fill_bounded(a, &rng_a, 18000U, 3 + (round % 5), &gmax_a);
        roaring_fill_bounded(b, &rng_b, 18000U, 4 + (round % 4), &gmax_b);

        pc.a = a;
        pc.b = b;

        roaring_bitmap_t *out_or = roaring_bitmap_or(a, b);
        roaring_bitmap_t *out_and = roaring_bitmap_and(a, b);
        roaring_bitmap_t *out_xor = roaring_bitmap_xor(a, b);
        roaring_bitmap_t *out_anotb = roaring_bitmap_andnot(a, b);
        roaring_bitmap_t *out_bnota = roaring_bitmap_andnot(b, a);

        assert_non_null(out_or);
        assert_non_null(out_and);
        assert_non_null(out_xor);
        assert_non_null(out_anotb);
        assert_non_null(out_bnota);

        assert_membership_via_export_iterate(out_or, 18000U, pred_or, &pc);
        assert_membership_via_export_iterate(out_and, 18000U, pred_and, &pc);
        assert_membership_via_export_iterate(out_xor, 18000U, pred_xor, &pc);
        assert_membership_via_export_iterate(out_anotb, 18000U, pred_andnot,
                                             &pc);

        pair_ctx ba;
        ba.a = b;
        ba.b = a;
        assert_membership_via_export_iterate(out_bnota, 18000U, pred_andnot,
                                             &ba);

        const uint64_t ca = roaring_bitmap_get_cardinality(a);
        const uint64_t cb = roaring_bitmap_get_cardinality(b);
        const uint64_t cor = roaring_bitmap_get_cardinality(out_or);
        const uint64_t cand = roaring_bitmap_get_cardinality(out_and);
        const uint64_t cxor = roaring_bitmap_get_cardinality(out_xor);
        assert_true(ca + cb == cor + cand);
        assert_true(cxor + cand == cor);

        roaring_bitmap_free(out_or);
        roaring_bitmap_free(out_and);
        roaring_bitmap_free(out_xor);
        roaring_bitmap_free(out_anotb);
        roaring_bitmap_free(out_bnota);
        roaring_bitmap_free(a);
        roaring_bitmap_free(b);
    }
}

DEFINE_TEST(bitwise_inplace_matches_alloc_bitmap) {
    DESCRIBE_TEST;
    splitmix_rng_t rng_a, rng_b;
    for (int k = 0; k < 12; k++) {
        const uint64_t rs =
            UINT64_C(0x501e55afe000033) + ((uint64_t)k << UINT64_C(12));

        roaring_bitmap_t *a = roaring_bitmap_create();
        roaring_bitmap_t *b = roaring_bitmap_create();
        uint32_t gmax_a = 0, gmax_b = 0;
        splitmix_seed(&rng_a, rs);
        splitmix_seed(&rng_b, rs ^ UINT64_C(0xbeef));
        roaring_fill_bounded(a, &rng_a, 9000U, 3, &gmax_a);
        roaring_fill_bounded(b, &rng_b, 9000U, 4, &gmax_b);

        roaring_bitmap_t *ref_or = roaring_bitmap_or(a, b);
        roaring_bitmap_t *ref_and = roaring_bitmap_and(a, b);
        roaring_bitmap_t *ref_xor = roaring_bitmap_xor(a, b);
        roaring_bitmap_t *ref_an = roaring_bitmap_andnot(a, b);

        roaring_bitmap_t *ia = roaring_bitmap_copy(a);
        roaring_bitmap_or_inplace(ia, b);
        roaring_bitmap_t *ib = roaring_bitmap_copy(a);
        roaring_bitmap_and_inplace(ib, b);
        roaring_bitmap_t *ic = roaring_bitmap_copy(a);
        roaring_bitmap_xor_inplace(ic, b);
        roaring_bitmap_t *id = roaring_bitmap_copy(a);
        roaring_bitmap_andnot_inplace(id, b);

        assert_true(roaring_bitmap_equals(ia, ref_or));
        assert_true(roaring_bitmap_equals(ib, ref_and));
        assert_true(roaring_bitmap_equals(ic, ref_xor));
        assert_true(roaring_bitmap_equals(id, ref_an));

        roaring_bitmap_free(ia);
        roaring_bitmap_free(ib);
        roaring_bitmap_free(ic);
        roaring_bitmap_free(id);
        roaring_bitmap_free(ref_or);
        roaring_bitmap_free(ref_and);
        roaring_bitmap_free(ref_xor);
        roaring_bitmap_free(ref_an);
        roaring_bitmap_free(a);
        roaring_bitmap_free(b);
    }
}

DEFINE_TEST(portable_roundtrip_equals_and_matches_or_membership_oracle) {
    DESCRIBE_TEST;
    splitmix_rng_t rng_a, rng_b;

    roaring_bitmap_t *a = roaring_bitmap_create();
    roaring_bitmap_t *b = roaring_bitmap_create();
    uint32_t gmax_a = 0, gmax_b = 0;

    splitmix_seed(&rng_a, UINT64_C(0xb10c5eedface));
    splitmix_seed(&rng_b, UINT64_C(0xca7b0075));
    roaring_fill_bounded(a, &rng_a, 14000U, 6, &gmax_a);
    roaring_fill_bounded(b, &rng_b, 14000U, 5, &gmax_b);

    roaring_bitmap_t *u = roaring_bitmap_or(a, b);
    pair_ctx pc = {a, b};

    const size_t sz = roaring_bitmap_portable_size_in_bytes(u);
    char *serialized = calloc(sz ? sz : 1u, 1);
    assert_non_null(serialized);
    roaring_bitmap_portable_serialize(u, serialized);

    roaring_bitmap_t *trip =
        roaring_bitmap_portable_deserialize_safe(serialized, sz ? sz : 0);
    assert_non_null(trip);
    assert_true(roaring_bitmap_equals(u, trip));
    assert_membership_via_export_iterate(trip, 14000U, pred_or, &pc);

    roaring_bitmap_free(trip);
    free(serialized);
    roaring_bitmap_free(u);
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(bitwise_inplace_matches_alloc_bitmap),
        cmocka_unit_test(bitwise_membership_and_cardinality_metamorphism),
        cmocka_unit_test(
            portable_roundtrip_equals_and_matches_or_membership_oracle),
        cmocka_unit_test(shallow_deep_or_fold),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
