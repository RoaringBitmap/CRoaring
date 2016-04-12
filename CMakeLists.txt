cmake_minimum_required(VERSION 2.6)

project(RoaringBitmap)
set(ROARING_LIB_NAME roaring)

option(AVX_TUNING "Enable AVX tuning" ON)
option(BUILD_STATIC "Build a static library" OFF)
option(SANITIZE "Sanitize addresses" OFF)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")

find_package(CTargets)
find_package(Options)

include_directories(include)
install(DIRECTORY include/ DESTINATION include/${ROARING_LIB_NAME})

enable_testing()
include(CTest)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(benchmarks)
