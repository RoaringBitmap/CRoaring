#!/bin/bash

echo -e " \x1B[0;32mTesting non-AVX version.\x1B[0m "
make --silent NOAVXTUNING=1 clean all && ./bitset_container_unit &&  ./array_container_unit && ./run_container_unit && ./toplevel_unit && ./unit &&
echo -e " \x1B[0;32mTesting AVX version.\x1B[0m " &&
make --silent NOAVXTUNING=0 clean all && ./bitset_container_unit &&  ./array_container_unit && ./run_container_unit && ./toplevel_unit  && ./unit
status=$?
if [ $status -ne 0  ]; then
  echo -e "\n\n \x1B[0;31m[Some tests failed.]\x1B[0m \n\n"
else
  echo -e "\n\n \x1B[0;31m[\x1B[0m \x1B[0;32mAll tests clear.\x1B[0m \x1B[0;31m]\x1B[0m \n\n"
fi
exit $status
