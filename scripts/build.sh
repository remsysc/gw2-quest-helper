#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "==> Running native C++ unit tests..."
cmake -G Ninja -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-linux" \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "${ROOT_DIR}/build-linux"
ctest --test-dir "${ROOT_DIR}/build-linux" --output-on-failure

echo "==> Cross-compiling GW2QuestHelper.dll (Windows x64 MSVC ABI)..."
cmake -G Ninja -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-win64" \
    -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/cmake/toolchain-msvc-clang.cmake" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "${ROOT_DIR}/build-win64"

echo "==> Built: ${ROOT_DIR}/build-win64/GW2QuestHelper.dll"
