#!/usr/bin/env bash
set -e
set -o noglob
COMMAND=$@
SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
MAINSOURCE=$SCRIPTPATH/..
ALL_CROARING_FILES=$(cd $SCRIPTPATH/.. && git ls-tree --full-tree --name-only -r HEAD | grep -e ".*\.\(c\|h\|cc\|cpp\|hh\)\$" | grep -vFf clang-format-ignore.txt)
tuser=$(echo $USER | tr -dc 'a-z')

container_name=${CONTAINER_NAME:-"clang-format-for-$tuser"}
echo $container_name

command -v docker >/dev/null 2>&1 || { echo >&2 "Please install docker. E.g., go to https://www.docker.com/products/docker-desktop Type 'docker' to diagnose the problem."; exit 1; }

docker info >/dev/null 2>&1 || { echo >&2 "Docker server is not running? type 'docker info'."; exit 1; }

docker image inspect $container_name >/dev/null 2>&1 || ( echo "instantiating the container" ; docker build --no-cache -t $container_name -f $SCRIPTPATH/clangcldockerfile --build-arg USER_NAME="$tuser"  --build-arg USER_ID=$(id -u)  --build-arg GROUP_ID=$(id -g) .  )

if [ -t 0 ]; then DOCKER_ARGS=-it; fi
docker run --rm $DOCKER_ARGS -h $container_name -v $MAINSOURCE:$MAINSOURCE:Z  -w $MAINSOURCE $container_name sh --style=file --verbose -i $ALL_CROARING_FILES $COMMAND



