if (CMAKE_VERSION VERSION_GREATER 3.0.0)
  cmake_policy(VERSION 3.0.0)
endif ()

function(add_c_test TEST_NAME)
  add_executable(${TEST_NAME} ${TEST_NAME}.c)
  target_link_libraries(${TEST_NAME} ${ROARING_LIB_NAME} cmocka)
  add_test(${TEST_NAME} ${TEST_NAME})
endfunction(add_c_test)

function(add_c_benchmark BENCH_NAME)
  add_executable(${BENCH_NAME} ${BENCH_NAME}.c)
  target_link_libraries(${BENCH_NAME} ${ROARING_LIB_NAME})
endfunction(add_c_benchmark)
