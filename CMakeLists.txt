cmake_minimum_required(VERSION 2.8...3.15)
project(RoaringBitmap
  DESCRIPTION "Roaring bitmaps in C (and C++)"
  LANGUAGES CXX C
)
include(GNUInstallDirs)

set(CMAKE_MACOSX_RPATH OFF)
if (NOT CMAKE_BUILD_TYPE)
                message(STATUS "No build type selected, default to Release")
                set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()


if(CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 11)
    message(FATAL_ERROR "${PROJECT_NAME} requires at least apple-clang version 11 to support runtime dispatching.")
endif()
set(ROARING_LIB_NAME roaring)
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 7)
set(PROJECT_VERSION_PATCH 3)
set(ROARING_LIB_VERSION "0.7.3" CACHE STRING "Roaring library version")
set(ROARING_LIB_SOVERSION "5" CACHE STRING "Roaring library soversion")

option(ROARING_EXCEPTIONS "Enable exception-throwing interface" ON)
if(NOT ROARING_EXCEPTIONS)
  message(STATUS "exception interface turned off. Code that does not check error codes will not compile.")
endif()


option(ROARING_DISABLE_X64 "Forcefully disable x64 optimizations even if hardware supports it (this disables AVX)" OFF)
option(ROARING_DISABLE_AVX "Forcefully disable AVX even if hardware supports it " OFF)
option(ROARING_DISABLE_NEON "Forcefully disable NEON even if hardware supports it" OFF)
option(ROARING_DISABLE_NATIVE "Forcefully disable -march optimizations (obsolete)" OFF)

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
option(ENABLE_ROARING_TESTS "If OFF, disable unit tests altogether" ON)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")

find_package(CTargets)
find_package(Options)
find_package(LTO)
include(Import)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/roaring.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/roaring.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/roaring.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

## C header files get installed to /usr/local/include/roaring typically


add_library(roaring-headers INTERFACE)
target_include_directories(roaring-headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${ROARING_LIB_NAME}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCDIR}>)
add_library(roaring-headers-cpp INTERFACE)
target_include_directories(roaring-headers-cpp INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp>
   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCDIR}>)

####
### Some users want the C++ header files to be installed as well.
### C++ header files get installed to /usr/local/include/roaring typically
SET(CPP_ROARING_HEADERS cpp/roaring64map.hh  cpp/roaring.hh) # needs to be updated if we add more files
install(FILES ${CPP_ROARING_HEADERS} DESTINATION include/${ROARING_LIB_NAME})
install(DIRECTORY include/${ROARING_LIB_NAME} DESTINATION include)

install(TARGETS roaring-headers roaring-headers-cpp
   EXPORT ${ROARING_LIB_NAME}-config
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   INCLUDES DESTINATION ${CMAKE_INSTALL_INCDIR})
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
if(ENABLE_ROARING_TESTS)
  add_subdirectory(benchmarks)
  add_subdirectory(tests)
endif()
# Being terse is good, but knowing how the build is configured is important
# and should not be hard to figure out.
MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} ) # this tends to be "sticky" so you can remain unknowingly in debug mode
MESSAGE( STATUS "ROARING_DISABLE_NEON: " ${ROARING_DISABLE_NEON} )
MESSAGE( STATUS "ROARING_BUILD_STATIC: " ${ROARING_BUILD_STATIC} )
MESSAGE( STATUS "ROARING_LINK_STATIC: " ${ROARING_LINK_STATIC} )
MESSAGE( STATUS "ROARING_BUILD_LTO: " ${ROARING_BUILD_LTO} )
MESSAGE( STATUS "ROARING_SANITIZE: " ${ROARING_SANITIZE} )
MESSAGE( STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER} ) # important to know which compiler is used
MESSAGE( STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS} ) # important to know the flags
MESSAGE( STATUS "CMAKE_C_FLAGS_DEBUG: " ${CMAKE_C_FLAGS_DEBUG} )
MESSAGE( STATUS "CMAKE_C_FLAGS_RELEASE: " ${CMAKE_C_FLAGS_RELEASE} )
MESSAGE( STATUS "ROARING_EXCEPTIONS: " ${ROARING_EXCEPTIONS} )

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
