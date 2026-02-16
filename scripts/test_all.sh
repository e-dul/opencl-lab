#!/bin/bash
# Run all tests

set -e

echo "🧪 Running tests..."

cd build
ctest --output-on-failure

echo "✅ Tests complete!"
