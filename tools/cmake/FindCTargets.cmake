if (CMAKE_VERSION VERSION_GREATER 3.0.0)
  cmake_policy(VERSION 3.0.0)
endif ()

function(add_c_test TEST_NAME)
  if(ROARING_BUILD_C_TESTS_AS_CPP)  # under C++, container_t* != void*
    SET_SOURCE_FILES_PROPERTIES(${TEST_NAME}.c PROPERTIES LANGUAGE CXX)
  endif()

  add_executable(${TEST_NAME} ${TEST_NAME}.c)
  target_link_libraries(${TEST_NAME} ${ROARING_LIB_NAME} cmocka)
  add_test(${TEST_NAME} ${TEST_NAME})
endfunction(add_c_test)


if (CMAKE_VERSION VERSION_GREATER 2.8.10)
  function(add_cpp_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
    target_compile_definitions(${TEST_NAME} PUBLIC ROARING_EXCEPTIONS=${ROARING_EXCEPTIONS})
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/cpp)
    target_link_libraries(${TEST_NAME} ${ROARING_LIB_NAME} cmocka)
    add_test(${TEST_NAME} ${TEST_NAME})
  endfunction(add_cpp_test)
else()
  function(add_cpp_test TEST_NAME)
    MESSAGE( STATUS "Your CMake version is too old for our C++ test script: " ${CMAKE_VERSION} )
  endfunction(add_cpp_test)
endif()

function(add_c_benchmark BENCH_NAME)
  add_executable(${BENCH_NAME} ${BENCH_NAME}.c)
  target_link_libraries(${BENCH_NAME} ${ROARING_LIB_NAME})
endfunction(add_c_benchmark)
