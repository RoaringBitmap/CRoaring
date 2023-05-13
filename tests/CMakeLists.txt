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


configure_file(${CMAKE_SOURCE_DIR}/tools/cmake/CTestCustom.cmake ${CMAKE_BINARY_DIR})
