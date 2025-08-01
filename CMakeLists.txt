cmake_minimum_required(VERSION 2.8...3.15)
project(RoaringBitmap
  DESCRIPTION "Roaring bitmaps in C (and C++)"
  LANGUAGES CXX C
)
set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 11)

include(GNUInstallDirs)

if (NOT CMAKE_BUILD_TYPE)
                message(STATUS "No build type selected, default to Release")
                set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()


if(CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 11)
    message(FATAL_ERROR "${PROJECT_NAME} requires at least apple-clang version 11 to support runtime dispatching.")
endif()
set(ROARING_LIB_NAME roaring)
set(PROJECT_VERSION_MAJOR 4)
set(PROJECT_VERSION_MINOR 3)
set(PROJECT_VERSION_PATCH 6)
set(ROARING_LIB_VERSION "4.3.6" CACHE STRING "Roaring library version")
set(ROARING_LIB_SOVERSION "18" CACHE STRING "Roaring library soversion")

option(ROARING_EXCEPTIONS "Enable exception-throwing interface" ON)
if(NOT ROARING_EXCEPTIONS)
  message(STATUS "exception interface turned off. Code that does not check error codes will not compile.")
endif()


option(ROARING_DISABLE_X64 "Forcefully disable x64 optimizations even if hardware supports it (this disables AVX)" OFF)
option(ROARING_DISABLE_AVX "Forcefully disable AVX even if hardware supports it " OFF)
option(ROARING_DISABLE_NEON "Forcefully disable NEON even if hardware supports it" OFF)
option(ROARING_DISABLE_AVX512 "Forcefully disable AVX512 even if compiler supports it" OFF)

option(ROARING_BUILD_STATIC "Build a static library" ON)
if(BUILD_SHARED_LIBS)
  MESSAGE( STATUS "BUILD_SHARED_LIBS: " ${BUILD_SHARED_LIBS})
  MESSAGE( STATUS "Building a shared library by request. ")
  set(ROARING_BUILD_STATIC OFF)
endif()
option(ROARING_LINK_STATIC "Link executables (tests, benchmarks) statically" OFF)
option(ROARING_BUILD_LTO "Build library with Link Time Optimization" OFF)
option(ROARING_BUILD_C_AS_CPP "Build library C files using C++ compilation" OFF)
option(ROARING_BUILD_C_TESTS_AS_CPP "Build test C files using C++ compilation" OFF)
option(ROARING_SANITIZE "Sanitize addresses" OFF)
option(ROARING_SANITIZE_THREADS "Sanitize threads" OFF)
option(ROARING_SANITIZE_UNDEFINED "Sanitize undefined behaviors" OFF)
option(ROARING_UNSAFE_FROZEN_TESTS "If ON, tests some frozen functions which are unsafe as they include unaligned reads, this can cause crashes" OFF)

option(ENABLE_ROARING_TESTS "If OFF, disable unit tests altogether" ON)
if(NOT ENABLE_ROARING_TESTS)
  message(STATUS "Tests are disabled, you can enabled them by setting ENABLE_ROARING_TESTS to ON")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")

option(ROARING_USE_CPM "Use CPM to fetch dependencies" ON)

if(ROARING_USE_CPM)
  include(cmake/CPM.cmake)
endif()
find_package(CTargets)
find_package(Options)
find_package(LTO)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/roaring.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/roaring.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/roaring.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

## C header files get installed to /usr/local/include/roaring typically


add_library(roaring-headers INTERFACE)
target_include_directories(roaring-headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_library(roaring-headers-cpp INTERFACE)
target_include_directories(roaring-headers-cpp INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp>
   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

####
### Some users want the C++ header files to be installed as well.
### C++ header files get installed to /usr/local/include/roaring typically
SET(CPP_ROARING_HEADERS
   cpp/roaring/roaring64map.hh
   cpp/roaring/roaring.hh) # needs to be updated if we add more files
install(FILES ${CPP_ROARING_HEADERS} DESTINATION include/roaring)
install(DIRECTORY include/roaring DESTINATION include)

install(TARGETS roaring-headers roaring-headers-cpp
   EXPORT roaring-config
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
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
if(ENABLE_ROARING_TESTS AND NOT EMSCRIPTEN)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8) # we only include the benchmarks on 64-bit systems.
    add_subdirectory(benchmarks)
  endif()
  add_subdirectory(tests)
endif()
find_program(BASH bash)

if(ENABLE_ROARING_TESTS AND BASH AND NOT EMSCRIPTEN)
  message(STATUS "Amalgamation tests enabled")
  set(CROARING_SINGLEHEADER_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/amalgamation_demo.c
    ${CMAKE_CURRENT_BINARY_DIR}/amalgamation_demo.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/roaring.c
    ${CMAKE_CURRENT_BINARY_DIR}/roaring.h
    ${CMAKE_CURRENT_BINARY_DIR}/roaring.hh
  )
  set_source_files_properties(${CROARING_SINGLEHEADER_FILES} PROPERTIES GENERATED TRUE)

  add_custom_command(
    OUTPUT ${CROARING_SINGLEHEADER_FILES}
    COMMAND ${BASH} ${CMAKE_CURRENT_SOURCE_DIR}/amalgamation.sh ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/amalgamation.sh roaring
  )
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/amalgamation_demo.c PROPERTIES LANGUAGE C CMAKE_C_STANDARD 11)
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/amalgamation_demo.cpp PROPERTIES LANGUAGE CXX CXX_STANDARD 11)
  add_custom_target(croaring-singleheader-files DEPENDS ${CROARING_SINGLEHEADER_FILES})

  add_library(croaring-singleheader-include-source INTERFACE)
  target_include_directories(croaring-singleheader-include-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
  add_dependencies(croaring-singleheader-include-source croaring-singleheader-files)

  add_executable(amalgamate_demo $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/amalgamation_demo.c>)
  target_link_libraries(amalgamate_demo croaring-singleheader-include-source)
  add_test(amalgamate_demo amalgamate_demo)

  add_library(croaring-singleheader-source-lib $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/roaring.c>)
  set_target_properties(croaring-singleheader-source-lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS YES)
  target_include_directories(croaring-singleheader-source-lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

  add_executable(amalgamate_demo_cpp $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/amalgamation_demo.cpp>)
  target_link_libraries(amalgamate_demo_cpp croaring-singleheader-include-source croaring-singleheader-source-lib)
  if(ROARING_EXCEPTIONS)
    target_compile_definitions(amalgamate_demo_cpp PUBLIC ROARING_EXCEPTIONS=1)
  else()
    target_compile_definitions(amalgamate_demo_cpp PUBLIC ROARING_EXCEPTIONS=0)
  endif()
  add_test(amalgamate_demo_cpp amalgamate_demo_cpp)
else()
  message(STATUS "Amalgamation tests disabled")
endif()
option(ENABLE_ROARING_MICROBENCHMARKS "Enable microbenchmarks" OFF)
if(ENABLE_ROARING_MICROBENCHMARKS)
    add_subdirectory(microbenchmarks)
else()
     MESSAGE( STATUS "You may enable microbenchmarks by setting ENABLE_ROARING_MICROBENCHMARKS to ON " )
endif()
add_library(roaring::roaring ALIAS roaring)

set_target_properties(
  roaring PROPERTIES
  VERSION "${ROARING_LIB_VERSION}"
  SOVERSION "${ROARING_LIB_SOVERSION}"
  WINDOWS_EXPORT_ALL_SYMBOLS YES
)

set(CPACK_PACKAGE_VENDOR "Daniel Lemire")
set(CPACK_PACKAGE_CONTACT "lemire@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Roaring bitmaps in C")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_RPM_PACKAGE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
include(CPack)
