cmake_minimum_required(VERSION 3.22)
project(snip VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    set(SNIP_WARN_FLAGS -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

include(FetchContent)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    SYSTEM
)
FetchContent_MakeAvailable(tomlplusplus)

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
    src/bpe.cpp
    src/search_parser.cpp
    src/file_parser.cpp
    src/stats.cpp
    src/mcp_server.cpp
    src/fallback_engine.cpp
    src/embedded_rules.cpp
    src/rule_engine.cpp
)
target_link_libraries(snip_core PUBLIC tomlplusplus::tomlplusplus)
target_compile_options(snip_core PRIVATE ${SNIP_WARN_FLAGS})

add_executable(snip src/main.cpp)
target_link_libraries(snip PRIVATE snip_core)
target_compile_options(snip PRIVATE ${SNIP_WARN_FLAGS})

enable_testing()
add_subdirectory(tests)