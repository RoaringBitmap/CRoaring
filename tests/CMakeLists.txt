### This code is buggy because it does not ensure that all variables have been defined :
#set (TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/testdata/")
#configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
#                "${CMAKE_CURRENT_SOURCE_DIR}/config.h")

add_c_test(bitset_container_unit)
add_c_test(array_container_unit)
add_c_test(mixed_container_unit)
add_c_test(run_container_unit)
add_c_test(toplevel_unit)
add_c_test(realdata_unit)
add_c_test(util_unit)
add_c_test(format_portability_unit)

add_subdirectory(vendor/cmocka)
