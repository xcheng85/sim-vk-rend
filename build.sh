#!/bin/bash
clear;

# check build dir
if [ ! -d build ] 
then 
    mkdir build 
fi

# generate cmake files
GIT_TRACE_PERFORMANCE=1 cmake -O . -B ./build -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=OFF

# copy assets files to output dir
cp -R "./resource"  "./build/bin/assets"

# compile project
cd build && cmake --build .  --config Debug -j4

# cmake --build . --target MyExe --config Debug

# cmake --build . --target MyExe -- -j4

echo compilation completed!