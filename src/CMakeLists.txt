if(BUILD_STATIC)
  set(ROARING_LIB_TYPE STATIC)
else()
  set(ROARING_LIB_TYPE SHARED)
endif()

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
    containers/mixed_negation.c
    containers/run.c
    roaring.c
    roaring_priority_queue.c
    roaring_array.c)

add_library(${ROARING_LIB_NAME} ${ROARING_LIB_TYPE} ${ROARING_SRC})
install(TARGETS ${ROARING_LIB_NAME} DESTINATION lib)
set_target_properties(${ROARING_LIB_NAME} PROPERTIES 
  LIBRARY_OUTPUT_DIRECTORY "..")
