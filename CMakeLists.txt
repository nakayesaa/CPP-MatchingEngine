# CMakeLists.txt
# ─────────────────────────────────────────────────────────────────────
# Build configuration for the matching engine.
#
# Nothing fancy here – it's a single executable built from three
# translation units. C++17 is required for structured bindings
# (used in printBook) and other modern conveniences.
#
# Optimization is set to -O2 (or /O2 on MSVC) because the whole point
# of this project is measuring throughput, so we want the compiler
# to actually try. Warnings are cranked up (/W4, -Wall -Wextra)
# to catch dumb mistakes early.
#
# To build:
#   cmake -B build && cmake --build build --config Release
# ─────────────────────────────────────────────────────────────────────

cmake_minimum_required(VERSION 3.16)
project(MatchingEngine LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(matching_engine
    src/OrderBook.cpp
    src/MarketSimulator.cpp
    src/main.cpp
)
target_include_directories(matching_engine PRIVATE include)

if(MSVC)
    target_compile_options(matching_engine PRIVATE /O2 /W4)
else()
    target_compile_options(matching_engine PRIVATE -O2 -Wall -Wextra)
endif()
