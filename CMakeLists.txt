cmake_minimum_required (VERSION 2.6)

project (RoaringBitmap)

option (AVX_TUNING "Enable AVX tuning" ON)
option (BUILD_STATIC "Build a static library" OFF)
option (DEBUG "Create a debug build" OFF)

set (ROARING_LIB_NAME roaring)
if (BUILD_STATIC)
  set (ROARING_LIB_TYPE STATIC)
else ()
  set (ROARING_LIB_TYPE SHARED)
endif()

set (ROARING_SRC
    src/array_util.c
    src/bitset_util.c
    src/containers/array.c
    src/containers/bitset.c
    src/containers/containers.c
    src/containers/convert.c
    src/containers/mixed_intersection.c
    src/containers/mixed_union.c
    src/containers/run.c
    src/roaring.c
    src/roaring_array.c)

add_library (${ROARING_LIB_NAME} ${ROARING_LIB_TYPE} ${ROARING_SRC})

install (TARGETS ${ROARING_LIB_NAME} DESTINATION lib)
install (DIRECTORY include/ DESTINATION include/${ROARING_LIB_NAME})

set (STD_FLAGS "-std=c11 -fPIC")
set (OPT_FLAGS "-march=native")
set (WARNING_FLAGS "-Wall -Winline -Wshadow -Wextra -pedantic")
set (INCLUDE_FLAGS "-Iinclude -Iinclude/containers -Ibenchmark")

if (AVX_TUNING)
  set (OPT_FLAGS "-DUSEAVX -mavx2 ${OPT_FLAGS}" )
endif()

if (DEBUG)
  set (DEBUG_FLAGS "-ggdb")
else ()
  set (OPT_FLAGS "-O3 ${OPT_FLAGS}")
endif()

set (CMAKE_C_FLAGS "${STD_FLAGS} ${OPT_FLAGS} ${INCLUDE_FLAGS} ${WARNING_FLAGS} ${DEBUG_FLAGS}")

enable_testing()
add_executable (unit tests/unit.c)
target_link_libraries (unit roaring)
add_test (unit unit)
add_executable (bitset_container_unit tests/bitset_container_unit.c)
target_link_libraries (bitset_container_unit roaring)
add_test (bitset_container_unit bitset_container_unit)
add_executable (array_container_unit tests/array_container_unit.c)
target_link_libraries (array_container_unit roaring)
add_test (array_container_unit array_container_unit)
add_executable (mixed_container_unit tests/mixed_container_unit.c)
target_link_libraries (mixed_container_unit roaring)
add_test (mixed_container_unit mixed_container_unit)
add_executable (run_container_unit tests/unit.c)
target_link_libraries (run_container_unit roaring)
add_test (run_container_unit unit)
add_executable (toplevel_unit tests/toplevel_unit.c)
target_link_libraries (toplevel_unit roaring)
add_test (toplevel_unit toplevel_unit)

# invoke all tests with: make check
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose DEPENDS
    unit
    bitset_container_unit
    array_container_unit
    mixed_container_unit
    run_container_unit
    toplevel_unit)

add_executable (real_bitmaps_benchmark benchmarks/real_bitmaps_benchmark.c)
target_link_libraries (real_bitmaps_benchmark roaring)
add_executable (bitset_container_benchmark benchmarks/bitset_container_benchmark.c)
target_link_libraries (bitset_container_benchmark roaring)
add_executable (array_container_benchmark benchmarks/array_container_benchmark.c)
target_link_libraries (array_container_benchmark roaring)
add_executable (run_container_benchmark benchmarks/run_container_benchmark.c)
target_link_libraries (run_container_benchmark roaring)
