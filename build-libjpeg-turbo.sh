#!/bin/sh

set -e

export CFLAGS="-mmacosx-version-min=10.11 -Wall"

CMAKE_BUILD_TYPE=Release
NUMCPUS=$(sysctl -n hw.ncpu)

# Get submodule libjepg-turbo.
git submodule update --init libjpeg-turbo

cd libjpeg-turbo

mkdir -p macosarmv8
pushd macosarmv8
cmake -mmacosx-version-min=10.11 .. -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_OSX_ARCHITECTURES=arm64
make -j $NUMCPUS
popd

mkdir -p macosx8664
pushd macosx8664
cmake -mmacosx-version-min=10.11 .. -DARMV8_BUILD=../macosarmv8 -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_OSX_ARCHITECTURES=x86_64
make -j $NUMCPUS
popd

ln -sf macosarmv8/jconfig.h .
