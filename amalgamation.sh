#!/bin/bash
########################################################################
# Generates an "amalgamation build" for roaring. Inspired by similar
# script used by whefs.
########################################################################
SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

echo "We are about to amalgamate all CRoaring files into one source file. "
echo "See https://www.sqlite.org/amalgamation.html and https://en.wikipedia.org/wiki/Single_Compilation_Unit for rationale. "

AMAL_H="roaring.h"
AMAL_C="roaring.c"

# order does not matter
ALLCFILES=$( ( [ -d $SCRIPTPATH/.git ] && ( type git >/dev/null 2>&1 ) &&  ( git ls-files $SCRIPTPATH/src/*.c $SCRIPTPATH/src/**/*c ) ) ||  ( find $SCRIPTPATH/src -name '*.c' ) )

# order matters
ALLCHEADERS="
$SCRIPTPATH/include/roaring/roaring_version.h
$SCRIPTPATH/include/roaring/portability.h
$SCRIPTPATH/include/roaring/containers/perfparameters.h
$SCRIPTPATH/include/roaring/array_util.h
$SCRIPTPATH/include/roaring/roaring_types.h
$SCRIPTPATH/include/roaring/utilasm.h
$SCRIPTPATH/include/roaring/bitset_util.h
$SCRIPTPATH/include/roaring/containers/array.h
$SCRIPTPATH/include/roaring/containers/bitset.h
$SCRIPTPATH/include/roaring/containers/run.h
$SCRIPTPATH/include/roaring/containers/convert.h
$SCRIPTPATH/include/roaring/containers/mixed_equal.h
$SCRIPTPATH/include/roaring/containers/mixed_subset.h
$SCRIPTPATH/include/roaring/containers/mixed_andnot.h
$SCRIPTPATH/include/roaring/containers/mixed_intersection.h
$SCRIPTPATH/include/roaring/containers/mixed_negation.h
$SCRIPTPATH/include/roaring/containers/mixed_union.h
$SCRIPTPATH/include/roaring/containers/mixed_xor.h
$SCRIPTPATH/include/roaring/containers/containers.h
$SCRIPTPATH/include/roaring/roaring_array.h
$SCRIPTPATH/include/roaring/misc/configreport.h
$SCRIPTPATH/include/roaring/roaring.h
"

for i in ${ALLCHEADERS} ${ALLCFILES}; do
    test -e $i && continue
    echo "FATAL: source file [$i] not found."
    exit 127
done


function stripinc()
{
    sed -e '/# *include *"/d' -e '/# *include *<roaring\//d'
}
function dofile()
{
    echo "/* begin file $1 */"
    # echo "#line 8 \"$1\"" ## redefining the line/file is not nearly as useful as it sounds for debugging. It breaks IDEs.
    stripinc < $1
    echo "/* end file $1 */"
}

timestamp=$(date)
echo "Creating ${AMAL_H}..."
echo "/* auto-generated on ${timestamp}. Do not edit! */" > "${AMAL_H}"
{
    for h in ${ALLCHEADERS}; do
        dofile $h
    done
} >> "${AMAL_H}"


echo "Creating ${AMAL_C}..."
echo "/* auto-generated on ${timestamp}. Do not edit! */" > "${AMAL_C}"
{
    echo "#include \"${AMAL_H}\""

    for h in ${ALLCFILES}; do
        dofile $h
    done
} >> "${AMAL_C}"




DEMOC="amalgamation_demo.c"
echo "Creating ${DEMOC}..."
echo "/* auto-generated on ${timestamp}. Do not edit! */" > "${DEMOC}"
cat <<< '
#include <stdio.h>
#include "roaring.c"
int main() {
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  for (uint32_t i = 100; i < 1000; i++) roaring_bitmap_add(r1, i);
  printf("cardinality = %d\n", (int) roaring_bitmap_get_cardinality(r1));
  roaring_bitmap_free(r1);
  return 0;
}
' >>  "${DEMOC}"


echo "Done with C amalgamation. Proceeding with C++ wrap."

AMAL_HH="roaring.hh"

echo "Creating ${AMAL_HH}..."
ALLCPPHEADERS="$SCRIPTPATH/cpp/roaring.hh $SCRIPTPATH/cpp/roaring64map.hh"
echo "/* auto-generated on ${timestamp}. Do not edit! */" > "${AMAL_HH}"
{
    echo "#include \"${AMAL_H}\""

    for h in ${ALLCPPHEADERS}; do
        dofile $h
    done
} >> "${AMAL_HH}"


DEMOCPP="amalgamation_demo.cpp"
echo "Creating ${DEMOCPP}..."
echo "/* auto-generated on ${timestamp}. Do not edit! */" > "${DEMOCPP}"
cat <<< '
#include <iostream>
#include "roaring.hh"
#include "roaring.c"
int main() {
  Roaring r1;
  for (uint32_t i = 100; i < 1000; i++) {
    r1.add(i);
  }
  std::cout << "cardinality = " << r1.cardinality() << std::endl;

  Roaring64Map r2;
  for (uint64_t i = 18000000000000000100ull; i < 18000000000000001000ull; i++) {
    r2.add(i);
  }
  std::cout << "cardinality = " << r2.cardinality() << std::endl;
  return 0;
}
' >>  "${DEMOCPP}"
echo "Done with C++."

echo "Done with all files generation. "

echo "Files have been written to current directory: $PWD "
ls -la ${AMAL_C} ${AMAL_H} ${AMAL_HH}  ${DEMOC} ${DEMOCPP}

echo "Giving final instructions:"


CBIN=${DEMOC%%.*}
CPPBIN=${DEMOCPP%%.*}

echo
echo "The interface is found in the file 'include/roaring/roaring.h'."
echo
echo "Try :"
echo "cc -march=native -O3 -std=c11  -o ${CBIN} ${DEMOC}  && ./${CBIN} "
echo
echo "For C++, try :"
echo "c++ -march=native -O3 -std=c++11 -o ${CPPBIN} ${DEMOCPP}  && ./${CPPBIN} "

lowercase(){
    echo "$1" | tr 'A-Z' 'a-z'
}

OS=`lowercase \`uname\``

echo
echo "You can build a shared library with the following command :"

if [ $OS == "darwin" ]; then
  echo "cc -march=native -O3 -std=c11 -shared -o libroaring.dylib -fPIC roaring.c"
else
  echo "cc -march=native -O3 -std=c11 -shared -o libroaring.so -fPIC roaring.c"
fi

echo "You can try compiling with the extra flags -DDISABLEAVX or -DDISABLE_X64 to disable AVX and all x64 optimization respectively."
