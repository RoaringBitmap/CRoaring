
cmake_minimum_required(VERSION 3.15)

project(test_roaring_install VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(roaring REQUIRED)

file(WRITE main.cpp "
#include <iostream>
#include \"roaring/roaring.hh\"
int main() {
  roaring::Roaring r1;
  for (uint32_t i = 100; i < 1000; i++) {
    r1.add(i);
  }
  std::cout << \"cardinality = \" << r1.cardinality() << std::endl;
  return 0;
}")

add_executable(repro main.cpp)
target_link_libraries(repro PUBLIC roaring::roaring)

file(WRITE main.c "
#include <stdio.h>
#include \"roaring/roaring.h\"
int main() {
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  for (uint32_t i = 100; i < 1000; i++) roaring_bitmap_add(r1, i);
  printf(\"cardinality = %d\\n\", (int) roaring_bitmap_get_cardinality(r1));
  roaring_bitmap_free(r1);
  return 0;
}")

add_executable(reproc main.c)
target_link_libraries(reproc PUBLIC roaring::roaring)