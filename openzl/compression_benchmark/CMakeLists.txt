set(SDDL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../roaring.sddl")
set(SDDL_COMPILED "${CMAKE_CURRENT_BINARY_DIR}/roaring_sddl.bin")
set(SDDL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/roaring_sddl.h")

# Compile the SDDL source to binary using the sddl_compiler tool.
add_custom_command(
  OUTPUT ${SDDL_COMPILED}
  COMMAND sddl_compiler < ${SDDL_SOURCE} > ${SDDL_COMPILED}
  DEPENDS ${SDDL_SOURCE} sddl_compiler
  COMMENT "Compiling roaring.sddl"
)

# Generate a C header embedding the compiled SDDL as a byte array.
add_custom_command(
  OUTPUT ${SDDL_HEADER}
  COMMAND ${CMAKE_COMMAND}
    -DINPUT_FILE=${SDDL_COMPILED}
    -DOUTPUT_FILE=${SDDL_HEADER}
    -DARRAY_NAME=roaring_sddl_compiled
    -DSIZE_NAME=roaring_sddl_compiled_size
    -DGUARD_NAME=ROARING_SDDL_H
    -P ${CMAKE_CURRENT_SOURCE_DIR}/bin2header.cmake
  DEPENDS ${SDDL_COMPILED}
  COMMENT "Generating roaring_sddl.h"
)

add_executable(compression_benchmark compression_benchmark.c ${SDDL_HEADER})
target_link_libraries(compression_benchmark roaring openzl sddl_profile)
target_include_directories(compression_benchmark PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/cpp
  ${CMAKE_CURRENT_BINARY_DIR}
  ${openzl_SOURCE_DIR}
)
