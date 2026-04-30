#!/bin/bash
# build_windows.sh — Cross-compile VEEXEngine for Windows 10/11 using MinGW-w64
# Usage: ./build_windows.sh [Release|Debug|RelWithDebInfo]
#
# Prerequisites:
#   1. MinGW-w64 (install via MacPorts):
#        sudo port install mingw-w64
#
#   2. Windows dependencies (vcpkg):
#        git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
#        cd ~/vcpkg && ./bootstrap-vcpkg.sh
#        ./vcpkg install glfw3:x64-mingw-dynamic
#        ./vcpkg install glm:x64-mingw-dynamic
#        ./vcpkg install openssl:x64-mingw-dynamic

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${1:-Release}"
BUILD_DIR="${SCRIPT_DIR}/build_win"

echo "========================================"
echo "  VEEXEngine Windows Cross-Build"
echo "  Build type: ${BUILD_TYPE}"
echo "========================================"

# Verify MinGW compiler exists
if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "ERROR: MinGW-w64 compiler not found!"
    echo "Install it with:  sudo port install mingw-w64"
    exit 1
fi

# Verify toolchain file exists
TOOLCHAIN_FILE="${SCRIPT_DIR}/mingw_toolchain.cmake"
if [ ! -f "${TOOLCHAIN_FILE}" ]; then
    echo "ERROR: Toolchain file not found: ${TOOLCHAIN_FILE}"
    exit 1
fi

# Auto-detect vcpkg root
if [ -z "${VCPKG_ROOT}" ]; then
    if [ -d "${HOME}/vcpkg" ]; then
        export VCPKG_ROOT="${HOME}/vcpkg"
    fi
fi

mkdir -p "${BUILD_DIR}"

cmake \
    -B "${BUILD_DIR}" \
    -S "${SCRIPT_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build "${BUILD_DIR}" --parallel "$(sysctl -n hw.ncpu)"

echo ""
echo "========================================"
echo "  Build complete!"
echo "  Output: ${BUILD_DIR}/Engine/VEEXEngine.exe"
echo "========================================"
