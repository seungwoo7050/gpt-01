#!/bin/bash
source ecs-realm-server/conanbuild.sh
cmake -S ecs-realm-server -B ecs-realm-server/build -DCMAKE_TOOLCHAIN_FILE=ecs-realm-server/conan_toolchain.cmake
make -C ecs-realm-server/build -j
