set(SANITIZE_FLAGS "")
if(SANITIZE)
  set(SANITIZE_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined")
endif()

set(OPT_FLAGS "-march=native")
if(AVX_TUNING)
  set (OPT_FLAGS "-DUSEAVX -mavx2 ${OPT_FLAGS}" )
endif()

set(STD_FLAGS "-std=c11 -fPIC")
set(WARNING_FLAGS "-Wall -Winline -Wshadow -Wextra -pedantic")

set(CMAKE_C_FLAGS_DEBUG "-ggdb")
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_C_FLAGS "${STD_FLAGS} ${OPT_FLAGS} ${INCLUDE_FLAGS} ${WARNING_FLAGS} ${SANITIZE_FLAGS} ")
