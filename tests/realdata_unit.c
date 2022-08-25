/*
 * realdata_unit.c
 */
#define _GNU_SOURCE

#include <assert.h>

#include <roaring/roaring.h>  // public api

#include <roaring/array_util.h>  // union_uint32(), intersection_uint32()
#include <roaring/misc/configreport.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
    using namespace roaring::internal;
#endif

#include "../benchmarks/numbersfromtextfiles.h"
#include "config.h"

/**
 * Once you have collected all the integers, build the bitmaps.
 */
static roaring_bitmap_t **create_all_bitmaps(size_t *howmany,
                                             uint32_t **numbers, size_t count,
                                             bool copy_on_write) {
    if (numbers == NULL) return NULL;
    printf("Constructing %d  bitmaps.\n", (int)count);
    roaring_bitmap_t **answer =
            (roaring_bitmap_t**)malloc(sizeof(roaring_bitmap_t *) * count);
    for (size_t i = 0; i < count; i++) {
        printf(".");
        fflush(stdout);
        answer[i] = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
        roaring_bitmap_set_copy_on_write(answer[i], copy_on_write);
    }
    printf("\n");
    return answer;
}

const char *datadir[] = {
    "census-income",       "census-income_srt",  "census1881",
    "census1881_srt",      "uscensus2000",       "weather_sept_85",
    "weather_sept_85_srt", "wikileaks-noquotes", "wikileaks-noquotes_srt"};

bool serialize_correctly(roaring_bitmap_t *r) {
    uint32_t expectedsize = roaring_bitmap_portable_size_in_bytes(r);
    char *serialized = (char*)malloc(expectedsize);
    if (serialized == NULL) {
        printf("failure to allocate memory!\n");
        return false;
    }
    uint32_t serialize_len = roaring_bitmap_portable_serialize(r, serialized);
    if (serialize_len != expectedsize) {
        printf("Bad serialized size!\n");
        free(serialized);
        return false;
    }
    roaring_bitmap_t *r2 = roaring_bitmap_portable_deserialize(serialized);
    free(serialized);
    if (!roaring_bitmap_equals(r, r2)) {
        printf("Won't recover original bitmap!\n");
        roaring_bitmap_free(r2);
        return false;
    }
    if (!roaring_bitmap_equals(r2, r)) {
        printf("Won't recover original bitmap!\n");
        roaring_bitmap_free(r2);
        return false;
    }
    roaring_bitmap_free(r2);
    return true;
}

// arrays expected to both be sorted.
bool array_equals(uint32_t *a1, int32_t size1, uint32_t *a2, int32_t size2) {
    if (size1 != size2) {
        printf("they differ since sizes differ %d %d\n", size1, size2);
        return false;
    }
    for (int i = 0; i < size1; ++i)
        if (a1[i] != a2[i]) {
            printf("same sizes %d %d but they differ at %d \n", size1, size2,
                   i);
            return false;
        }
    return true;
}

bool is_union_correct(roaring_bitmap_t *bitmap1, roaring_bitmap_t *bitmap2) {
    roaring_bitmap_t *temp = roaring_bitmap_or(bitmap1, bitmap2);
    if (roaring_bitmap_get_cardinality(temp) !=
        roaring_bitmap_or_cardinality(bitmap1, bitmap2)) {
        printf("bad union cardinality\n");
        return false;
    }
    uint64_t card1, card2, card;
    card1 = roaring_bitmap_get_cardinality(bitmap1);
    card2 = roaring_bitmap_get_cardinality(bitmap2);
    card = roaring_bitmap_get_cardinality(temp);
    uint32_t *arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    uint32_t *arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));

    if ((arr1 == NULL) || (arr2 == NULL) || (arr == NULL)) {
        free(arr1);
        free(arr2);
        free(arr);
        return false;
    }

    roaring_bitmap_to_uint32_array(bitmap1, arr1);
    roaring_bitmap_to_uint32_array(bitmap2, arr2);
    roaring_bitmap_to_uint32_array(temp, arr);

    uint32_t *buffer = (uint32_t *)malloc(sizeof(uint32_t) * (card1 + card2));
    size_t cardtrue = union_uint32(arr1, card1, arr2, card2, buffer);
    bool answer = array_equals(arr, card, buffer, cardtrue);
    if (!answer) {
        printf("\n\nbitmap1:\n");
        roaring_bitmap_printf_describe(bitmap1);  // debug
        printf("\n\nbitmap2:\n");
        roaring_bitmap_printf_describe(bitmap2);  // debug
        printf("\n\nresult:\n");
        roaring_bitmap_printf_describe(temp);  // debug
        roaring_bitmap_t *ca = roaring_bitmap_of_ptr(cardtrue, buffer);
        printf("\n\ncorrect result:\n");
        roaring_bitmap_printf_describe(ca);  // debug
        free(ca);
    }
    free(buffer);
    free(arr1);
    free(arr2);
    free(arr);
    roaring_bitmap_free(temp);
    return answer;
}

// function copy-pasted from toplevel_unit.c
static roaring_bitmap_t *synthesized_xor(roaring_bitmap_t *r1,
                                         roaring_bitmap_t *r2) {
    unsigned universe_size = 0;
    roaring_statistics_t stats;
    roaring_bitmap_statistics(r1, &stats);
    universe_size = stats.max_value;
    roaring_bitmap_statistics(r2, &stats);
    if (stats.max_value > universe_size) universe_size = stats.max_value;

    roaring_bitmap_t *r1_or_r2 = roaring_bitmap_or(r1, r2);
    roaring_bitmap_t *r1_and_r2 = roaring_bitmap_and(r1, r2);
    roaring_bitmap_t *r1_nand_r2 =
        roaring_bitmap_flip(r1_and_r2, 0U, universe_size + 1U);
    roaring_bitmap_t *r1_xor_r2 = roaring_bitmap_and(r1_or_r2, r1_nand_r2);
    roaring_bitmap_free(r1_or_r2);
    roaring_bitmap_free(r1_and_r2);
    roaring_bitmap_free(r1_nand_r2);
    return r1_xor_r2;
}

static roaring_bitmap_t *synthesized_andnot(roaring_bitmap_t *r1,
                                            roaring_bitmap_t *r2) {
    unsigned universe_size = 0;
    roaring_statistics_t stats;
    roaring_bitmap_statistics(r1, &stats);
    universe_size = stats.max_value;
    roaring_bitmap_statistics(r2, &stats);
    if (stats.max_value > universe_size) universe_size = stats.max_value;

    roaring_bitmap_t *not_r2 = roaring_bitmap_flip(r2, 0U, universe_size + 1U);
    roaring_bitmap_t *r1_andnot_r2 = roaring_bitmap_and(r1, not_r2);
    roaring_bitmap_free(not_r2);
    return r1_andnot_r2;
}

bool is_xor_correct(roaring_bitmap_t *bitmap1, roaring_bitmap_t *bitmap2) {
    roaring_bitmap_t *temp = roaring_bitmap_xor(bitmap1, bitmap2);
    if (roaring_bitmap_get_cardinality(temp) !=
        roaring_bitmap_xor_cardinality(bitmap1, bitmap2)) {
        printf("bad symmetric difference cardinality\n");
        return false;
    }

    roaring_bitmap_t *expected = synthesized_xor(bitmap1, bitmap2);
    bool answer = roaring_bitmap_equals(temp, expected);
    if (!answer) {
        printf("Bad XOR\n\nbitmap1:\n");
        roaring_bitmap_printf_describe(bitmap1);  // debug
        printf("\n\nbitmap2:\n");
        roaring_bitmap_printf_describe(bitmap2);  // debug
        printf("\n\nresult:\n");
        roaring_bitmap_printf_describe(temp);  // debug
        printf("\n\ncorrect result:\n");
        roaring_bitmap_printf_describe(expected);  // debug
    }
    roaring_bitmap_free(temp);
    roaring_bitmap_free(expected);
    return answer;
}

bool is_andnot_correct(roaring_bitmap_t *bitmap1, roaring_bitmap_t *bitmap2) {
    roaring_bitmap_t *temp = roaring_bitmap_andnot(bitmap1, bitmap2);
    if (roaring_bitmap_get_cardinality(temp) !=
        roaring_bitmap_andnot_cardinality(bitmap1, bitmap2)) {
        printf("bad difference cardinality\n");
        return false;
    }

    roaring_bitmap_t *expected = synthesized_andnot(bitmap1, bitmap2);
    bool answer = roaring_bitmap_equals(temp, expected);
    if (!answer) {
        printf("Bad ANDNOT\n\nbitmap1:\n");
        roaring_bitmap_printf_describe(bitmap1);  // debug
        // print_container(3, bitmap1);
        printf("\n\nbitmap2:\n");
        roaring_bitmap_printf_describe(bitmap2);  // debug
        printf("\n\nresult:\n");
        roaring_bitmap_printf_describe(temp);  // debug
        printf("\n\ncorrect result:\n");
        roaring_bitmap_printf_describe(expected);  // debug
        printf("difference is ");
        roaring_bitmap_printf(roaring_bitmap_xor(temp, expected));
    }
    roaring_bitmap_free(temp);
    roaring_bitmap_free(expected);
    return answer;
}

bool is_negation_correct(roaring_bitmap_t *bitmap) {
    roaring_statistics_t stats;
    bool answer = true;
    roaring_bitmap_statistics(bitmap, &stats);
    unsigned universe_size = stats.max_value + 1;
    roaring_bitmap_t *inverted = roaring_bitmap_flip(bitmap, 0U, universe_size);

    roaring_bitmap_t *double_inverted =
        roaring_bitmap_flip(inverted, 0U, universe_size);

    answer = (roaring_bitmap_get_cardinality(inverted) +
                  roaring_bitmap_get_cardinality(bitmap) ==
              universe_size);
    if (answer) answer = roaring_bitmap_equals(bitmap, double_inverted);

    if (!answer) {
        printf("Bad flip\n\nbitmap1:\n");
        roaring_bitmap_printf_describe(bitmap);  // debug
        printf("\n\nflipped:\n");
        roaring_bitmap_printf_describe(inverted);  // debug
    }

    roaring_bitmap_free(double_inverted);
    roaring_bitmap_free(inverted);
    return answer;
}

bool is_intersection_correct(roaring_bitmap_t *bitmap1,
                             roaring_bitmap_t *bitmap2) {
    roaring_bitmap_t *temp = roaring_bitmap_and(bitmap1, bitmap2);
    if (roaring_bitmap_get_cardinality(temp) !=
        roaring_bitmap_and_cardinality(bitmap1, bitmap2)) {
        printf("bad intersection cardinality\n");
        return false;
    }

    uint64_t card1, card2, card;
    card1 = roaring_bitmap_get_cardinality(bitmap1);
    card2 = roaring_bitmap_get_cardinality(bitmap2);
    card = roaring_bitmap_get_cardinality(temp);
    uint32_t *arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    uint32_t *arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));

    if ((arr1 == NULL) || (arr2 == NULL) || (arr == NULL)) {
        free(arr1);
        free(arr2);
        free(arr);
        return false;
    }

    roaring_bitmap_to_uint32_array(bitmap1, arr1);
    roaring_bitmap_to_uint32_array(bitmap2, arr2);
    roaring_bitmap_to_uint32_array(temp, arr);

    uint32_t *buffer = (uint32_t *)malloc(sizeof(uint32_t) * (card1 + card2));
    size_t cardtrue = intersection_uint32(arr1, card1, arr2, card2, buffer);
    bool answer = array_equals(arr, card, buffer, cardtrue);
    if (!answer) {
        printf("\n\nbitmap1:\n");
        roaring_bitmap_printf_describe(bitmap1);  // debug
        printf("\n\nbitmap2:\n");
        roaring_bitmap_printf_describe(bitmap2);  // debug
        printf("\n\nresult:\n");
        roaring_bitmap_printf_describe(temp);  // debug
        roaring_bitmap_t *ca = roaring_bitmap_of_ptr(cardtrue, buffer);
        printf("\n\ncorrect result:\n");
        roaring_bitmap_printf_describe(ca);  // debug
        free(ca);
    }
    free(buffer);
    free(arr1);
    free(arr2);
    free(arr);
    roaring_bitmap_free(temp);
    return answer;
}


bool is_intersect_correct(roaring_bitmap_t *bitmap1,
                             roaring_bitmap_t *bitmap2) {
	uint64_t c = roaring_bitmap_and_cardinality(bitmap1, bitmap2);
	if(roaring_bitmap_intersect(bitmap1,bitmap2) != (c>0)) return false;
	roaring_bitmap_t * bitmap1minus2 = roaring_bitmap_andnot(bitmap1, bitmap2);
	bool answer = true;
	if(roaring_bitmap_intersect(bitmap1minus2,bitmap2)) {
		answer = false;
	}
	roaring_bitmap_t * bitmap1plus2 = roaring_bitmap_or(bitmap1, bitmap2);
	if(!roaring_bitmap_intersect(bitmap1plus2,bitmap2)) {
		answer =  false;
	}
	roaring_bitmap_free(bitmap1minus2);
	roaring_bitmap_free(bitmap1plus2);
	return answer;
}


roaring_bitmap_t *inplace_union(roaring_bitmap_t *bitmap1,
                                roaring_bitmap_t *bitmap2) {
    roaring_bitmap_t *answer = roaring_bitmap_copy(bitmap1);
    roaring_bitmap_or_inplace(answer, bitmap2);
    return answer;
}

roaring_bitmap_t *inplace_intersection(roaring_bitmap_t *bitmap1,
                                       roaring_bitmap_t *bitmap2) {
    roaring_bitmap_t *answer = roaring_bitmap_copy(bitmap1);
    roaring_bitmap_and_inplace(answer, bitmap2);
    return answer;
}

roaring_bitmap_t *inplace_xor(roaring_bitmap_t *bitmap1,
                              roaring_bitmap_t *bitmap2) {
    roaring_bitmap_t *answer = roaring_bitmap_copy(bitmap1);
    roaring_bitmap_xor_inplace(answer, bitmap2);
    return answer;
}

roaring_bitmap_t *inplace_andnot(roaring_bitmap_t *bitmap1,
                                 roaring_bitmap_t *bitmap2) {
    roaring_bitmap_t *answer = roaring_bitmap_copy(bitmap1);
    roaring_bitmap_andnot_inplace(answer, bitmap2);
    return answer;
}

bool slow_bitmap_equals(roaring_bitmap_t *bitmap1, roaring_bitmap_t *bitmap2) {
    uint64_t card1, card2;
    card1 = roaring_bitmap_get_cardinality(bitmap1);
    card2 = roaring_bitmap_get_cardinality(bitmap2);
    uint32_t *arr1 = (uint32_t *)malloc(card1 * sizeof(uint32_t));
    uint32_t *arr2 = (uint32_t *)malloc(card2 * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(bitmap1, arr1);
    roaring_bitmap_to_uint32_array(bitmap2, arr2);
    bool answer = array_equals(arr1, card1, arr2, card2);
    free(arr1);
    free(arr2);
    return answer;
}

bool compare_intersections(roaring_bitmap_t **rnorun, roaring_bitmap_t **rruns,
                           size_t count) {
    roaring_bitmap_t *tempandnorun;
    roaring_bitmap_t *tempandruns;
    for (size_t i = 0; i + 1 < count; ++i) {
        tempandnorun = roaring_bitmap_and(rnorun[i], rnorun[i + 1]);
        if (!is_intersection_correct(rnorun[i], rnorun[i + 1])) {
            printf("no run intersection incorrect\n");
            return false;
        }
        if (!is_intersect_correct(rnorun[i], rnorun[i + 1])) {
            printf("no run intersect incorrect\n");
            return false;
        }
        tempandruns = roaring_bitmap_and(rruns[i], rruns[i + 1]);
        if (!is_intersection_correct(rruns[i], rruns[i + 1])) {
            printf("runs intersection incorrect\n");
            return false;
        }
        if (!is_intersect_correct(rruns[i], rruns[i + 1])) {
            printf("runs intersect incorrect\n");
            return false;
        }
        if (!slow_bitmap_equals(tempandnorun, tempandruns)) {
            printf("Intersections don't agree! (slow) \n");
            return false;
        }

        if (!roaring_bitmap_equals(tempandnorun, tempandruns)) {
            printf("Intersections don't agree!\n");
            printf("\n\nbitmap1:\n");
            roaring_bitmap_printf_describe(tempandnorun);  // debug
            printf("\n\nbitmap2:\n");
            roaring_bitmap_printf_describe(tempandruns);  // debug
            return false;
        }
        roaring_bitmap_free(tempandnorun);
        roaring_bitmap_free(tempandruns);

        tempandnorun = inplace_intersection(rnorun[i], rnorun[i + 1]);
        if (!is_intersection_correct(rnorun[i], rnorun[i + 1])) {
            printf("[inplace] no run intersection incorrect\n");
            return false;
        }
        if (!is_intersect_correct(rnorun[i], rnorun[i + 1])) {
             printf("[inplace] no run intersect incorrect\n");
             return false;
        }
        tempandruns = inplace_intersection(rruns[i], rruns[i + 1]);
        if (!is_intersection_correct(rruns[i], rruns[i + 1])) {
            printf("[inplace] runs intersection incorrect\n");
            return false;
        }
        if (!is_intersect_correct(rruns[i], rruns[i + 1])) {
            printf("[inplace] runs intersect incorrect\n");
            return false;
        }
        if (!slow_bitmap_equals(tempandnorun, tempandruns)) {
            printf("[inplace] Intersections don't agree! (slow) \n");
            return false;
        }

        if (!roaring_bitmap_equals(tempandnorun, tempandruns)) {
            printf("[inplace] Intersections don't agree!\n");
            printf("\n\nbitmap1:\n");
            roaring_bitmap_printf_describe(tempandnorun);  // debug
            printf("\n\nbitmap2:\n");
            roaring_bitmap_printf_describe(tempandruns);  // debug
            return false;
        }
        roaring_bitmap_free(tempandnorun);
        roaring_bitmap_free(tempandruns);
    }
    return true;
}

bool compare_unions(roaring_bitmap_t **rnorun, roaring_bitmap_t **rruns,
                    size_t count) {
    roaring_bitmap_t *tempornorun;
    roaring_bitmap_t *temporruns;
    for (size_t i = 0; i + 1 < count; ++i) {
        tempornorun = roaring_bitmap_or(rnorun[i], rnorun[i + 1]);
        if (!is_union_correct(rnorun[i], rnorun[i + 1])) {
            printf("no-run union incorrect\n");
            return false;
        }
        temporruns = roaring_bitmap_or(rruns[i], rruns[i + 1]);
        if (!is_union_correct(rruns[i], rruns[i + 1])) {
            printf("runs unions incorrect\n");
            return false;
        }
        if (!slow_bitmap_equals(tempornorun, temporruns)) {
            printf("Unions don't agree! (slow) \n");
            return false;
        }

        if (!roaring_bitmap_equals(tempornorun, temporruns)) {
            printf("Unions don't agree!\n");
            printf("\n\nbitmap1:\n");
            roaring_bitmap_printf_describe(tempornorun);  // debug
            printf("\n\nbitmap2:\n");
            roaring_bitmap_printf_describe(temporruns);  // debug
            return false;
        }
        roaring_bitmap_free(tempornorun);
        roaring_bitmap_free(temporruns);
        tempornorun = inplace_union(rnorun[i], rnorun[i + 1]);
        if (!is_union_correct(rnorun[i], rnorun[i + 1])) {
            printf("[inplace] no-run union incorrect\n");
            return false;
        }
        temporruns = inplace_union(rruns[i], rruns[i + 1]);
        if (!is_union_correct(rruns[i], rruns[i + 1])) {
            printf("[inplace] runs unions incorrect\n");
            return false;
        }

        if (!slow_bitmap_equals(tempornorun, temporruns)) {
            printf("[inplace] Unions don't agree! (slow) \n");
            return false;
        }

        if (!roaring_bitmap_equals(tempornorun, temporruns)) {
            printf("[inplace] Unions don't agree!\n");
            printf("\n\nbitmap1:\n");
            roaring_bitmap_printf_describe(tempornorun);  // debug
            printf("\n\nbitmap2:\n");
            roaring_bitmap_printf_describe(temporruns);  // debug
            return false;
        }
        roaring_bitmap_free(tempornorun);
        roaring_bitmap_free(temporruns);
    }
    return true;
}

bool compare_xors(roaring_bitmap_t **rnorun, roaring_bitmap_t **rruns,
                  size_t count) {
    roaring_bitmap_t *tempornorun;
    roaring_bitmap_t *temporruns;
    for (size_t i = 0; i + 1 < count; ++i) {
        tempornorun = roaring_bitmap_xor(rnorun[i], rnorun[i + 1]);
        if (!is_xor_correct(rnorun[i], rnorun[i + 1])) {
            printf("no-run xor incorrect\n");
            return false;
        }
        temporruns = roaring_bitmap_xor(rruns[i], rruns[i + 1]);
        if (!is_xor_correct(rruns[i], rruns[i + 1])) {
            printf("runs xors incorrect\n");
            return false;
        }
        if (!slow_bitmap_equals(tempornorun, temporruns)) {
            printf("Xors don't agree! (slow) \n");
            return false;
        }

        if (!roaring_bitmap_equals(tempornorun, temporruns)) {
            printf("Xors don't agree!\n");
            printf("\n\nbitmap1:\n");
            roaring_bitmap_printf_describe(tempornorun);  // debug
            printf("\n\nbitmap2:\n");
            roaring_bitmap_printf_describe(temporruns);  // debug
            return false;
        }
        roaring_bitmap_free(tempornorun);
        roaring_bitmap_free(temporruns);
        tempornorun = inplace_xor(rnorun[i], rnorun[i + 1]);
        if (!is_xor_correct(rnorun[i], rnorun[i + 1])) {
            printf("[inplace] no-run xor incorrect\n");
            return false;
        }
        temporruns = inplace_xor(rruns[i], rruns[i + 1]);
        if (!is_xor_correct(rruns[i], rruns[i + 1])) {
            printf("[inplace] runs xors incorrect\n");
            return false;
        }

        if (!slow_bitmap_equals(tempornorun, temporruns)) {
            printf("[inplace] Xors don't agree! (slow) \n");
            return false;
        }

        if (!roaring_bitmap_equals(tempornorun, temporruns)) {
            printf("[inplace] Xors don't agree!\n");
            printf("\n\nbitmap1:\n");
            roaring_bitmap_printf_describe(tempornorun);  // debug
            printf("\n\nbitmap2:\n");
            roaring_bitmap_printf_describe(temporruns);  // debug
            return false;
        }
        roaring_bitmap_free(tempornorun);
        roaring_bitmap_free(temporruns);
    }
    return true;
}

bool compare_andnots(roaring_bitmap_t **rnorun, roaring_bitmap_t **rruns,
                     size_t count) {
    roaring_bitmap_t *tempornorun;
    roaring_bitmap_t *temporruns;
    for (size_t i = 0; i + 1 < count; ++i) {
        tempornorun = roaring_bitmap_andnot(rnorun[i], rnorun[i + 1]);
        if (!is_andnot_correct(rnorun[i], rnorun[i + 1])) {
            printf("no-run andnot incorrect\n");
            return false;
        }
        temporruns = roaring_bitmap_andnot(rruns[i], rruns[i + 1]);
        if (!is_andnot_correct(rruns[i], rruns[i + 1])) {
            printf("runs andnots incorrect\n");
            return false;
        }
        if (!slow_bitmap_equals(tempornorun, temporruns)) {
            printf("Andnots don't agree! (slow) \n");
            return false;
        }

        if (!roaring_bitmap_equals(tempornorun, temporruns)) {
            printf("Andnots don't agree!\n");
            printf("\n\nbitmap1:\n");
            roaring_bitmap_printf_describe(tempornorun);  // debug
            printf("\n\nbitmap2:\n");
            roaring_bitmap_printf_describe(temporruns);  // debug
            return false;
        }
        roaring_bitmap_free(tempornorun);
        roaring_bitmap_free(temporruns);
        tempornorun = inplace_andnot(rnorun[i], rnorun[i + 1]);
        if (!is_andnot_correct(rnorun[i], rnorun[i + 1])) {
            printf("[inplace] no-run andnot incorrect\n");
            return false;
        }
        temporruns = inplace_andnot(rruns[i], rruns[i + 1]);
        if (!is_andnot_correct(rruns[i], rruns[i + 1])) {
            printf("[inplace] runs andnots incorrect\n");
            return false;
        }

        if (!slow_bitmap_equals(tempornorun, temporruns)) {
            printf("[inplace] Andnots don't agree! (slow) \n");
            return false;
        }

        if (!roaring_bitmap_equals(tempornorun, temporruns)) {
            printf("[inplace] Andnots don't agree!\n");
            printf("\n\nbitmap1:\n");
            roaring_bitmap_printf_describe(tempornorun);  // debug
            printf("\n\nbitmap2:\n");
            roaring_bitmap_printf_describe(temporruns);  // debug
            return false;
        }
        roaring_bitmap_free(tempornorun);
        roaring_bitmap_free(temporruns);
    }
    return true;
}

bool compare_negations(roaring_bitmap_t **rnorun, roaring_bitmap_t **rruns,
                       size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!is_negation_correct(rnorun[i])) {
            printf("no-run negation incorrect\n");
            return false;
        }
        if (!is_negation_correct(rruns[i])) {
            printf("runs negations incorrect\n");
            return false;
        }
    }
    return true;
}

bool compare_wide_unions(roaring_bitmap_t **rnorun, roaring_bitmap_t **rruns,
                         size_t count) {
    roaring_bitmap_t *tempornorun =
        roaring_bitmap_or_many(count, (const roaring_bitmap_t **)rnorun);
    roaring_bitmap_t *temporruns =
        roaring_bitmap_or_many(count, (const roaring_bitmap_t **)rruns);
    if (!slow_bitmap_equals(tempornorun, temporruns)) {
        printf("[compare_wide_unions] Unions don't agree! (fast run-norun) \n");
        return false;
    }
    assert(roaring_bitmap_equals(tempornorun, temporruns));

    roaring_bitmap_t *tempornorunheap =
        roaring_bitmap_or_many_heap(count, (const roaring_bitmap_t **)rnorun);
    roaring_bitmap_t *temporrunsheap =
        roaring_bitmap_or_many_heap(count, (const roaring_bitmap_t **)rruns);
    // assert(slow_bitmap_equals(tempornorun, tempornorunheap));
    // assert(slow_bitmap_equals(temporruns,temporrunsheap));

    assert(roaring_bitmap_equals(tempornorun, tempornorunheap));
    assert(roaring_bitmap_equals(temporruns, temporrunsheap));
    roaring_bitmap_free(tempornorunheap);
    roaring_bitmap_free(temporrunsheap);

    roaring_bitmap_t *longtempornorun;
    roaring_bitmap_t *longtemporruns;
    if (count == 1) {
        longtempornorun = rnorun[0];
        longtemporruns = rruns[0];
    } else {
        assert(roaring_bitmap_equals(rnorun[0], rruns[0]));
        assert(roaring_bitmap_equals(rnorun[1], rruns[1]));
        longtempornorun = roaring_bitmap_or(rnorun[0], rnorun[1]);
        longtemporruns = roaring_bitmap_or(rruns[0], rruns[1]);
        assert(roaring_bitmap_equals(longtempornorun, longtemporruns));
        for (int i = 2; i < (int)count; ++i) {
            assert(roaring_bitmap_equals(rnorun[i], rruns[i]));
            assert(roaring_bitmap_equals(longtempornorun, longtemporruns));

            roaring_bitmap_t *t1 =
                roaring_bitmap_or(rnorun[i], longtempornorun);
            roaring_bitmap_t *t2 = roaring_bitmap_or(rruns[i], longtemporruns);
            assert(roaring_bitmap_equals(t1, t2));
            roaring_bitmap_free(longtempornorun);
            longtempornorun = t1;
            roaring_bitmap_free(longtemporruns);
            longtemporruns = t2;
            assert(roaring_bitmap_equals(longtempornorun, longtemporruns));
        }
    }
    if (!slow_bitmap_equals(longtempornorun, tempornorun)) {
        printf("[compare_wide_unions] Unions don't agree! (regular) \n");
        return false;
    }
    if (!slow_bitmap_equals(temporruns, longtemporruns)) {
        printf("[compare_wide_unions] Unions don't agree! (runs) \n");
        return false;
    }
    roaring_bitmap_free(tempornorun);
    roaring_bitmap_free(temporruns);

    roaring_bitmap_free(longtempornorun);
    roaring_bitmap_free(longtemporruns);

    return true;
}

bool compare_wide_xors(roaring_bitmap_t **rnorun, roaring_bitmap_t **rruns,
                       size_t count) {
    roaring_bitmap_t *tempornorun =
        roaring_bitmap_xor_many(count, (const roaring_bitmap_t **)rnorun);
    roaring_bitmap_t *temporruns =
        roaring_bitmap_xor_many(count, (const roaring_bitmap_t **)rruns);
    if (!slow_bitmap_equals(tempornorun, temporruns)) {
        printf("[compare_wide_xors] Xors don't agree! (fast run-norun) \n");
        return false;
    }
    assert(roaring_bitmap_equals(tempornorun, temporruns));

    roaring_bitmap_t *longtempornorun;
    roaring_bitmap_t *longtemporruns;
    if (count == 1) {
        longtempornorun = rnorun[0];
        longtemporruns = rruns[0];
    } else {
        assert(roaring_bitmap_equals(rnorun[0], rruns[0]));
        assert(roaring_bitmap_equals(rnorun[1], rruns[1]));
        longtempornorun = roaring_bitmap_xor(rnorun[0], rnorun[1]);
        longtemporruns = roaring_bitmap_xor(rruns[0], rruns[1]);
        assert(roaring_bitmap_equals(longtempornorun, longtemporruns));
        for (int i = 2; i < (int)count; ++i) {
            assert(roaring_bitmap_equals(rnorun[i], rruns[i]));
            assert(roaring_bitmap_equals(longtempornorun, longtemporruns));

            roaring_bitmap_t *t1 =
                roaring_bitmap_xor(rnorun[i], longtempornorun);
            roaring_bitmap_t *t2 = roaring_bitmap_xor(rruns[i], longtemporruns);
            assert(roaring_bitmap_equals(t1, t2));
            roaring_bitmap_free(longtempornorun);
            longtempornorun = t1;
            roaring_bitmap_free(longtemporruns);
            longtemporruns = t2;
            assert(roaring_bitmap_equals(longtempornorun, longtemporruns));
        }
    }
    if (!slow_bitmap_equals(longtempornorun, tempornorun)) {
        printf("[compare_wide_xors] Xors don't agree! (regular) \n");
        return false;
    }
    if (!slow_bitmap_equals(temporruns, longtemporruns)) {
        printf("[compare_wide_xors] Xors don't agree! (runs) \n");
        return false;
    }
    roaring_bitmap_free(tempornorun);
    roaring_bitmap_free(temporruns);

    roaring_bitmap_free(longtempornorun);
    roaring_bitmap_free(longtemporruns);

    return true;
}

bool is_bitmap_equal_to_array(roaring_bitmap_t *bitmap, uint32_t *vals,
                              size_t numbers) {
    uint64_t card;
    card = roaring_bitmap_get_cardinality(bitmap);
    uint32_t *arr = (uint32_t *)malloc(card * sizeof(uint32_t));
    roaring_bitmap_to_uint32_array(bitmap, arr);
    bool answer = array_equals(arr, card, vals, numbers);
    free(arr);
    return answer;
}

bool loadAndCheckAll(const char *dirname, bool copy_on_write) {
    printf("[%s] %s datadir=%s %s\n", __FILE__, __func__, dirname,
           copy_on_write ? "copy-on-write" : "hard-copies");

    const char *extension = ".txt";
    size_t count;

    size_t *howmany = NULL;
    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &howmany, &count);
    if (numbers == NULL) {
        printf(
            "I could not find or load any data file with extension %s in "
            "directory %s.\n",
            extension, dirname);
        return false;
    }

    roaring_bitmap_t **bitmaps =
        create_all_bitmaps(howmany, numbers, count, copy_on_write);
    for (size_t i = 0; i < count; i++) {
        if (!is_bitmap_equal_to_array(bitmaps[i], numbers[i], howmany[i])) {
            printf("arrays don't agree with set values\n");
            return false;
        }
    }

    roaring_bitmap_t **bitmapswrun =
            (roaring_bitmap_t**)malloc(sizeof(roaring_bitmap_t *) * count);
    for (int i = 0; i < (int)count; i++) {
        bitmapswrun[i] = roaring_bitmap_copy(bitmaps[i]);
        roaring_bitmap_run_optimize(bitmapswrun[i]);
        if (roaring_bitmap_get_cardinality(bitmaps[i]) !=
            roaring_bitmap_get_cardinality(bitmapswrun[i])) {
            printf("cardinality change due to roaring_bitmap_run_optimize\n");
            return false;
        }
    }
    for (size_t i = 0; i < count; i++) {
        if (!is_bitmap_equal_to_array(bitmapswrun[i], numbers[i], howmany[i])) {
            printf("arrays don't agree with set values\n");
            return false;
        }
    }
    for (int i = 0; i < (int)count; i++) {
        if (!serialize_correctly(bitmaps[i])) {
            return false;  //  memory leaks
        }
        if (!serialize_correctly(bitmapswrun[i])) {
            return false;  //  memory leaks
        }
    }
    if (!compare_intersections(bitmaps, bitmapswrun, count)) {
        return false;  //  memory leaks
    }
    if (!compare_unions(bitmaps, bitmapswrun, count)) {
        return false;  //  memory leaks
    }
    if (!compare_wide_unions(bitmaps, bitmapswrun, count)) {
        return false;  //  memory leaks
    }

    if (!compare_negations(bitmaps, bitmapswrun, count)) {
        return false;  //  memory leaks
    }

    if (!compare_xors(bitmaps, bitmapswrun, count)) {
        return false;  //  memory leaks
    }

    if (!compare_andnots(bitmaps, bitmapswrun, count)) {
        return false;  //  memory leaks
    }

    if (!compare_wide_xors(bitmaps, bitmapswrun, count)) {
        return false;  //  memory leaks
    }

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
        roaring_bitmap_free(bitmaps[i]);
        bitmaps[i] = NULL;  // paranoid
        roaring_bitmap_free(bitmapswrun[i]);
        bitmapswrun[i] = NULL;  // paranoid
    }
    free(bitmapswrun);
    free(bitmaps);
    free(howmany);
    free(numbers);

    return true;
}

int main() {
    tellmeall();

    char dirbuffer[1024];
    size_t bddl = strlen(BENCHMARK_DATA_DIR);
    strcpy(dirbuffer, BENCHMARK_DATA_DIR);
    for (size_t i = 0; i < sizeof(datadir) / sizeof(const char *); i++) {
        strcpy(dirbuffer + bddl, datadir[i]);
        if (!loadAndCheckAll(dirbuffer, false)) {
            printf("failure\n");
            return -1;
        }
        if (!loadAndCheckAll(dirbuffer, true)) {
            printf("failure\n");
            return -1;
        }
    }

    return EXIT_SUCCESS;
}
