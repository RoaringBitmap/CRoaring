#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/roaring.h>
#include <openzl/zl_compress.h>
#include <openzl/zl_decompress.h>

/*
 * Serialize a roaring bitmap, compress it with OpenZL, decompress,
 * and verify the round-tripped data produces an identical bitmap.
 */
static int roundtrip(roaring_bitmap_t *bitmap, const char *label) {
    /* Portable-serialize the bitmap. */
    uint32_t serialized_size = roaring_bitmap_portable_size_in_bytes(bitmap);
    char *serialized = (char *)malloc(serialized_size);
    if (!serialized) {
        fprintf(stderr, "[%s] malloc(serialized) failed\n", label);
        return 1;
    }
    roaring_bitmap_portable_serialize(bitmap, serialized);

    /* Compress with OpenZL. */
    ZL_CCtx *cctx = ZL_CCtx_create();
    if (!cctx) {
        fprintf(stderr, "[%s] ZL_CCtx_create failed\n", label);
        free(serialized);
        return 1;
    }
    (void)ZL_CCtx_setParameter(cctx, ZL_CParam_formatVersion, ZL_MAX_FORMAT_VERSION);

    size_t compress_bound = ZL_compressBound(serialized_size);
    char *compressed = (char *)malloc(compress_bound);
    if (!compressed) {
        fprintf(stderr, "[%s] malloc(compressed) failed\n", label);
        ZL_CCtx_free(cctx);
        free(serialized);
        return 1;
    }

    ZL_Report creport = ZL_CCtx_compress(
        cctx, compressed, compress_bound, serialized, serialized_size);
    if (ZL_isError(creport)) {
        fprintf(stderr, "[%s] ZL_CCtx_compress failed: %s\n",
                label, ZL_ErrorCode_toString(ZL_errorCode(creport)));
        free(compressed);
        ZL_CCtx_free(cctx);
        free(serialized);
        return 1;
    }
    size_t compressed_size = ZL_validResult(creport);

    printf("[%s] serialized %u bytes -> compressed %zu bytes (%.1f%%)\n",
           label, serialized_size, compressed_size,
           100.0 * (double)compressed_size / (double)serialized_size);

    ZL_CCtx_free(cctx);

    /* Decompress with OpenZL. */
    ZL_Report dreport_size = ZL_getDecompressedSize(compressed, compressed_size);
    if (ZL_isError(dreport_size)) {
        fprintf(stderr, "[%s] ZL_getDecompressedSize failed: %s\n",
                label, ZL_ErrorCode_toString(ZL_errorCode(dreport_size)));
        free(compressed);
        free(serialized);
        return 1;
    }
    size_t decompressed_size = ZL_validResult(dreport_size);

    char *decompressed = (char *)malloc(decompressed_size);
    if (!decompressed) {
        fprintf(stderr, "[%s] malloc(decompressed) failed\n", label);
        free(compressed);
        free(serialized);
        return 1;
    }

    ZL_Report dreport = ZL_decompress(
        decompressed, decompressed_size, compressed, compressed_size);
    if (ZL_isError(dreport)) {
        fprintf(stderr, "[%s] ZL_decompress failed: %s\n",
                label, ZL_ErrorCode_toString(ZL_errorCode(dreport)));
        free(decompressed);
        free(compressed);
        free(serialized);
        return 1;
    }

    free(compressed);

    /* Verify byte-for-byte equality. */
    if (decompressed_size != serialized_size) {
        fprintf(stderr, "[%s] size mismatch: expected %u, got %zu\n",
                label, serialized_size, decompressed_size);
        free(decompressed);
        free(serialized);
        return 1;
    }
    if (memcmp(serialized, decompressed, serialized_size) != 0) {
        fprintf(stderr, "[%s] decompressed data differs from original\n", label);
        free(decompressed);
        free(serialized);
        return 1;
    }

    /* Deserialize the round-tripped bytes and verify bitmap equality. */
    roaring_bitmap_t *recovered =
        roaring_bitmap_portable_deserialize_safe(decompressed, decompressed_size);
    free(decompressed);
    free(serialized);

    if (!recovered) {
        fprintf(stderr, "[%s] portable_deserialize_safe returned NULL\n", label);
        return 1;
    }
    if (!roaring_bitmap_equals(bitmap, recovered)) {
        fprintf(stderr, "[%s] recovered bitmap differs from original\n", label);
        roaring_bitmap_free(recovered);
        return 1;
    }

    roaring_bitmap_free(recovered);
    printf("[%s] PASSED\n", label);
    return 0;
}

int main(void) {
    int failures = 0;

    /* 1. Sparse bitmap (array containers only). */
    {
        roaring_bitmap_t *bm = roaring_bitmap_create();
        roaring_bitmap_add(bm, 1);
        roaring_bitmap_add(bm, 100);
        roaring_bitmap_add(bm, 1000);
        roaring_bitmap_add(bm, 10000);
        roaring_bitmap_add(bm, 100000);
        roaring_bitmap_add(bm, 1000000);
        failures += roundtrip(bm, "sparse");
        roaring_bitmap_free(bm);
    }

    /* 2. Dense bitmap (bitset containers). */
    {
        roaring_bitmap_t *bm = roaring_bitmap_create();
        for (uint32_t i = 0; i < 100000; i++) {
            roaring_bitmap_add(bm, 3 * i);
        }
        failures += roundtrip(bm, "dense");
        roaring_bitmap_free(bm);
    }

    /* 3. Run-optimized bitmap with long consecutive runs. */
    {
        roaring_bitmap_t *bm = roaring_bitmap_create();
        roaring_bitmap_add_range_closed(bm, 0, 49999);
        roaring_bitmap_add_range_closed(bm, 100000, 199999);
        roaring_bitmap_add_range_closed(bm, 500000, 599999);
        roaring_bitmap_run_optimize(bm);
        failures += roundtrip(bm, "runs");
        roaring_bitmap_free(bm);
    }

    /* 4. Mixed containers: sparse + dense + runs in one bitmap. */
    {
        roaring_bitmap_t *bm = roaring_bitmap_create();
        /* Container 0: sparse (array) */
        roaring_bitmap_add(bm, 10);
        roaring_bitmap_add(bm, 200);
        roaring_bitmap_add(bm, 3000);
        /* Container 1 (high16 = 1): dense (bitset) */
        for (uint32_t i = 0; i < 50000; i++) {
            roaring_bitmap_add(bm, (1 << 16) + i);
        }
        /* Container 2 (high16 = 2): consecutive run */
        roaring_bitmap_add_range_closed(bm, 2 * (1 << 16), 2 * (1 << 16) + 9999);
        roaring_bitmap_run_optimize(bm);
        failures += roundtrip(bm, "mixed");
        roaring_bitmap_free(bm);
    }

    /* 5. Empty bitmap. */
    {
        roaring_bitmap_t *bm = roaring_bitmap_create();
        failures += roundtrip(bm, "empty");
        roaring_bitmap_free(bm);
    }

    /* 6. Large bitmap with many containers. */
    {
        roaring_bitmap_t *bm = roaring_bitmap_create();
        for (uint32_t i = 0; i < 1000000; i += 7) {
            roaring_bitmap_add(bm, i);
        }
        roaring_bitmap_run_optimize(bm);
        failures += roundtrip(bm, "large_with_runs");
        roaring_bitmap_free(bm);
    }

    if (failures) {
        fprintf(stderr, "\n%d test(s) FAILED\n", failures);
    } else {
        printf("\nAll tests PASSED\n");
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
