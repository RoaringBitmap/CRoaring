add_executable(openzl_roundtrip_test openzl_roundtrip_test.c)
target_link_libraries(openzl_roundtrip_test roaring openzl)
target_include_directories(openzl_roundtrip_test PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/cpp
)
add_test(NAME openzl_roundtrip_test COMMAND openzl_roundtrip_test)
