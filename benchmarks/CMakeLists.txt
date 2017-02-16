set (BENCHMARK_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/realdata/") # does this ever get used?

add_c_benchmark(create_benchmark)
add_c_benchmark(real_bitmaps_benchmark)
add_c_benchmark(bitset_container_benchmark)
add_c_benchmark(array_container_benchmark)
add_c_benchmark(run_container_benchmark)

# We exclude POSIX tests from Visual Studio default build 
set_target_properties(real_bitmaps_benchmark PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)

