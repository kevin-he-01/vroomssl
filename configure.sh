#!/bin/bash

export CC=/opt/llvm21/bin/clang
export CXX=/opt/llvm21/bin/clang++

cmake -GNinja -B build -DCMAKE_BUILD_TYPE=Release
cmake -GNinja -B build.debug -DCMAKE_BUILD_TYPE=Debug -DQUICK_COMPILE_MODE=ON
