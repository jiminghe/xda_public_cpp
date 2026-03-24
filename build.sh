#!/bin/bash
set -e

BUILD_DIR="build"

if [ "$1" = "clean" ]; then
    rm -rf "$BUILD_DIR"
    echo "Cleaned build directory."
    exit 0
fi

if [ "$1" = "rebuild" ]; then
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake ..
make -j"$(nproc)"
