#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <roaring/roaring.h>
#include <openzl/zl_compress.h>
#include <openzl/zl_compressor.h>
#include <openzl/zl_decompress.h>
#include <custom_parsers/sddl/sddl_profile.h>

#include "../../benchmarks/numbersfromtextfiles.h"
#include "roaring_sddl.h"

static const char *datadir[] = {
    "census-income",       "census-income_srt",  "census1881",
    "census1881_srt",      "uscensus2000",       "weather_sept_85",
    "weather_sept_85_srt", "wikileaks-noquotes", "wikileaks-noquotes_srt"};

#define NUM_DATASETS (sizeof(datadir) / sizeof(datadir[0]))

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

#define MIN_TIME 0.5

/*
 * Serialize all bitmaps in a dataset into one contiguous buffer.
 * Caller must free the returned buffer.
 */
static char *serialize_dataset(const char *basedir, const char *name,
                               bool runoptimize, size_t *out_size) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", basedir, name);

    size_t count = 0;
    size_t *howmany = NULL;
    uint32_t **numbers =
        read_all_integer_files(path, ".txt", &howmany, &count);
    if (numbers == NULL || count == 0) {
        *out_size = 0;
        return NULL;
    }

    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        roaring_bitmap_t *bm = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
        if (runoptimize) roaring_bitmap_run_optimize(bm);
        total += roaring_bitmap_portable_size_in_bytes(bm);
        roaring_bitmap_free(bm);
    }

    char *buf = (char *)malloc(total);
    size_t offset = 0;
    for (size_t i = 0; i < count; i++) {
        roaring_bitmap_t *bm = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
        if (runoptimize) roaring_bitmap_run_optimize(bm);
        offset += roaring_bitmap_portable_serialize(bm, buf + offset);
        roaring_bitmap_free(bm);
    }

    for (size_t i = 0; i < count; i++) free(numbers[i]);
    free(numbers);
    free(howmany);

    *out_size = total;
    return buf;
}

typedef struct {
    size_t compressed_size;
    double compress_speed_mbs;
    double decompress_speed_mbs;
} bench_result_t;

/*
 * Compress, verify roundtrip, then time compress/decompress.
 * Returns non-zero on verification failure.
 */
static int bench_generic(const char *serialized, size_t serialized_size,
                         bench_result_t *result) {
    size_t bound = ZL_compressBound(serialized_size);
    char *compressed = (char *)malloc(bound);

    ZL_CCtx *cctx = ZL_CCtx_create();
    (void)ZL_CCtx_setParameter(cctx, ZL_CParam_formatVersion,
                                ZL_MAX_FORMAT_VERSION);

    ZL_Report r = ZL_CCtx_compress(
        cctx, compressed, bound, serialized, serialized_size);
    if (ZL_isError(r)) {
        fprintf(stderr, "generic compress failed: %s\n",
                ZL_ErrorCode_toString(ZL_errorCode(r)));
        ZL_CCtx_free(cctx);
        free(compressed);
        return 1;
    }
    size_t csz = ZL_validResult(r);

    /* Verify roundtrip. */
    char *decompressed = (char *)malloc(serialized_size);
    ZL_Report dr = ZL_decompress(decompressed, serialized_size, compressed, csz);
    if (ZL_isError(dr)) {
        fprintf(stderr, "generic roundtrip: decompress failed\n");
        free(decompressed);
        ZL_CCtx_free(cctx);
        free(compressed);
        return 1;
    }
    if (ZL_validResult(dr) != serialized_size ||
        memcmp(serialized, decompressed, serialized_size) != 0) {
        fprintf(stderr, "generic roundtrip: data mismatch\n");
        free(decompressed);
        ZL_CCtx_free(cctx);
        free(compressed);
        return 1;
    }
    free(decompressed);

    /* Time compression. */
    int iters = 0;
    double elapsed = 0;
    while (elapsed < MIN_TIME) {
        double t0 = now_seconds();
        (void)ZL_CCtx_compress(
            cctx, compressed, bound, serialized, serialized_size);
        elapsed += now_seconds() - t0;
        iters++;
    }
    ZL_CCtx_free(cctx);

    /* Time decompression. */
    decompressed = (char *)malloc(serialized_size);
    int diters = 0;
    double delapsed = 0;
    while (delapsed < MIN_TIME) {
        double t0 = now_seconds();
        (void)ZL_decompress(decompressed, serialized_size, compressed, csz);
        delapsed += now_seconds() - t0;
        diters++;
    }
    free(decompressed);
    free(compressed);

    result->compressed_size = csz;
    result->compress_speed_mbs =
        (double)serialized_size * iters / elapsed / (1024.0 * 1024.0);
    result->decompress_speed_mbs =
        (double)serialized_size * diters / delapsed / (1024.0 * 1024.0);
    return 0;
}

/*
 * Compress with SDDL, verify roundtrip, then time compress/decompress.
 * Uses the no-run SDDL profile. For run-format data where the SDDL
 * description misparses the header, permissive mode falls back to
 * generic compression.
 */
static int bench_sddl(const char *serialized, size_t serialized_size,
                       bench_result_t *result) {
    size_t bound = ZL_compressBound(serialized_size);
    char *compressed = (char *)malloc(bound);

    ZL_Compressor *comp = ZL_Compressor_create();
    ZL_RESULT_OF(ZL_GraphID) gr = ZL_SDDL_setupProfile(
        comp, roaring_sddl_compiled, roaring_sddl_compiled_size);
    if (ZL_RES_isError(gr)) {
        fprintf(stderr, "SDDL setupProfile failed\n");
        ZL_Compressor_free(comp);
        free(compressed);
        return 1;
    }
    (void)ZL_Compressor_selectStartingGraphID(comp, ZL_RES_value(gr));

    ZL_CCtx *cctx = ZL_CCtx_create();
    (void)ZL_CCtx_setParameter(cctx, ZL_CParam_formatVersion,
                                ZL_MAX_FORMAT_VERSION);
    (void)ZL_CCtx_setParameter(cctx, ZL_CParam_permissiveCompression, 1);
    (void)ZL_CCtx_refCompressor(cctx, comp);

    ZL_Report r = ZL_CCtx_compress(
        cctx, compressed, bound, serialized, serialized_size);
    if (ZL_isError(r)) {
        fprintf(stderr, "SDDL compress failed: %s\n",
                ZL_ErrorCode_toString(ZL_errorCode(r)));
        ZL_CCtx_free(cctx);
        ZL_Compressor_free(comp);
        free(compressed);
        return 1;
    }
    size_t csz = ZL_validResult(r);

    /* Verify roundtrip. */
    char *decompressed = (char *)malloc(serialized_size);
    ZL_Report dr = ZL_decompress(decompressed, serialized_size, compressed, csz);
    if (ZL_isError(dr)) {
        fprintf(stderr, "SDDL roundtrip: decompress failed\n");
        free(decompressed);
        ZL_CCtx_free(cctx);
        ZL_Compressor_free(comp);
        free(compressed);
        return 1;
    }
    if (ZL_validResult(dr) != serialized_size ||
        memcmp(serialized, decompressed, serialized_size) != 0) {
        fprintf(stderr, "SDDL roundtrip: data mismatch\n");
        free(decompressed);
        ZL_CCtx_free(cctx);
        ZL_Compressor_free(comp);
        free(compressed);
        return 1;
    }
    free(decompressed);

    /* Time compression. */
    int iters = 0;
    double elapsed = 0;
    while (elapsed < MIN_TIME) {
        double t0 = now_seconds();
        (void)ZL_CCtx_compress(
            cctx, compressed, bound, serialized, serialized_size);
        elapsed += now_seconds() - t0;
        iters++;
    }
    ZL_CCtx_free(cctx);
    ZL_Compressor_free(comp);

    /* Time decompression. */
    decompressed = (char *)malloc(serialized_size);
    int diters = 0;
    double delapsed = 0;
    while (delapsed < MIN_TIME) {
        double t0 = now_seconds();
        (void)ZL_decompress(decompressed, serialized_size, compressed, csz);
        delapsed += now_seconds() - t0;
        diters++;
    }
    free(decompressed);
    free(compressed);

    result->compressed_size = csz;
    result->compress_speed_mbs =
        (double)serialized_size * iters / elapsed / (1024.0 * 1024.0);
    result->decompress_speed_mbs =
        (double)serialized_size * diters / delapsed / (1024.0 * 1024.0);
    return 0;
}

static void print_header(void) {
    printf("  %-25s %10s %10s %10s %7s %7s %12s %12s %12s %12s\n",
           "dataset", "serial", "generic", "sddl",
           "g-ratio", "s-ratio",
           "c:gen MB/s", "c:sddl MB/s", "d:gen MB/s", "d:sddl MB/s");
    printf("  %-25s %10s %10s %10s %7s %7s %12s %12s %12s %12s\n",
           "-------", "------", "-------", "----",
           "-------", "-------",
           "----------", "-----------", "----------", "-----------");
}

static void print_row(const char *name, size_t serialized_size,
                      bench_result_t *gen, bench_result_t *sddl) {
    double ratio_g = (double)gen->compressed_size / (double)serialized_size;
    if (sddl->compressed_size > 0) {
        double ratio_s = (double)sddl->compressed_size / (double)serialized_size;
        printf("  %-25s %10zu %10zu %10zu %7.3f %7.3f %10.1f  %11.1f  %10.1f  %11.1f\n",
               name, serialized_size,
               gen->compressed_size, sddl->compressed_size,
               ratio_g, ratio_s,
               gen->compress_speed_mbs, sddl->compress_speed_mbs,
               gen->decompress_speed_mbs, sddl->decompress_speed_mbs);
    } else {
        printf("  %-25s %10zu %10zu %10s %7.3f %7s %10.1f  %11s  %10.1f  %11s\n",
               name, serialized_size,
               gen->compressed_size, "ERR", ratio_g, "ERR",
               gen->compress_speed_mbs, "ERR",
               gen->decompress_speed_mbs, "ERR");
    }
}

int main(int argc, char **argv) {
    const char *basedir = "benchmarks/realdata";
    if (argc > 1) basedir = argv[1];

    for (int pass = 0; pass < 2; pass++) {
        bool runopt = (pass == 1);
        printf("%s run optimization:\n", runopt ? "With" : "Without");
        print_header();

        for (size_t d = 0; d < NUM_DATASETS; d++) {
            size_t sz = 0;
            char *serialized = serialize_dataset(basedir, datadir[d], runopt, &sz);
            if (!serialized || sz == 0) {
                fprintf(stderr, "  %-25s  (no data)\n", datadir[d]);
                continue;
            }

            bench_result_t gen = {0}, sddl = {0};
            if (bench_generic(serialized, sz, &gen) != 0) {
                fprintf(stderr, "  %-25s  generic FAILED\n", datadir[d]);
                free(serialized);
                return 1;
            }
            if (bench_sddl(serialized, sz, &sddl) != 0) {
                /* SDDL may fail for run-optimized data; show ERR column. */
                sddl.compressed_size = 0;
            }
            print_row(datadir[d], sz, &gen, &sddl);

            free(serialized);
        }
        printf("\n");
    }

    return 0;
}
