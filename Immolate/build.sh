#!/usr/bin/env bash
# Builds the native library and runs the tests (macOS / Linux).
# Output: build/Immolate.dylib (macOS) or build/Immolate.so (Linux).
# Copy that file into your Brainstorm mod folder.
set -euo pipefail
cd "$(dirname "$0")"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
