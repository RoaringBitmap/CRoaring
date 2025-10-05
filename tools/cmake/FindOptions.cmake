macro(append var string)
  set(${var} "${${var}} ${string}")
endmacro(append)


if((NOT MSVC) AND ROARING_ARCH)
set(OPT_FLAGS "-march=${ROARING_ARCH}")
endif()

if(FORCE_AVX) # some compilers like clang do not automagically define __AVX2__ and __BMI2__ even when the hardware supports it
if(NOT MSVC)
   set (OPT_FLAGS "${OPT_FLAGS} -mavx2 -mbmi2")
else()
   set (OPT_FLAGS "${OPT_FLAGS} /arch:AVX2")
endif()
endif()

if(FORCE_AVX512) # some compilers like clang do not automagically define __AVX512__ even when the hardware supports it
if(NOT MSVC)
   set (OPT_FLAGS "${OPT_FLAGS} -mbmi2 -mavx512f -mavx512bw -mavx512dq -mavx512vbmi2 -mavx512bitalg -mavx512vpopcntdq")
else()
   set (OPT_FLAGS "${OPT_FLAGS} /arch:AVX512")
endif()
endif()

if(NOT MSVC)
set(WARNING_FLAGS "-Wall")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
set(WARNING_FLAGS "${WARNING_FLAGS} -Wmissing-braces -Wextra -Wsign-compare -Wshadow -Wwrite-strings -Wpointer-arith -Winit-self")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
set(WARNING_FLAGS "${WARNING_FLAGS} -Wextra -Wsign-compare -Wshadow -Wwrite-strings -Wpointer-arith -Winit-self -Wcast-align")
endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STD_FLAGS} ${OPT_FLAGS} ${INCLUDE_FLAGS} ${WARNING_FLAGS} ")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXXSTD_FLAGS} ${OPT_FLAGS} ${INCLUDE_FLAGS} ${WARNING_FLAGS} ")

if(MSVC)
add_definitions( "/W3 /D_CRT_SECURE_NO_WARNINGS /wd4005 /wd4996 /wd4267 /wd4244  /wd4113 /nologo")
if(MSVC_VERSION GREATER 1910)
  add_definitions("/permissive-")
endif()
endif()

if(ROARING_LINK_STATIC)
  if(NOT MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "-static")
  else()
    MESSAGE(WARNING "Option ROARING_LINK_STATIC is not supported with MSVC and was ignored")
  endif()
endif()
