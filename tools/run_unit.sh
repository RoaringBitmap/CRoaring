#!/bin/bash

set -o errexit

declare -a TESTS

TESTS=(bitset_container_unit \
       array_container_unit \
       run_container_unit \
       toplevel_unit \
       mixed_container_unit \
       unit)

function run_tests() {
  for t in ${TESTS[@]}; do
    ./$t
  done
}

function clean_build() {
  [[ -f Makefile ]] && make clean
  cmake $@ .
  cmake --build .
}

function main() {
  echo -e " \x1B[0;32mTesting non-AVX version.\x1B[0m "
  clean_build -DAVX_TUNING=0
  run_tests

  echo -e " \x1B[0;32mTesting AVX version.\x1B[0m "
  clean_build -DAVX_TUNING=1
  run_tests

  echo -e "\n\n \x1B[0;31m[\x1B[0m \x1B[0;32mAll tests clear.\x1B[0m \x1B[0;31m]\x1B[0m \n\n"
}

main
