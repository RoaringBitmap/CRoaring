cmake_minimum_required(VERSION 2.8.7)
set(CMAKE_MACOSX_RPATH OFF)
if (NOT CMAKE_BUILD_TYPE)
                message(STATUS "No build type selected, default to Release")
                set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

project(RoaringBitmap)
set(ROARING_LIB_NAME roaring)

option(AVX_TUNING "Enable AVX tuning" ON)
option(BUILD_STATIC "Build a static library" OFF) # turning it on disables the production of a dynamic library
option(SANITIZE "Sanitize addresses" OFF)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")

find_package(CTargets)
find_package(Options)

include_directories(include)
install(DIRECTORY include/ DESTINATION include/${ROARING_LIB_NAME})

enable_testing()
include(CTest)

##################################
#### My understanding is that variable get passed down to subdirectories, so
#### this needs to be done here.
set (TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/testdata/")
set (BENCHMARK_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/realdata/")
MESSAGE( STATUS "BENCHMARK_DATA_DIR: " ${BENCHMARK_DATA_DIR} )
MESSAGE( STATUS "TEST_DATA_DIR: " ${TEST_DATA_DIR} )
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/tests/config.h.in"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/config.h")
#################################

add_subdirectory(src)
add_subdirectory(benchmarks)
add_subdirectory(tests)

# Being terse is good, but knowing how the build is configured is important
# and should not be hard to figure out.
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} ) # this tends to be "sticky" so you can remain unknowingly in debug mode
MESSAGE( STATUS "AVX_TUNING: " ${AVX_TUNING} ) # options in cmake a "sticky" so old options can remain even if that is counterintuitive
MESSAGE( STATUS "BUILD_STATIC: " ${BUILD_STATIC} )
MESSAGE( STATUS "SANITIZE: " ${SANITIZE} )
MESSAGE( STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER} ) # important to know which compiler is used
MESSAGE (STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS} ) # important to know the flags
MESSAGE( STATUS "CMAKE_C_FLAGS_DEBUG: " ${CMAKE_C_FLAGS_DEBUG} )
MESSAGE( STATUS "CMAKE_C_FLAGS_RELEASE: " ${CMAKE_C_FLAGS_RELEASE} )
