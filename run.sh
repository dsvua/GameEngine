#!/bin/bash

# Exit on error
set -e

# Enable Vulkan validation layers
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

# Check if the executable exists
if [ ! -f "./bin/GameEngine" ]; then
    echo "Executable not found. Building project..."
    ./build.sh
fi

# Run the application
echo "Starting AIGameEngine - CTRL+C to exit..."
# Run with unlimited time
(cd bin && ./GameEngine)