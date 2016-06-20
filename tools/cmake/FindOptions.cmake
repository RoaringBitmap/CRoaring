set(SANITIZE_FLAGS "")
if(SANITIZE)
  set(SANITIZE_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined")
endif()

set(OPT_FLAGS "-march=native")
if(AVX_TUNING)
  # even if AVX_TUNING is enabled, the code can still disable it if __AVX2__ or __BMI2__ are undefined
  set (OPT_FLAGS "${OPT_FLAGS} -DUSEAVX  ${OPT_FLAGS}" )
endif()

if(FORCE_AVX) # some compilers like clang do not automatigically define __AVX2__ and __BMI2__ even when the hardware supports it 
   set (OPT_FLAGS "${OPT_FLAGS} -mavx2 -mbmi2")
endif()

set(STD_FLAGS "-std=c11 -fPIC")
set(CXXSTD_FLAGS "-std=c++11 -fPIC")

set(WARNING_FLAGS "-Wall -Winline -Wshadow -Wextra -pedantic")

set(CMAKE_C_FLAGS_DEBUG "-ggdb")
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_C_FLAGS "${STD_FLAGS} ${OPT_FLAGS} ${INCLUDE_FLAGS} ${WARNING_FLAGS} ${SANITIZE_FLAGS} ")

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS "${CXXSTD_FLAGS} ${OPT_FLAGS} ${INCLUDE_FLAGS} ${WARNING_FLAGS} ${SANITIZE_FLAGS} ")
