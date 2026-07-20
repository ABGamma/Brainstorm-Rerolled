cmake_minimum_required(VERSION 3.10)
project(Immolate LANGUAGES CXX)

# Set C++ standard and required flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define sources for the library
set(LIB_SOURCES
    src/functions.hpp
    src/instance.hpp
    src/immolate.cpp
    src/items.cpp
    src/rng.cpp
    src/search.cpp
    src/seed.cpp
    src/util.cpp
    src/functions.cpp
)

# Define sources for the executable
set(EXECUTABLE_SOURCES
    src/main.cpp
)

# Set optimization flags based on the compiler
if(MSVC)
    set(OPTIMIZATION_FLAGS "/Ox")  # Full optimization for MSVC
else()
    # -ffp-contract=off is required for correctness, not just consistency:
    # clang (aarch64) and gcc contract a*b+c into fused multiply-adds by
    # default, which changes the low bits of the doubles used by the seed RNG
    # and makes the search disagree with the game.
    set(OPTIMIZATION_FLAGS "-O3 -g -std=c++20 -ffp-contract=off -Wall -Wextra -Wpedantic -Wno-c++17-extensions -gdwarf-3 -ldl")  # Full optimization for GCC/Clang, plus debugging info
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPTIMIZATION_FLAGS}")

# Note: fast-math must stay off on every platform. The seed search relies on
# exactly reproducing the game's IEEE-754 double arithmetic, and
# -ffp-model=fast is allowed to reorder/contract float ops, which silently
# breaks seed matching. (The previous ARM-only block also used the Intel
# compiler spelling -fp-model=fast, which clang rejects outright.)

# Output compile_commands.json for clangd stuff
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCES})

# Define BUILDING_DLL when compiling the DLL
target_compile_definitions(${PROJECT_NAME} PRIVATE BUILDING_DLL)

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_NAME "${PROJECT_NAME}"
    ARCHIVE_OUTPUT_NAME "${PROJECT_NAME}"
)

# Generate export header
include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME}
    BASE_NAME IMMOLATE
    EXPORT_MACRO_NAME IMMOLATE_API
    EXPORT_FILE_NAME ImmolateExport.h
    STATIC_DEFINE IMMOLATE_BUILT_AS_STATIC
)

add_executable(immol ${EXECUTABLE_SOURCES})
target_link_libraries(immol ${PROJECT_NAME})