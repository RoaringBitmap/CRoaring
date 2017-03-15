if("${CMAKE_C_COMPILER_ID}" MATCHES "Intel") # icc / icpc
  # prevent shared libraries from depending on Intel provided libraries
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-intel")
endif()


# we default on a shared library.
if(BUILD_STATIC)
  set(ROARING_LIB_TYPE STATIC)
  MESSAGE( STATUS "Building a static library." )
else()
  MESSAGE( STATUS "Building a dynamic library (default)." )
  set(ROARING_LIB_TYPE SHARED)
endif()

MESSAGE( STATUS "ROARING_LIB_TYPE: " ${ROARING_LIB_TYPE})
set(ROARING_SRC
    array_util.c
    bitset_util.c
    containers/array.c
    containers/bitset.c
    containers/containers.c
    containers/convert.c
    containers/mixed_intersection.c
    containers/mixed_union.c
    containers/mixed_equal.c
    containers/mixed_subset.c
    containers/mixed_negation.c
    containers/mixed_xor.c
    containers/mixed_andnot.c
    containers/run.c
    roaring.c
    roaring_priority_queue.c
    roaring_array.c)

add_library(${ROARING_LIB_NAME} ${ROARING_LIB_TYPE} ${ROARING_SRC})
install(TARGETS ${ROARING_LIB_NAME} DESTINATION lib)

if(NOT MSVC)
## We output the library at the root of the current directory where cmake is invoked
## This is handy but Visual Studio will happily ignore us
set_target_properties(${ROARING_LIB_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
MESSAGE( STATUS "Library output directory (does not apply to Visual Studio): " ${CMAKE_BINARY_DIR})
endif()

if(MSVC AND (ROARING_LIB_TYPE STREQUAL "SHARED"))
  if (CMAKE_VERSION VERSION_LESS 3.4)
    MESSAGE( STATUS "To build  a Windows DLL using Visual Studio, you may need cmake 3.4 or better." ) 
  endif()
  MESSAGE( STATUS "Building a Windows DLL using Visual Studio, exporting all symbols automatically." ) 
 set_target_properties(${ROARING_LIB_NAME} 
    PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()
