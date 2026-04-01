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
