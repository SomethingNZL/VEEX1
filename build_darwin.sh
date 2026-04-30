#!/bin/bash
# build_darwin.sh — Build VEEXEngine for macOS (Darwin)
# Usage: ./build_darwin.sh [Release|Debug|RelWithDebInfo]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${1:-Release}"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "========================================"
echo "  VEEXEngine macOS Build"
echo "  Build type: ${BUILD_TYPE}"
echo "========================================"

# macOS dependencies (install via MacPorts):
#   sudo port install glfw glm openssl

mkdir -p "${BUILD_DIR}"

cmake \
    -B "${BUILD_DIR}" \
    -S "${SCRIPT_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_PREFIX_PATH="/opt/local"

cmake --build "${BUILD_DIR}" --parallel "$(sysctl -n hw.ncpu)"

echo ""
echo "========================================"
echo "  Build complete!"
echo "  Output: ${BUILD_DIR}/Engine/VEEXEngine"
echo "========================================"
