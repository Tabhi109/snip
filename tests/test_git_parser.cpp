#include <catch2/catch_test_macros.hpp>
#include "snip/git_parser.hpp"

TEST_CASE("GitParser strips verbose guidance hints", "[git]") {
    snip::GitParser parser;
    std::vector<std::string> args = {"git", "status"};
    
    std::string raw = 
        "On branch main\n"
        "Changes not staged for commit:\n"
        "  (use \"git add <file>...\" to update what will be committed)\n"
        "  (use \"git restore <file>...\" to discard changes in working directory)\n"
        "\tmodified:   src/main.cpp\n";

    auto res = parser.parse(args, raw, 0);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("(use \"git add") == std::string::npos);
    REQUIRE(res.text.find("modified:   src/main.cpp") != std::string::npos);
}