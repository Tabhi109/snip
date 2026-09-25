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
    REQUIRE(res.text.find("M src/main.cpp") != std::string::npos);
    REQUIRE(res.text.find("## main") != std::string::npos);
}

TEST_CASE("GitParser condenses diff headers and prunes metadata", "[git]") {
    snip::GitParser parser;
    std::vector<std::string> args = {"git", "diff"};

    std::string raw =
        "diff --git a/src/main.cpp b/src/main.cpp\n"
        "index e69de29..b7e1a3b 100644\n"
        "--- a/src/main.cpp\n"
        "+++ b/src/main.cpp\n"
        "@@ -10,4 +10,6 @@ int main() {\n"
        "   int x = 1;\n"
        "+  int y = 2;\n"
        "+  return y;\n"
        " }\n";

    auto res = parser.parse(args, raw, 0);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("diff --git") == std::string::npos);
    REQUIRE(res.text.find("index e69de29") == std::string::npos);
    REQUIRE(res.text.find("file: src/main.cpp") != std::string::npos);
    REQUIRE(res.text.find("@@ int main()") != std::string::npos);
    REQUIRE(res.text.find("+  int y = 2;") != std::string::npos);
}

TEST_CASE("GitParser formats git log into single-line records", "[git]") {
    snip::GitParser parser;
    std::vector<std::string> args = {"git", "log"};

    std::string raw =
        "commit 4b825dc642cb6eb9a060e54bf8d69288fbee4904 (HEAD -> main)\n"
        "Author: Abhinav Tripathi <abhinav@example.com>\n"
        "Date:   Fri Sep 25 16:30:00 2026 +0530\n\n"
        "    feat: add BPE token counting engine\n";

    auto res = parser.parse(args, raw, 0);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("Date:") == std::string::npos);
    REQUIRE(res.text.find("4b825dc | Abhinav Tripathi | feat: add BPE token counting engine") != std::string::npos);
}