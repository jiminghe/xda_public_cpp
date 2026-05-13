#!/bin/bash
set -e

BUILD_DIR="build"

if [ "$1" = "clean" ]; then
    rm -rf "$BUILD_DIR"
    make -C xspublic clean
    echo "Cleaned build directory and xspublic artifacts."
    exit 0
fi

if [ "$1" = "rebuild" ]; then
    rm -rf "$BUILD_DIR"
    make -C xspublic clean
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake ..
make -j"$(nproc)"
