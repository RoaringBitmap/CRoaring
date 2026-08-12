set(OPENZL_BUILD_TOOLS ON CACHE BOOL "Build OpenZL tools (needed for SDDL compiler)" FORCE)

CPMAddPackage(
  NAME openzl
  GIT_REPOSITORY https://github.com/facebook/openzl.git
  GIT_TAG 35300cebf0bca276fae7034aa511045df0de1936
)

if(ENABLE_ROARING_TESTS)
  add_subdirectory(test)
endif()

add_subdirectory(compression_benchmark)
