#!/usr/bin/env bash
set -e

BUILD_DIR="build"

if [ "$1" == "--clean" ]; then
    echo "removing existing build dir..."
    rm -rf "$BUILD_DIR"
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo "configuring..."
    cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

echo "building..."
ninja -C "$BUILD_DIR"
# qml -I "$BUILD_DIR" test.qml
echo "done."
