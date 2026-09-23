#include <catch2/catch_test_macros.hpp>
#include "snip/git_parser.hpp"

TEST_CASE("GitParser compresses verbose status into semantic format", "[git]") {
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
    // The new parser emits "M src/main.cpp", not "modified: src/main.cpp"
    REQUIRE(res.text.find("M src/main.cpp") != std::string::npos);
    REQUIRE(res.text.find("## main") != std::string::npos);
}