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


configure_file(${CMAKE_SOURCE_DIR}/tools/cmake/CTestCustom.cmake ${CMAKE_BINARY_DIR})

set(BUILD_STATIC_LIB ON)
import_dependency(vendor/cmocka https://cmocka.org/files/1.1/cmocka-1.1.5.tar.xz)
add_subdirectory(vendor/cmocka)

