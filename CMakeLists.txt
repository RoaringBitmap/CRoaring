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

# Being terse is good, but knowing how the build is configured is important
# and should not be hard to figure out.
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} ) # this tends to be "sticky" so you can remain unknowingly in debug mode
MESSAGE( STATUS "AVX_TUNING: " ${AVX_TUNING} ) # options in cmake a "sticky" so old options can remain even if that is counterintuitive
MESSAGE( STATUS "BUILD_STATIC: " ${BUILD_STATIC} )
MESSAGE( STATUS "SANITIZE: " ${SANITIZE} )
MESSAGE( STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER} ) # important to know which compiler is used
MESSAGE (STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS} ) # important to know the flags

