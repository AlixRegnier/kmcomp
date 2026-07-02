#!/bin/bash

mkdir -p build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release -DKMCOMP_BUILD_MAIN=true -DKMCOMP_METRICS=false
make -j

cd -
