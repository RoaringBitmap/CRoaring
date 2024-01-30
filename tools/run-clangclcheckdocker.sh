#!/usr/bin/env bash

set -o noglob
SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
MAINSOURCE=$SCRIPTPATH/..

ALL_CROARING_FILES="
$MAINSOURCE/include/roaring/roaring_version.h
$MAINSOURCE/include/roaring/roaring_types.h
$MAINSOURCE/include/roaring/portability.h
$MAINSOURCE/include/roaring/bitset/bitset.h
$MAINSOURCE/include/roaring/roaring.h
$MAINSOURCE/include/roaring/memory.h
$MAINSOURCE/include/roaring/roaring64.h
$MAINSOURCE/cpp/roaring.hh
$MAINSOURCE/cpp/roaring64map.hh
$MAINSOURCE/include/roaring/isadetection.h
$MAINSOURCE/include/roaring/containers/perfparameters.h
$MAINSOURCE/include/roaring/containers/container_defs.h
$MAINSOURCE/include/roaring/array_util.h
$MAINSOURCE/include/roaring/utilasm.h
$MAINSOURCE/include/roaring/bitset_util.h
$MAINSOURCE/include/roaring/containers/array.h
$MAINSOURCE/include/roaring/containers/bitset.h
$MAINSOURCE/include/roaring/containers/run.h
$MAINSOURCE/include/roaring/containers/convert.h
$MAINSOURCE/include/roaring/containers/mixed_equal.h
$MAINSOURCE/include/roaring/containers/mixed_subset.h
$MAINSOURCE/include/roaring/containers/mixed_andnot.h
$MAINSOURCE/include/roaring/containers/mixed_intersection.h
$MAINSOURCE/include/roaring/containers/mixed_negation.h
$MAINSOURCE/include/roaring/containers/mixed_union.h
$MAINSOURCE/include/roaring/containers/mixed_xor.h
$MAINSOURCE/include/roaring/containers/containers.h
$MAINSOURCE/include/roaring/roaring_array.h
$MAINSOURCE/include/roaring/art/art.h
"

tuser=$(echo $USER | tr -dc 'a-z')

container_name=${CONTAINER_NAME:-"clang-format-for-$tuser"}
echo $container_name

command -v docker >/dev/null 2>&1 || { echo >&2 "Please install docker. E.g., go to https://www.docker.com/products/docker-desktop Type 'docker' to diagnose the problem."; exit 1; }

docker info >/dev/null 2>&1 || { echo >&2 "Docker server is not running? type 'docker info'."; exit 1; }

docker image inspect $container_name >/dev/null 2>&1 || ( echo "instantiating the container" ; docker build --no-cache -t $container_name -f $SCRIPTPATH/clangcldockerfile --build-arg USER_NAME="$tuser"  --build-arg USER_ID=$(id -u)  --build-arg GROUP_ID=$(id -g) .  )

if [ -t 0 ]; then DOCKER_ARGS=-it; fi
docker run --rm $DOCKER_ARGS -h $container_name -v $MAINSOURCE:$MAINSOURCE:Z  -w $MAINSOURCE  $container_name sh --style=file --verbose --dry-run -i $ALL_CROARING_FILES



