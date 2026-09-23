#include <catch2/catch_test_macros.hpp>
#include "snip/test_parser.hpp"

TEST_CASE("TestParser collapses passing pytest into single summary line", "[test_parser]") {
    snip::TestParser parser;
    std::vector<std::string> args = {"pytest"};

    std::string raw = 
        "============================= test session starts ==============================\n"
        "platform darwin -- Python 3.11.0, pytest-7.4.0\n"
        "rootdir: /Users/abhi/Desktop/Projects/snip\n"
        "collected 4 items\n\n"
        "tests/test_auth.py ....                                                  [100%]\n\n"
        "============================== 4 passed in 0.05s ===============================\n";

    auto res = parser.parse(args, raw, 0);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("✓") != std::string_view::npos);
    REQUIRE(res.text.find("4 passed in 0.05s") != std::string_view::npos);
    REQUIRE(res.text.find("platform darwin") == std::string_view::npos);
}

TEST_CASE("TestParser extracts failure assertion and hides passing noise in pytest", "[test_parser]") {
    snip::TestParser parser;
    std::vector<std::string> args = {"pytest"};

    std::string raw = 
        "============================= test session starts ==============================\n"
        "tests/test_auth.py .F..\n\n"
        "=================================== FAILURES ===================================\n"
        "_________________________________ test_login ___________________________________\n"
        "tests/test_auth.py:22: in test_login\n"
        ">       assert status_code == 200\n"
        "E       assert 401 == 200\n"
        "=========================== short test summary info ============================\n"
        "FAILED tests/test_auth.py::test_login - assert 401 == 200\n"
        "========================= 1 failed, 3 passed in 0.10s ==========================\n";

    auto res = parser.parse(args, raw, 1);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("assert 401 == 200") != std::string_view::npos);
    REQUIRE(res.text.find("FAILED tests/test_auth.py::test_login") != std::string_view::npos);
    REQUIRE(res.text.find("test session starts") == std::string_view::npos);
}

TEST_CASE("TestParser handles passing cargo test", "[test_parser]") {
    snip::TestParser parser;
    std::vector<std::string> args = {"cargo", "test"};

    std::string raw = 
        "   Compiling snip v0.1.0\n"
        "    Finished test [unoptimized + debuginfo] target(s) in 0.42s\n"
        "     Running unittests src/main.rs (target/debug/deps/snip-1234)\n\n"
        "running 3 tests\n"
        "test tests::test_parse ... ok\n"
        "test tests::test_bpe ... ok\n"
        "test tests::test_dedup ... ok\n\n"
        "test result: ok. 3 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out\n";

    auto res = parser.parse(args, raw, 0);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("✓") != std::string_view::npos);
    REQUIRE(res.text.find("test_parse ... ok") == std::string_view::npos);
}