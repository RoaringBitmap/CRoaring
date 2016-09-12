#!/bin/bash
set -e
MAINDIR=$( git rev-parse --show-toplevel)
echo $MAINDIR
# the directory of the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# the temp directory used, within $DIR
WORK_DIR=`mktemp -d -p "$DIR"`

# deletes the temp directory
function cleanup {
  rm -rf "$WORK_DIR"
  echo "Deleted temp working directory $WORK_DIR"
}

# register the cleanup function to be called on the EXIT signal
trap cleanup exit

cd $WORK_DIR

echo "working in " $PWD

echo "testing debug"
set -x          # activate debugging from here
mkdir -p debug && cd debug && cmake -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON $MAINDIR && make && make test
set +x

echo "testing debug no x64"
set -x          # activate debugging from here
mkdir -p debugnox64 && cd debugnox64 && cmake -DDISABLE_X64=ON -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON $MAINDIR && make && make test
set +x

echo "testing release"
set -x          # activate debugging from here
mkdir -p release && cd release && cmake $MAINDIR && make && make test
set +x

echo "testing release no x64"
set -x          # activate debugging from here
mkdir -p releasenox64 && cd releasenox64 && cmake -DDISABLE_X64=ON $MAINDIR  && make && make test
set +x

