#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

cmake -B build-win \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/mingw.cmake" \
  -DLLL_CROSS_MINGW=ON \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-win -j
echo
echo "Built: build-win/lllc.exe"
