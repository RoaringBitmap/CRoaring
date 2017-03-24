/*
  Typedefs used by various components
*/

#ifndef ROARING_TYPES_H
#define ROARING_TYPES_H

typedef bool (*roaring_iterator)(uint32_t value, void *param);
typedef bool (*roaring_iterator64)(uint64_t value, void *param);

/**
*  (For advanced users.)
* The roaring_statistics_t can be used to collect detailed statistics about
* the composition of a roaring bitmap.
*/
typedef struct roaring_statistics_s {
    uint32_t n_containers; /* number of containers */

    uint32_t n_array_containers;  /* number of array containers */
    uint32_t n_run_containers;    /* number of run containers */
    uint32_t n_bitset_containers; /* number of bitmap containers */

    uint32_t
        n_values_array_containers;    /* number of values in array containers */
    uint32_t n_values_run_containers; /* number of values in run containers */
    uint32_t
        n_values_bitset_containers; /* number of values in  bitmap containers */

    uint32_t n_bytes_array_containers;  /* number of allocated bytes in array
                                           containers */
    uint32_t n_bytes_run_containers;    /* number of allocated bytes in run
                                           containers */
    uint32_t n_bytes_bitset_containers; /* number of allocated bytes in  bitmap
                                           containers */

    uint32_t
        max_value; /* the maximal value, undefined if cardinality is zero */
    uint32_t
        min_value; /* the minimal value, undefined if cardinality is zero */
    uint64_t sum_value; /* the sum of all values (could be used to compute
                           average) */

    uint64_t cardinality; /* total number of values stored in the bitmap */

    // and n_values_arrays, n_values_rle, n_values_bitmap
} roaring_statistics_t;

#endif /* ROARING_TYPES_H */
