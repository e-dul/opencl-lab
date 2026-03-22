#!/bin/bash
# Build all modules

set -e

echo "🔨 Building project..."

# Configure with default options (Mod 0 + 1)
cmake -B build     -DBUILD_MODULE_0=ON     -DBUILD_MODULE_1=ON     -DBUILD_MODULE_2=OFF     -DBUILD_TOOLBOX=OFF     -DBUILD_ADDONS=OFF

cmake --build build -j$(nproc)

echo "✅ Build complete!"
