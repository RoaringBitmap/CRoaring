#!/bin/bash
########################################################################
# Generates an "amalgamation build" for roaring. Inspired by similar
# script used by whefs.
########################################################################

echo "We are about to amalgamate all CRoaring files into one source file. "
echo "See https://www.sqlite.org/amalgamation.html for rationale. "

AMAL_H="roaring.h"
AMAL_C="roaring.c"

# order does not matter
ALLCFILES=$(find src -name '*.c' )

# order matters
ALLCHEADERS="
include/roaring/portability.h
include/roaring/array_util.h
include/roaring/roaring_types.h
include/roaring/roaring_array.h
include/roaring/utilasm.h
include/roaring/roaring.h
include/roaring/bitset_util.h
include/roaring/containers/array.h
include/roaring/containers/bitset.h
include/roaring/containers/run.h
include/roaring/containers/convert.h
include/roaring/containers/mixed_equal.h
include/roaring/containers/mixed_andnot.h
include/roaring/containers/mixed_intersection.h
include/roaring/containers/mixed_negation.h
include/roaring/containers/mixed_union.h
include/roaring/containers/mixed_xor.h
include/roaring/containers/containers.h
include/roaring/containers/perfparameters.h
include/roaring/misc/configreport.h
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
    echo "#line 8 \"$1\""
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
    echo "#line 1 \"${AMAL_C}\""
    echo "#include \"${AMAL_H}\""

    for h in ${ALLCFILES}; do
        dofile $h
    done
} >> "${AMAL_C}"




DEMOC="almagamation_demo.c"
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
ALLCPPHEADERS="cpp/roaring.hh"
echo "/* auto-generated on ${timestamp}. Do not edit! */" > "${AMAL_HH}"
{
    echo "#include \"${AMAL_H}\""
    for h in ${ALLCPPHEADERS}; do
        dofile $h
    done
} >> "${AMAL_HH}"


DEMOCPP="almagamation_demo.cpp"
echo "Creating ${DEMOCPP}..."
echo "/* auto-generated on ${timestamp}. Do not edit! */" > "${DEMOCPP}"
cat <<< '
#include <iostream>
#include "roaring.hh"
int main() {
  Roaring r1;
  for (uint32_t i = 100; i < 1000; i++) {
    r1.add(i);
  }
  std::cout << "cardinality = " << r1.cardinality() << std::endl;
  return 0;
}
' >>  "${DEMOCPP}"

ls -la ${AMAL_C} ${AMAL_H} ${AMAL_HH}  ${DEMOC} ${DEMOCPP}


echo "Done with C++."

echo "Done with all files generation, giving final instructions: "

CBIN=${DEMOC%%.*}
CPPBIN=${DEMOCPP%%.*}

echo
echo "Try :"
echo "cc -march=native -O3 -std=c11  -o ${CBIN} ${DEMOC} -Wshadow -Wextra -pedantic && ./${CBIN} "
echo
echo "For C++, try :"
echo "cc -march=native -O3 -std=c11  -c ${AMAL_C} -Wshadow -Wextra -pedantic -flto && c++ -march=native -O3 -std=c++11 -o ${CPPBIN} ${DEMOCPP} ${AMAL_C%%.*}.o -Wshadow -Wextra -pedantic  -flto && ./${CPPBIN} "
