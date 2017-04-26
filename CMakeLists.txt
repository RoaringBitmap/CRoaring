cmake_minimum_required(VERSION 2.8)


set(CMAKE_MACOSX_RPATH OFF)
if (NOT CMAKE_BUILD_TYPE)
                message(STATUS "No build type selected, default to Release")
                set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

project(RoaringBitmap)
set(ROARING_LIB_NAME roaring)

option(DISABLE_X64 "Forcefully disable x64 optimizations even if hardware supports it (this disables AVX) " OFF)
option(DISABLE_AVX "Forcefully disable AVX even if hardware supports it" OFF)

IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
SET(DISABLE_AVX ON) # for ARM processors, there is no hope of having AVX support
SET(DISABLE_NATIVE ON) # ARM platforms may not like -march=native
ENDIF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
SET(DISABLE_AVX ON) # for ARM processors, there is no hope of having AVX support
SET(DISABLE_NATIVE ON) # ARM platforms may not like -march=native
endif()

option(BUILD_STATIC "Build a static library" OFF) # turning it on disables the production of a dynamic library
option(BUILD_LTO "Build library with Link Time Optimization" OFF)
option(SANITIZE "Sanitize addresses" OFF)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")

find_package(CTargets)
find_package(Options)
find_package(LTO)

include_directories(include)
install(DIRECTORY include/${ROARING_LIB_NAME} DESTINATION include)

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
MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} ) # this tends to be "sticky" so you can remain unknowingly in debug mode
MESSAGE( STATUS "DISABLE_X64: " ${DISABLE_X64} ) # options in cmake are "sticky" so old options can remain even if that is counterintuitive
MESSAGE( STATUS "DISABLE_AVX: " ${DISABLE_AVX} ) # options in cmake are "sticky" so old options can remain even if that is counterintuitive
MESSAGE( STATUS "BUILD_STATIC: " ${BUILD_STATIC} )
MESSAGE( STATUS "BUILD_LTO: " ${BUILD_LTO} )
MESSAGE( STATUS "SANITIZE: " ${SANITIZE} )
MESSAGE( STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER} ) # important to know which compiler is used
MESSAGE (STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS} ) # important to know the flags
MESSAGE( STATUS "CMAKE_C_FLAGS_DEBUG: " ${CMAKE_C_FLAGS_DEBUG} )
MESSAGE( STATUS "CMAKE_C_FLAGS_RELEASE: " ${CMAKE_C_FLAGS_RELEASE} )
