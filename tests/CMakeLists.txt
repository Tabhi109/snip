include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
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
)

target_link_libraries(snip_tests PRIVATE snip_core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(snip_tests)