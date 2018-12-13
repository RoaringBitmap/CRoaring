#!/usr/bin/env bash
set -e
set -o pipefail

if [ $# -ne 1 ]; then
  echo "Usage: $0 <configuration>"
  exit 1
fi
conf="$1"

cd $(dirname $(readlink -f "$0"))/..

case "$conf" in
  amalgamation)
    ./amalgamation.sh
    clang -march=native -O3 -std=c11  -o amalgamation_demo amalgamation_demo.c
    ./amalgamation_demo
    clang++ -march=native -O3 -std=c++11 -o amalgamation_demo amalgamation_demo.cpp
    ./amalgamation_demo
    clang -march=native -mno-sse3 -O3 -std=c11  -o amalgamation_demo amalgamation_demo.c
    ./amalgamation_demo
    clang++ -march=native -mno-sse3 -O3 -std=c++11 -o amalgamation_demo amalgamation_demo.cpp
    ./amalgamation_demo
    ;;

  sani)
    rm -Rf build${conf}
    mkdir build${conf}
    cd build${conf}
    cmake -DCMAKE_BUILD_TYPE=Debug -DROARING_SANITIZE=ON ..
    make
    make CTEST_OUTPUT_ON_FAILURE=1 test
    ;;

  saninox64)
    rm -Rf build${conf}
    mkdir build${conf}
    cd build${conf}
    cmake -DCMAKE_BUILD_TYPE=Debug -DROARING_SANITIZE=ON -DROARING_DISABLE_X64=ON ..
    make
    make CTEST_OUTPUT_ON_FAILURE=1 test
    ;;

  release)
    rm -Rf build${conf}
    mkdir build${conf}
    cd build${conf}
    cmake ..
    make
    make CTEST_OUTPUT_ON_FAILURE=1 test
    ;;

  releasenox64)
    rm -Rf build${conf}
    mkdir build${conf}
    cd build${conf}
    cmake -DROARING_DISABLE_X64=ON ..
    make
    make CTEST_OUTPUT_ON_FAILURE=1 test
    ;;

  debug)
    rm -Rf build${conf}
    mkdir build${conf}
    cd build${conf}
    cmake -DCMAKE_BUILD_TYPE=Debug ..
    make
    make CTEST_OUTPUT_ON_FAILURE=1 test
    ;;

  debugnox64)
    rm -Rf build${conf}
    mkdir build${conf}
    cd build${conf}
    cmake  -DCMAKE_BUILD_TYPE=Debug  -DROARING_DISABLE_X64=ON ..
    make
    make CTEST_OUTPUT_ON_FAILURE=1 test
    ;;

  armhf)
    rm -Rf build${conf}
    mkdir build${conf}
    cd build${conf}
    cmake \
      -DCMAKE_C_COMPILER='arm-linux-gnueabihf-gcc-5' \
      -DCMAKE_CXX_COMPILER='arm-linux-gnueabihf-g++-5' \
      -DROARING_BUILD_STATIC=yes \
      -DROARING_LINK_STATIC=yes \
      -DROARING_DISABLE_NATIVE=no \
      -DROARING_ARCH='armv7-a' \
      ..
    make
    # Travis kills job because it doesn't get any output from
    # realdata_unit in 10 minutes.
    # Print test output to avoid this.
    make ARGS="-V" test
    ;;

  aarch64)
    rm -Rf build${conf}
    mkdir build${conf}
    cd build${conf}
    cmake \
      -DCMAKE_BUILD_TYPE=Release  \
      -DCMAKE_C_COMPILER='aarch64-linux-gnu-gcc-5' \
      -DCMAKE_CXX_COMPILER='aarch64-linux-gnu-g++-5' \
      -DROARING_BUILD_STATIC=yes \
      -DROARING_LINK_STATIC=yes \
      -DROARING_DISABLE_NATIVE=no \
      -DROARING_ARCH='armv8-a' \
      ..
    make
    make CTEST_OUTPUT_ON_FAILURE=1 test
    ;;

  *)
    echo "Unknown configuration: '$conf'"
    exit 1
    ;;
esac


