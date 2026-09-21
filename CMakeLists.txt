cmake_minimum_required(VERSION 3.22)
project(snip VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        -O2
    )
endif()

include_directories(include)

add_library(snip_core STATIC
    src/sanitizer.cpp
    src/runner.cpp
    src/cache.cpp
    src/dedup.cpp
    src/git_parser.cpp
    src/test_parser.cpp
    src/dispatcher.cpp
    src/init.cpp
)

add_executable(snip src/main.cpp)
target_link_libraries(snip PRIVATE snip_core)