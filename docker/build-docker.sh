#!/usr/bin/env bash

export LC_ALL=C

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/.. || exit

DOCKER_IMAGE=${DOCKER_IMAGE:-vkaxproject/vkaxd-develop}
DOCKER_TAG=${DOCKER_TAG:-latest}

BUILD_DIR=${BUILD_DIR:-.}

rm docker/bin/*
mkdir docker/bin
cp $BUILD_DIR/src/vkaxd docker/bin/
cp $BUILD_DIR/src/vkax-cli docker/bin/
cp $BUILD_DIR/src/vkax-tx docker/bin/
strip docker/bin/vkaxd
strip docker/bin/vkax-cli
strip docker/bin/vkax-tx

docker build --pull -t $DOCKER_IMAGE:$DOCKER_TAG -f docker/Dockerfile docker
