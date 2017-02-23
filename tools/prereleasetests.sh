#!/bin/bash
set -e
MAINDIR=$( git rev-parse --show-toplevel)
echo $MAINDIR
echo "cleaning main directory"
rm -r -f $MAINDIR/CMakeFiles 
rm -f $MAINDIR/CMakeCache.txt

# the directory of the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# the temp directory used, within $DIR
WORK_DIR=`mktemp -d `

RED='\033[0;31m'
BLUE='\034[0;31m'
NC='\033[0m' 


# deletes the temp directory
function cleanup {
  rm -rf "$WORK_DIR"
  echo "Deleted temp working directory $WORK_DIR"
}

function err_report() {
    echo -e "${RED}You have some error.${NC}"
}

trap err_report ERR

# register the cleanup function to be called on the EXIT signal
trap cleanup exit

cd $WORK_DIR

echo "working in " $PWD


echo "testing debug (static)"
cd $WORK_DIR  
mkdir debugstatic 
cd debugstatic
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC=ON $MAINDIR 
make 
make test

echo "testing debug no x64 (static)"
cd $WORK_DIR
mkdir debugnox64static 
cd debugnox64static 
cmake -DDISABLE_X64=ON -DBUILD_STATIC=ON -DCMAKE_BUILD_TYPE=Debug $MAINDIR 
make 
make test

echo "testing release"
cd $WORK_DIR 
mkdir releasestatic 
cd releasestatic  
cmake -DBUILD_STATIC=ON $MAINDIR 
make 
make test

echo "testing release no x64 (static)"
cd $WORK_DIR 
mkdir releasenox64static 
cd releasenox64static
cmake -DDISABLE_X64=ON -DBUILD_STATIC=ON $MAINDIR  
make 
make test



echo "testing debug"
cd $WORK_DIR  
mkdir debug 
cd debug 
cmake -DCMAKE_BUILD_TYPE=Debug  $MAINDIR 
make 
make test

echo "testing debug no x64"
cd $WORK_DIR
mkdir debugnox64 
cd debugnox64 
cmake -DDISABLE_X64=ON -DCMAKE_BUILD_TYPE=Debug $MAINDIR 
make 
make test

echo "testing release"
cd $WORK_DIR 
mkdir release 
cd release 
cmake $MAINDIR 
make 
make test

echo "testing release no x64"
cd $WORK_DIR 
mkdir releasenox64 
cd releasenox64 
cmake -DDISABLE_X64=ON $MAINDIR  
make 
make test

echo -e "${BLUE}Code looks good.${NC}"

