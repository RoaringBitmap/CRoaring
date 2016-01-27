#!/bin/bash

make --silent NOAVXTUNING=1 clean all && ./bitset_container_unit &&  ./array_container_unit && ./run_container_unit && ./toplevel_unit && ./unit &&
make --silent NOAVXTUNING=0 clean all && ./bitset_container_unit &&  ./array_container_unit && ./run_container_unit && ./toplevel_unit  && ./unit

