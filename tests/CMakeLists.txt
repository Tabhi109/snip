include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
    SYSTEM
)
FetchContent_MakeAvailable(Catch2)

add_executable(snip_tests
    test_main.cpp
    test_sanitizer.cpp
    test_dedup.cpp
    test_git_parser.cpp
    test_bpe.cpp
    test_test_parser.cpp
    test_search_parser.cpp
    test_fallback_engine.cpp
    test_rule_engine.cpp
    test_fixtures.cpp
    test_stats.cpp
    test_mcp.cpp
    test_pipe.cpp
)

target_link_libraries(snip_tests PRIVATE snip_core Catch2::Catch2)
target_compile_definitions(snip_tests PRIVATE 
    SNIP_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    SNIP_EXECUTABLE_PATH="$<TARGET_FILE:snip>"
)
target_compile_options(snip_tests PRIVATE ${SNIP_WARN_FLAGS})

include(CTest)
include(Catch)
catch_discover_tests(snip_tests)