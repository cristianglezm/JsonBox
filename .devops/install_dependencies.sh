#!/bin/bash
# Builds GoogleTest into googletest/build/install for the CI jobs that run the tests.
# JsonBox itself has no other dependency.

PLATFORM=$1
BUILD_TYPE=$2

build_gtest() {
    # $@: extra cmake arguments for the platform
    git clone --depth 1 https://github.com/google/googletest
    cd googletest && mkdir build && cd build
    export GTest_ROOT=$(pwd)/install
    "${CMAKE_DRIVER[@]}" cmake -DCMAKE_INSTALL_PREFIX=$GTest_ROOT -DCMAKE_BUILD_TYPE=$BUILD_TYPE "$@" ..
    cmake --build . -j 4 -t install --config $BUILD_TYPE
    cd ../..
}

CMAKE_DRIVER=()

case "$PLATFORM" in
    Linux*)
        echo "building dependencies for Linux*"
        build_gtest -DCMAKE_CXX_FLAGS=-fPIC
    ;;
    "Windows MinGW"*)
        echo "building dependencies for Windows MinGW*"
        build_gtest -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -GNinja
    ;;
    "Windows LLVM"*)
        echo "building dependencies for Windows LLVM*"
        build_gtest -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -GNinja
    ;;
    Windows*)
        echo "building dependencies for Windows MSVC*"
        # the same C runtime as JsonBox, or the tests do not link
        build_gtest -GNinja -Dgtest_force_shared_crt=ON
    ;;
    mac*)
        echo "building dependencies for mac*"
        build_gtest
    ;;
    Emscripten*)
        echo "building dependencies for Emscripten*"
        git clone https://github.com/emscripten-core/emsdk.git
        cd emsdk && ./emsdk install latest && ./emsdk activate latest
        source emsdk_env.sh
        cd ..
        CMAKE_DRIVER=(emcmake)
        build_gtest -DCMAKE_CXX_FLAGS=-fexceptions
    ;;
    *)
      echo "usage:"
      echo "$0 <platform> <build_type>"
      echo "where platforms:"
      echo "    Linux"
      echo "    Windows LLVM"
      echo "    Windows MinGW"
      echo "    Windows"
      echo "    Mac"
      echo "    Emscripten"
      echo "and build_type:"
      echo "    Release"
      echo "    Debug"
    ;;
esac
