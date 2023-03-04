#!/bin/bash -eu
# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

mkdir build-dir && cd build-dir
cmake -DENABLE_ROARING_TESTS=OFF ..
make -j$(nproc)

$CC $CFLAGS  \
     -I$SRC/croaring/include \
     -c $SRC/croaring_fuzzer.c -o fuzzer.o

$CXX $CXXFLAGS $LIB_FUZZING_ENGINE fuzzer.o   \
     -o $OUT/croaring_fuzzer $SRC/croaring/build-dir/src/libroaring.a

$CXX $CFLAGS $CXXFLAGS  \
     -I$SRC/croaring/include \
     -I$SRC/croaring \
     -c $SRC/croaring_fuzzer_cc.cc -o fuzzer_cc.o

$CXX $CXXFLAGS $LIB_FUZZING_ENGINE fuzzer_cc.o   \
     -o $OUT/croaring_fuzzer_cc $SRC/croaring/build-dir/src/libroaring.a

zip $OUT/croaring_fuzzer_seed_corpus.zip $SRC/croaring/tests/testdata/*bin
cp $SRC/croaring/tests/testdata/*bin $OUT/
