#!/bin/bash

set -e

cd build

echo "Running CMake..."
cmake ..

echo "Building project..."
cmake --build .

echo "Running game..."
./doofus