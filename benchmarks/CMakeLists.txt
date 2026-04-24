# Unified benchmark: one binary, --filter selects a subset of registered cases.
# Uses lemire/counters (https://github.com/lemire/counters) for hardware
# performance counters (cycles, instructions, branches, branch misses,
# cache misses).
if(ROARING_USE_CPM AND NOT WIN32)
  CPMAddPackage(
    NAME counters
    GIT_REPOSITORY https://github.com/lemire/counters.git
    GIT_TAG v4.1.0
    OPTIONS
      "COUNTERS_INSTALL OFF"
      "COUNTERS_BUILD_TESTS OFF"
  )

  # Copy the real-bitmap datasets into the build directory so the unified
  # benchmark auto-loads them by default. file(COPY) is configure-time and
  # skip-if-unchanged (preserves timestamps), so reconfigure is cheap.
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/realdata"
       DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
  set(BENCHMARK_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/realdata")

  add_executable(benchmark benchmark.cpp)
  target_link_libraries(benchmark PRIVATE roaring counters::counters)
  target_compile_definitions(benchmark PRIVATE
    BENCHMARK_DATA_DIR="${BENCHMARK_DATA_DIR}")
endif()
