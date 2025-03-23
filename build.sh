#!/bin/bash

# Exit on error
set -e

# Create build directory if it doesn't exist
mkdir -p build

# Build the project
cd build
cmake ../
cmake --build . -j$(nproc)

echo "Build completed successfully!"