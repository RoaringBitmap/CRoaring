
set (BENCHMARK_DATA_DIR "${PROJECT_SOURCE_DIR}/benchmarks/realdata/")

include(${PROJECT_SOURCE_DIR}/tools/cmake/Import.cmake)

set_off(BENCHMARK_ENABLE_TESTING)
set_off(BENCHMARK_ENABLE_INSTALL)
set_off(BENCHMARK_ENABLE_WERROR)
set(BENCHMARK_ENABLE_WERROR OFF)
 CPMAddPackage(
     NAME benchmark
     GITHUB_REPOSITORY google/benchmark
     GIT_TAG 3441176
     OPTIONS "BENCHMARK_ENABLE_TESTING OFF"
             "BENCHMARK_ENABLE_INSTALL OFF"
             "BENCHMARK_ENABLE_WERROR OFF"

   )

add_executable(bench bench.cpp)
target_link_libraries(bench PRIVATE roaring)
target_link_libraries(bench PRIVATE benchmark::benchmark)
target_compile_definitions(bench PRIVATE BENCHMARK_DATA_DIR="${BENCHMARK_DATA_DIR}")
