#include <catch2/catch_test_macros.hpp>
#include "snip/search_parser.hpp"

TEST_CASE("SearchParser groups grep/rg matches hierarchically by file", "[search]") {
    snip::SearchParser parser;
    std::vector<std::string> args = {"rg", "BPETokenizer"};

    std::string raw =
        "src/bpe.cpp:12:BPETokenizer::BPETokenizer() {\n"
        "src/bpe.cpp:45:std::vector<std::string_view> split_into_chunks\n"
        "include/snip/bpe.hpp:14:class BPETokenizer {\n"
        "include/snip/bpe.hpp:16:    BPETokenizer();\n";

    auto res = parser.parse(args, raw, 0);

    REQUIRE(res.was_compressed == true);
    // Files appear as headers
    REQUIRE(res.text.find("src/bpe.cpp\n") != std::string::npos);
    REQUIRE(res.text.find("include/snip/bpe.hpp\n") != std::string::npos);
    // Line numbers are indented under headers
    REQUIRE(res.text.find("  12: BPETokenizer::BPETokenizer() {") != std::string::npos);
    REQUIRE(res.text.find("  14: class BPETokenizer {") != std::string::npos);
}

TEST_CASE("SearchParser caps results when match count exceeds threshold", "[search]") {
    snip::SearchParser parser;
    std::vector<std::string> args = {"rg", "test"};

    std::string raw;
    for (int i = 1; i <= 35; ++i) {
        raw += "tests/test.cpp:" + std::to_string(i) + ": void test_fn_" + std::to_string(i) + "();\n";
    }

    auto res = parser.parse(args, raw, 0);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("[+10 more matches suppressed]") != std::string::npos);
}