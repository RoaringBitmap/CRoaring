#!/bin/bash

make NOAVXTUNING=1 clean all
./bitset_container_unit
./array_container_unit
./run_container_unit
./unit

make NOAVXTUNING=0 clean all
./bitset_container_unit
./array_container_unit
./run_container_unit
./unit

