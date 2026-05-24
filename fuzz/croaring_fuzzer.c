// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "roaring/roaring.h"
#include "roaring/roaring64.h"

int bitmap32(const char *data, size_t size) {
    // We test that deserialization never fails.
    roaring_bitmap_t *bitmap =
        roaring_bitmap_portable_deserialize_safe(data, size);
    if (bitmap) {
        // The bitmap may not be usable if it does not follow the specification.
        // We can validate the bitmap we recovered to make sure it is proper.
        const char *reason_failure = NULL;
        if (roaring_bitmap_internal_validate(bitmap, &reason_failure)) {
            // the bitmap is ok!
            uint32_t cardinality = roaring_bitmap_get_cardinality(bitmap);

            for (uint32_t i = 100; i < 1000; i++) {
                if (!roaring_bitmap_contains(bitmap, i)) {
                    cardinality++;
                    roaring_bitmap_add(bitmap, i);
                }
            }
            uint32_t new_cardinality = roaring_bitmap_get_cardinality(bitmap);
            if (cardinality != new_cardinality) {
                printf("bug\n");
                exit(1);
            }
        }
        roaring_bitmap_free(bitmap);
    }
    return 0;
}

int bitmap64(const char *data, size_t size) {
    // We test that deserialization never fails.
    roaring64_bitmap_t *bitmap =
        roaring64_bitmap_portable_deserialize_safe(data, size);
    if (bitmap) {
        // The bitmap may not be usable if it does not follow the specification.
        // We can validate the bitmap we recovered to make sure it is proper.
        const char *reason_failure = NULL;
        if (roaring64_bitmap_internal_validate(bitmap, &reason_failure)) {
            // the bitmap is ok!
            uint64_t cardinality = roaring64_bitmap_get_cardinality(bitmap);

            for (uint32_t i = 100; i < 1000; i++) {
                if (!roaring64_bitmap_contains(bitmap, i)) {
                    cardinality++;
                    roaring64_bitmap_add(bitmap, i);
                }
            }
            uint64_t new_cardinality = roaring64_bitmap_get_cardinality(bitmap);
            if (cardinality != new_cardinality) {
                printf("bug\n");
                exit(1);
            }
        }
        roaring64_bitmap_free(bitmap);
    }
    return 0;
}

static uint8_t fuzz_pull8(const uint8_t *d, size_t len, size_t *pos) {
    if (*pos >= len) {
        return (uint8_t)0;
    }
    return d[(*pos)++];
}

static uint32_t fuzz_pull_le32(const uint8_t *d, size_t len, size_t *pos) {
    uint32_t v = 0;
    for (int i = 0; i < 4 && *pos < len; i++) {
        v |= (uint32_t)d[(*pos)++] << (8 * i);
    }
    return v;
}

static roaring_bitmap_t *fuzz_or_fold_balanced(
    const roaring_bitmap_t **leaf_ptrs, int lo, int hi) {
    if (lo > hi) {
        return NULL;
    }
    if (lo == hi) {
        return roaring_bitmap_copy(leaf_ptrs[lo]);
    }
    const int mid = lo + ((hi - lo) / 2);
    roaring_bitmap_t *left = fuzz_or_fold_balanced(leaf_ptrs, lo, mid);
    roaring_bitmap_t *right = fuzz_or_fold_balanced(leaf_ptrs, mid + 1, hi);
    if (left == NULL || right == NULL) {
        roaring_bitmap_free(left);
        roaring_bitmap_free(right);
        return NULL;
    }
    roaring_bitmap_or_inplace(left, right);
    roaring_bitmap_free(right);
    return left;
}

static roaring_bitmap_t *fuzz_or_fold_left_linear(
    const roaring_bitmap_t **leaf_ptrs, uint32_t n) {
    roaring_bitmap_t *acc = roaring_bitmap_copy(leaf_ptrs[0]);
    if (acc == NULL) {
        return NULL;
    }
    for (uint32_t i = 1; i < n; i++) {
        roaring_bitmap_or_inplace(acc, leaf_ptrs[i]);
    }
    return acc;
}

static void fuzz_free_leaf_ptrs(roaring_bitmap_t **leaves, uint32_t n) {
    if (leaves == NULL) {
        return;
    }
    for (uint32_t i = 0; i < n; i++) {
        roaring_bitmap_free(leaves[i]);
    }
    free(leaves);
}

/**
 * Stress left-associated OR inplace vs divide-and-conquer OR on operands that
 * only use ids below doc_cap (union semantics oracle).
 */
static int metamorphic_or_chain_fuzz(const uint8_t *data, size_t size) {
    if (size < 12) {
        return 0;
    }

    size_t pos = 0;
    const uint32_t nb0 = (uint32_t)fuzz_pull8(data, size, &pos);
    const uint32_t nb1 = (uint32_t)fuzz_pull8(data, size, &pos);
    const uint32_t nb2 = (uint32_t)fuzz_pull8(data, size, &pos);

    uint32_t n_leaves = 2u + (nb0 + nb1 * 13u + nb2 * 7u) % 279u;

    const uint32_t qb0 = (uint32_t)fuzz_pull8(data, size, &pos);
    const uint32_t qb1 = (uint32_t)fuzz_pull8(data, size, &pos);
    const uint32_t qb2 = (uint32_t)fuzz_pull8(data, size, &pos);

    /* Bounded doc universe: 500 … 49799 inclusive (always nonzero). */
    uint32_t doc_cap = 500u + (((qb0 << 16u) | (qb1 << 8u) | qb2) % 49300u);

    roaring_bitmap_t **leaves =
        (roaring_bitmap_t **)calloc((size_t)n_leaves, sizeof(*leaves));
    if (leaves == NULL) {
        return 0;
    }

    uint32_t operand_max_seen = 0;

    for (uint32_t i = 0; i < n_leaves; i++) {
        leaves[i] = roaring_bitmap_create();
        if (leaves[i] == NULL) {
            fuzz_free_leaf_ptrs(leaves, i);
            return 0;
        }

        uint32_t nadds = (fuzz_pull_le32(data, size, &pos) >> 21) % 228u;
        nadds += 5;

        for (uint32_t j = 0; j < nadds; j++) {
            uint32_t v = fuzz_pull_le32(data, size, &pos) % doc_cap;
            roaring_bitmap_add(leaves[i], v);
            if (v > operand_max_seen) {
                operand_max_seen = v;
            }
        }
        if (roaring_bitmap_is_empty(leaves[i])) {
            uint32_t v = fuzz_pull_le32(data, size, &pos) % doc_cap;
            roaring_bitmap_add(leaves[i], v);
            if (v > operand_max_seen) {
                operand_max_seen = v;
            }
        }
        const char *reason = NULL;
        if (!roaring_bitmap_internal_validate(leaves[i], &reason)) {
            fuzz_free_leaf_ptrs(leaves, n_leaves);
            return 0;
        }
    }

    const roaring_bitmap_t **cptrs =
        (const roaring_bitmap_t **)malloc(sizeof(*cptrs) * (size_t)n_leaves);
    if (cptrs == NULL) {
        fuzz_free_leaf_ptrs(leaves, n_leaves);
        return 0;
    }
    for (uint32_t i = 0; i < n_leaves; i++) {
        cptrs[i] = leaves[i];
    }

    roaring_bitmap_t *linear = fuzz_or_fold_left_linear(cptrs, n_leaves);
    roaring_bitmap_t *bal = fuzz_or_fold_balanced(cptrs, 0, (int)n_leaves - 1);
    roaring_bitmap_t *heap_u = roaring_bitmap_or_many_heap(n_leaves, cptrs);
    roaring_bitmap_t *many_u = roaring_bitmap_or_many((size_t)n_leaves, cptrs);

    free((void *)cptrs);

    if (linear == NULL || bal == NULL || heap_u == NULL || many_u == NULL) {
        roaring_bitmap_free(linear);
        roaring_bitmap_free(bal);
        roaring_bitmap_free(heap_u);
        roaring_bitmap_free(many_u);
        fuzz_free_leaf_ptrs(leaves, n_leaves);
        abort();
    }

    if (!roaring_bitmap_equals(linear, bal) ||
        !roaring_bitmap_equals(linear, heap_u) ||
        !roaring_bitmap_equals(linear, many_u)) {
        roaring_bitmap_free(linear);
        roaring_bitmap_free(bal);
        roaring_bitmap_free(heap_u);
        roaring_bitmap_free(many_u);
        fuzz_free_leaf_ptrs(leaves, n_leaves);
        abort();
    }

    if (!roaring_bitmap_internal_validate(linear, NULL)) {
        roaring_bitmap_free(linear);
        roaring_bitmap_free(bal);
        roaring_bitmap_free(heap_u);
        roaring_bitmap_free(many_u);
        fuzz_free_leaf_ptrs(leaves, n_leaves);
        abort();
    }

    const uint64_t ncard = roaring_bitmap_get_cardinality(linear);
    if (ncard > (uint64_t)SIZE_MAX / sizeof(uint32_t)) {
        abort();
    }

    uint32_t *tmp =
        (uint32_t *)malloc(ncard ? (size_t)ncard * sizeof(uint32_t) : 1u);
    if (tmp != NULL && ncard != 0) {
        roaring_bitmap_to_uint32_array(linear, tmp);
        for (uint64_t k = 0; k < ncard; k++) {
            if (tmp[k] >= doc_cap) {
                free(tmp);
                roaring_bitmap_free(linear);
                roaring_bitmap_free(bal);
                roaring_bitmap_free(heap_u);
                roaring_bitmap_free(many_u);
                fuzz_free_leaf_ptrs(leaves, n_leaves);
                abort();
            }
            if (tmp[k] > operand_max_seen) {
                free(tmp);
                roaring_bitmap_free(linear);
                roaring_bitmap_free(bal);
                roaring_bitmap_free(heap_u);
                roaring_bitmap_free(many_u);
                fuzz_free_leaf_ptrs(leaves, n_leaves);
                abort();
            }
        }
    }
    free(tmp);

    roaring_bitmap_free(linear);
    roaring_bitmap_free(bal);
    roaring_bitmap_free(heap_u);
    roaring_bitmap_free(many_u);
    fuzz_free_leaf_ptrs(leaves, n_leaves);
    return 0;
}

typedef struct fuzz_pair_ctx {
    roaring_bitmap_t *xa;
    roaring_bitmap_t *xb;
} fuzz_pair_ctx;

static bool fuzz_pred_or_mb(uint32_t x, void *v) {
    fuzz_pair_ctx *p = (fuzz_pair_ctx *)v;
    return roaring_bitmap_contains(p->xa, x) ||
           roaring_bitmap_contains(p->xb, x);
}

static bool fuzz_pred_and_mb(uint32_t x, void *v) {
    fuzz_pair_ctx *p = (fuzz_pair_ctx *)v;
    return roaring_bitmap_contains(p->xa, x) &&
           roaring_bitmap_contains(p->xb, x);
}

static bool fuzz_pred_xor_mb(uint32_t x, void *v) {
    fuzz_pair_ctx *p = (fuzz_pair_ctx *)v;
    return roaring_bitmap_contains(p->xa, x) !=
           roaring_bitmap_contains(p->xb, x);
}

static bool fuzz_pred_ab_not_b(uint32_t x, void *v) {
    fuzz_pair_ctx *p = (fuzz_pair_ctx *)v;
    return roaring_bitmap_contains(p->xa, x) &&
           !roaring_bitmap_contains(p->xb, x);
}

static bool fuzz_pred_b_not_a(uint32_t x, void *v) {
    fuzz_pair_ctx *p = (fuzz_pair_ctx *)v;
    return roaring_bitmap_contains(p->xb, x) &&
           !roaring_bitmap_contains(p->xa, x);
}

static void fuzz_pull_fill_bitmap(roaring_bitmap_t *bmp, uint32_t doc_cap,
                                  const uint8_t *data, size_t len,
                                  size_t *pos) {
    uint32_t budget = fuzz_pull_le32(data, len, pos) % 900u + 35u;

    for (uint32_t step = 0; step < budget; step++) {
        const uint32_t coin = fuzz_pull_le32(data, len, pos);
        if ((coin % 23U == 0U) && doc_cap > 30U) {
            uint32_t base = fuzz_pull_le32(data, len, pos) % doc_cap;
            uint32_t width =
                (fuzz_pull_le32(data, len, pos) % (doc_cap - base)) + 1U;
            if (doc_cap >= base && base + width <= doc_cap) {
                roaring_bitmap_add_range(bmp, base, base + width);
            }
        } else {
            roaring_bitmap_add(bmp, fuzz_pull_le32(data, len, pos) % doc_cap);
        }
    }

    if (roaring_bitmap_is_empty(bmp)) {
        roaring_bitmap_add(bmp, fuzz_pull_le32(data, len, pos) % doc_cap);
    }
}

static void fuzz_scan_export_predicate(roaring_bitmap_t *rb, uint32_t doc_cap,
                                       bool (*ok)(uint32_t x, void *ctx),
                                       void *ctx) {
    const uint64_t ncard = roaring_bitmap_get_cardinality(rb);
    if (ncard > (uint64_t)SIZE_MAX / sizeof(uint32_t)) {
        abort();
    }
    if (ncard == 0u) {
        return;
    }
    uint32_t *buf = (uint32_t *)malloc((size_t)ncard * sizeof(uint32_t));
    if (buf == NULL) {
        return;
    }
    roaring_bitmap_to_uint32_array(rb, buf);
    for (uint64_t i = 0; i < ncard; i++) {
        const uint32_t x = buf[i];
        if (x >= doc_cap || !ok(x, ctx)) {
            free(buf);
            abort();
        }
    }
    free(buf);
}

/**
 * pairwise OR / AND / XOR / ANDNOT metamorphism + cardinality identities vs
 * export membership oracles (phantoms above operand logic).
 */
static int metamorphic_bitmap_binary_ops_fuzz(const uint8_t *data,
                                              size_t size) {
    if (size < 32) {
        return 0;
    }
    size_t pos = 0;
    uint32_t doc_cap = 640U + fuzz_pull_le32(data, size, &pos) % 46800u;
    if (doc_cap < 200u) {
        doc_cap = 200u;
    }

    roaring_bitmap_t *a = roaring_bitmap_create();
    roaring_bitmap_t *b = roaring_bitmap_create();
    if (a == NULL || b == NULL) {
        roaring_bitmap_free(a);
        roaring_bitmap_free(b);
        return 0;
    }

    fuzz_pull_fill_bitmap(a, doc_cap, data, size, &pos);
    fuzz_pull_fill_bitmap(b, doc_cap, data, size, &pos);

    if (!roaring_bitmap_internal_validate(a, NULL) ||
        !roaring_bitmap_internal_validate(b, NULL)) {
        roaring_bitmap_free(a);
        roaring_bitmap_free(b);
        return 0;
    }

    fuzz_pair_ctx pr = {a, b};

    roaring_bitmap_t *out_or = roaring_bitmap_or(a, b);
    roaring_bitmap_t *out_and = roaring_bitmap_and(a, b);
    roaring_bitmap_t *out_xor = roaring_bitmap_xor(a, b);
    roaring_bitmap_t *out_anb = roaring_bitmap_andnot(a, b);
    roaring_bitmap_t *out_bna = roaring_bitmap_andnot(b, a);

    if (out_or == NULL || out_and == NULL || out_xor == NULL ||
        out_anb == NULL || out_bna == NULL) {
        abort();
    }

    fuzz_scan_export_predicate(out_or, doc_cap, fuzz_pred_or_mb, &pr);
    fuzz_scan_export_predicate(out_and, doc_cap, fuzz_pred_and_mb, &pr);
    fuzz_scan_export_predicate(out_xor, doc_cap, fuzz_pred_xor_mb, &pr);
    fuzz_scan_export_predicate(out_anb, doc_cap, fuzz_pred_ab_not_b, &pr);
    fuzz_scan_export_predicate(out_bna, doc_cap, fuzz_pred_b_not_a, &pr);

    const uint64_t ca = roaring_bitmap_get_cardinality(a);
    const uint64_t cb = roaring_bitmap_get_cardinality(b);
    const uint64_t cor = roaring_bitmap_get_cardinality(out_or);
    const uint64_t cand = roaring_bitmap_get_cardinality(out_and);
    const uint64_t cxor = roaring_bitmap_get_cardinality(out_xor);
    if (ca + cb != cor + cand) {
        abort();
    }
    if (cxor + cand != cor) {
        abort();
    }

    roaring_bitmap_free(out_or);
    roaring_bitmap_free(out_and);
    roaring_bitmap_free(out_xor);
    roaring_bitmap_free(out_anb);
    roaring_bitmap_free(out_bna);
    roaring_bitmap_free(a);
    roaring_bitmap_free(b);
    return 0;
}

int LLVMFuzzerTestOneInput(const char *data, size_t size) {
    if (size == 0) {
        return 0;
    }
    switch (((uint8_t)data[0]) % 8u) {
        case 0:
            /* fall-through */
        case 4:
            return bitmap32(data + 1, size - 1);
        case 1:
            /* fall-through */
        case 5:
            return bitmap64(data + 1, size - 1);
        case 2:
            /* fall-through */
        case 6:
            return metamorphic_or_chain_fuzz((const uint8_t *)(data + 1),
                                             size - 1);
        default:
            /* case 3, 7 */
            return metamorphic_bitmap_binary_ops_fuzz(
                (const uint8_t *)(data + 1), size - 1);
    }
}
