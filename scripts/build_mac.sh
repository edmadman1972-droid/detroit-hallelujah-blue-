#!/usr/bin/env bash
# Build + package on a Mac. Needs Xcode (or the Command Line Tools) and CMake 3.22+.
#   brew install cmake        # if you don't have CMake yet
#   bash scripts/build_mac.sh
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
bash scripts/package_mac.sh build Release
echo "Done. Open dist/ and run the .pkg installer, or copy the bundles yourself."
