# Immolate — native seed searcher for the Brainstorm Balatro mod.
#
# Produces:
#   Immolate.dll    (Windows, also used by Linux/Steam Deck players via Proton)
#   Immolate.dylib  (macOS, universal: Apple Silicon + Intel)
#   golden          test program, run with `ctest` (see tests/golden.cpp)
#   immol           small command-line searcher used during development
#
# Quick start:   cmake -B build -DCMAKE_BUILD_TYPE=Release
#                cmake --build build --config Release
#                ctest --test-dir build -C Release --output-on-failure
cmake_minimum_required(VERSION 3.16)

# Must be set before project() to take effect.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
# Static MSVC runtime: the DLL then works without the Visual C++
# Redistributable, which fresh Proton prefixes usually lack.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Immolate VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Floating-point exactness
#
# Balatro's RNG is built on double-precision arithmetic, and a seed only
# predicts the real game if every operation rounds exactly like LuaJIT does.
# Two compiler optimisations silently break that, so both are disabled:
#   * fast-math (-ffast-math, /fp:fast) reorders and approximates operations.
#   * FMA contraction fuses a*b+c into one instruction with a single rounding.
#     It is ON by default for Clang on Apple Silicon (arm64 has FMA), which
#     would make the Mac build disagree with the Windows build.
# ---------------------------------------------------------------------------
add_library(immolate_fp INTERFACE)
if(MSVC)
  target_compile_options(immolate_fp INTERFACE /fp:precise)
else()
  target_compile_options(immolate_fp INTERFACE -ffp-contract=off -fno-fast-math)
endif()

set(CORE_SOURCES
  src/functions.cpp
  src/items.cpp
  src/rng.cpp
  src/seed.cpp
  src/util.cpp
  src/filters.cpp
  src/brainstorm_api.cpp
  src/prefilter.cpp
  src/scan.cpp
  src/fastscan.cpp
  src/filterprog.cpp
)

# Core logic as a static library, shared by the mod library and the tests.
add_library(immolate_core STATIC ${CORE_SOURCES})
target_include_directories(immolate_core PUBLIC src)
target_link_libraries(immolate_core PUBLIC immolate_fp Threads::Threads)
target_compile_definitions(immolate_core PUBLIC
  IMMOLATE_VERSION="${PROJECT_VERSION}")
set_target_properties(immolate_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  # Internal functions stay private to the library; only BS_API functions
  # (brainstorm_api.h) are exported.
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)

# The library the mod loads.
add_library(Immolate SHARED src/immolate.cpp)
target_link_libraries(Immolate PRIVATE immolate_core)
set_target_properties(Immolate PROPERTIES
  PREFIX ""                       # Immolate.dylib, not libImmolate.dylib
  CXX_VISIBILITY_PRESET hidden    # export only the BS_API functions
  VISIBILITY_INLINES_HIDDEN ON
)
if(APPLE)
  set_target_properties(Immolate PROPERTIES SUFFIX ".dylib")
endif()
if(MINGW)
  # Bake the GCC runtime into the DLL so it has no extra DLL dependencies.
  target_link_options(Immolate PRIVATE -static -static-libgcc -static-libstdc++ -s)
endif()

# Development CLI (src/main.cpp has its own hard-coded filter).
add_executable(immol src/main.cpp)
target_link_libraries(immol PRIVATE immolate_fp Threads::Threads)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(golden tests/golden.cpp)
target_link_libraries(golden PRIVATE immolate_core)

# Full golden comparison: output must match tests/golden_expected.txt.
add_test(NAME golden
  COMMAND ${CMAKE_COMMAND}
    -DGOLDEN=$<TARGET_FILE:golden>
    "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
    -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/golden_expected.txt
    -DOUT=${CMAKE_CURRENT_BINARY_DIR}/golden_actual.txt
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/compare.cmake)

# API behaviour: known seeds, key lookup, errors, cancel, wrap-around.
add_executable(api_test tests/api_test.cpp src/immolate.cpp)
target_link_libraries(api_test PRIVATE immolate_core)
add_test(NAME api COMMAND api_test)

# Performance benchmark (not a test): `bench` or `bench threads`.
add_executable(bench tests/bench.cpp)
target_link_libraries(bench PRIVATE immolate_core)

# Fast-path maths (fract, round13) must match the original bit for bit.
add_executable(math_test tests/math_test.cpp)
target_link_libraries(math_test PRIVATE immolate_core)
add_test(NAME math COMMAND math_test)

# Batched first check (prefilter) must match the normal engine exactly.
add_executable(prefilter_test tests/prefilter_test.cpp)
target_link_libraries(prefilter_test PRIVATE immolate_core)
add_test(NAME prefilter COMMAND prefilter_test)

# Bean / Burglar / Souls shortcuts == the full generators (src/fastscan.cpp).
add_executable(fastscan_test tests/fastscan_test.cpp)
target_link_libraries(fastscan_test PRIVATE immolate_core)
add_test(NAME fastscan COMMAND fastscan_test)

# Custom filters: parser, fast check == reference meaning, search (src/filterprog.cpp).
add_executable(filterprog_test tests/filterprog_test.cpp)
target_link_libraries(filterprog_test PRIVATE immolate_core)
add_test(NAME filterprog COMMAND filterprog_test)

# Boss blinds vs the real game and the game's rules (src/functions.hpp).
add_executable(boss_test tests/boss_test.cpp)
target_link_libraries(boss_test PRIVATE immolate_core)
add_test(NAME boss COMMAND boss_test)

# MinGW (Windows cross-builds): link the C++ runtime into every test program
# so the .exe files run under Wine/Windows without extra DLLs.
if(MINGW)
  foreach(t golden api_test math_test prefilter_test fastscan_test filterprog_test boss_test bench)
    target_link_options(${t} PRIVATE -static)
  endforeach()
endif()
