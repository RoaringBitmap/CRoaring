### This code is buggy because it does not ensure that all variables have been defined :
#set (TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/testdata/")
#configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
#                "${CMAKE_CURRENT_SOURCE_DIR}/config.h")


add_cpp_test(cpp_unit)
add_cpp_test(cpp_random_unit)
add_cpp_test(cpp_example1)
add_cpp_test(cpp_example2)
add_c_test(c_example1)
add_c_test(cbitset_unit)
add_c_test(array_container_unit)
add_c_test(bitset_container_unit)
add_c_test(mixed_container_unit)
add_c_test(run_container_unit)
add_c_test(toplevel_unit)
add_c_test(util_unit)
add_c_test(format_portability_unit)
add_c_test(robust_deserialization_unit)
add_c_test(container_comparison_unit)
add_c_test(add_offset)
add_cpp_test(art_unit)
add_cpp_test(roaring64_unit)
add_cpp_test(roaring64_serialization)
find_package(Threads)
if(Threads_FOUND)
  message(STATUS "Your system supports threads.")
  add_executable(threads_unit threads_unit.cpp)
  target_link_libraries(threads_unit PRIVATE roaring Threads::Threads)
  if(ROARING_SANITIZE_THREADS)
    # libtsan might be needed
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      message(STATUS "Under Linux, you may need to install libtsan." )
    endif()
    target_compile_options(threads_unit PRIVATE -fsanitize=thread -fno-sanitize-recover=all)
    target_link_options(threads_unit PRIVATE -fsanitize=thread -fno-sanitize-recover=all)
    message(STATUS "Sanitizing threads.")
  endif()
  add_test(threads_unit threads_unit)
else(Threads_FOUND)
  message(STATUS "Your system does not support threads.")
endif(Threads_FOUND)

if (NOT WIN32)
# We exclude POSIX tests from Microsoft Windows
add_c_test(realdata_unit)
# We used to exclude POSIX tests from Visual Studio default build the documented way but this leads to spurious test failures.
# set_target_properties(realdata_unit PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
endif()


if(MSVC)
  add_custom_command(TARGET toplevel_unit POST_BUILD        # Adds a post-build event
    COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE:roaring>"
    COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE_DIR:toplevel_unit>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different  # which executes "cmake -E copy_if_different..."
        "$<TARGET_FILE:roaring>"      # <--this is in-file
        "$<TARGET_FILE_DIR:toplevel_unit>")                 # <--this is out-file path
endif()

get_directory_property(parent_dir PARENT_DIRECTORY)
configure_file("${parent_dir}/tools/cmake/CTestCustom.cmake" "${CMAKE_BINARY_DIR}")


# The amalgamate_demo and amalgamate_demo_cpp do not catch errors from our
# quick start instructions.

if(BASH_EXECUTABLE)
  set(AMALGAMATION_DIR "${CMAKE_BINARY_DIR}/tests/amalgamation")
  file(MAKE_DIRECTORY "${AMALGAMATION_DIR}")

  add_custom_command(
    OUTPUT "${AMALGAMATION_DIR}/roaring.c" "${AMALGAMATION_DIR}/roaring.h" "${AMALGAMATION_DIR}/roaring.hh"
    COMMAND "${BASH_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/amalgamation.sh" "${AMALGAMATION_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Running amalgamation.sh to generate amalgamated files"
    VERBATIM
  )

  add_custom_target(amalgamation ALL
    DEPENDS "${AMALGAMATION_DIR}/roaring.c" "${AMALGAMATION_DIR}/roaring.h" "${AMALGAMATION_DIR}/roaring.hh"
  )

  set(AMALG_C "${AMALGAMATION_DIR}/demo_amalg_c.c")
  file(WRITE "${AMALG_C}" "
#include <stdio.h>
#include <stdlib.h>
#include \"roaring.c\"
int main() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    for (uint32_t i = 100; i < 1000; i++) roaring_bitmap_add(r1, i);
    printf(\"cardinality = %d\\n\", (int) roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);

    bitset_t *b = bitset_create();
    for (int k = 0; k < 1000; ++k) {
            bitset_set(b, 3 * k);
    }
    printf(\"%zu \\n\", bitset_count(b));
    bitset_free(b);
    return EXIT_SUCCESS;
}
")

  add_executable(amalg_c_demo "${AMALG_C}")
  add_dependencies(amalg_c_demo amalgamation)
  target_include_directories(amalg_c_demo PRIVATE "${AMALGAMATION_DIR}")

  set(AMALG_CPP "${AMALGAMATION_DIR}/demo_amalg_cpp.cpp")
  file(WRITE "${AMALG_CPP}" "
#include <iostream>
#include \"roaring.hh\" // the amalgamated roaring.hh includes roaring64map.hh
#include \"roaring.c\"
int main() {
    roaring::Roaring r1;
    for (uint32_t i = 100; i < 1000; i++) {
        r1.add(i);
    }
    std::cout << \"cardinality = \" << r1.cardinality() << std::endl;

    roaring::Roaring64Map r2;
    for (uint64_t i = 18000000000000000100ull; i < 18000000000000001000ull; i++) {
        r2.add(i);
    }
    std::cout << \"cardinality = \" << r2.cardinality() << std::endl;
    return 0;
}
")

  add_executable(amalg_cpp_demo "${AMALG_CPP}")
  add_dependencies(amalg_cpp_demo amalgamation)
  target_include_directories(amalg_cpp_demo PRIVATE "${AMALGAMATION_DIR}")

  add_test(NAME amalg_c_demo_test COMMAND amalg_c_demo)
  add_test(NAME amalg_cpp_demo_test COMMAND amalg_cpp_demo)
else()
  message(STATUS "Bash executable not found, skipping amalgamation tests.")
endif()
