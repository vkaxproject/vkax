#!/usr/bin/env bash

export LC_ALL=C

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/.. || exit

DOCKER_IMAGE=${DOCKER_IMAGE:-jgcproject/jgcd-develop}
DOCKER_TAG=${DOCKER_TAG:-latest}

BUILD_DIR=${BUILD_DIR:-.}

rm docker/bin/*
mkdir docker/bin
cp $BUILD_DIR/src/jgcd docker/bin/
cp $BUILD_DIR/src/jgc-cli docker/bin/
cp $BUILD_DIR/src/jgc-tx docker/bin/
strip docker/bin/jgcd
strip docker/bin/jgc-cli
strip docker/bin/jgc-tx

docker build --pull -t $DOCKER_IMAGE:$DOCKER_TAG -f docker/Dockerfile docker
