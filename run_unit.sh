#!/bin/bash

set -o errexit

declare -a TESTS

TESTS=(bitset_container_unit \
       array_container_unit \
       run_container_unit \
       toplevel_unit \
       unit)

function run_tests() {
  for t in ${TESTS[@]}; do
    ./$t
  done
}

function main() {
  echo -e " \x1B[0;32mTesting non-AVX version.\x1B[0m "
  make --silent NOAVXTUNING=1 clean all
  run_tests

  echo -e " \x1B[0;32mTesting AVX version.\x1B[0m "
  make --silent NOAVXTUNING=0 clean all
  run_tests

  echo -e "\n\n \x1B[0;31m[\x1B[0m \x1B[0;32mAll tests clear.\x1B[0m \x1B[0;31m]\x1B[0m \n\n"
}

main
