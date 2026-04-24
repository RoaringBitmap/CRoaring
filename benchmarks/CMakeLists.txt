# Unified benchmark: one binary, --filter selects a subset of registered cases.
# Uses sea_counters (https://github.com/lemire/sea_counters) for hardware
# performance counters (cycles, instructions, branches, branch misses).
if(ROARING_USE_CPM AND NOT WIN32)
  CPMAddPackage(
    NAME sea_counters
    GIT_REPOSITORY https://github.com/lemire/sea_counters.git
    GIT_TAG v3.0.0
    OPTIONS
      "SEA_COUNTERS_INSTALL OFF"
      "SEA_SEA_COUNTERS_BUILD_TESTS OFF"
  )

  # Copy the real-bitmap datasets into the build directory so the unified
  # benchmark auto-loads them by default. file(COPY) is configure-time and
  # skip-if-unchanged (preserves timestamps), so reconfigure is cheap.
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/realdata"
       DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
  set(BENCHMARK_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/realdata")

  add_executable(unified_benchmark unified_benchmark.cpp)
  target_link_libraries(unified_benchmark PRIVATE roaring sea_counters::sea_counters)
  target_compile_definitions(unified_benchmark PRIVATE
    BENCHMARK_DATA_DIR="${BENCHMARK_DATA_DIR}")
endif()
